/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "piconet.h"

boolean net_client_connected;

#if PICO_ON_DEVICE
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/sync.h"
#include "pico/binary_info.h"
#if DOOM_DEBUG_INFO
#define piconet_assert(x) ({ if (!(x)) panic("DOH "__STRING(x)"\n");})
#define piconet_warning printf
#define piconet_info printf
#else
#define piconet_assert(x) ((void)0)
static inline void piconet_warning(const char *fmt, ...) {}
static inline void piconet_info(const char *fmt, ...) {}
#endif

#define PICONET_VERSION 1

// todo ditch client that causes abort (after a bit)

#define PERIODIC_ALARM_NUM 1
#define I2C_DMA_CHANNEL_READ 9
#define I2C_DMA_CHANNEL_WRITE 10
#define TO_HOST_LENGTH 32 // fixed size
#define TO_CLIENT_LENGTH 128

#define CLIENT_TIMEOUT_SHORT_US 100000 // long enough for an entire poll round
#define CLIENT_TIMEOUT_LONG_US 1000000

#include "d_loop.h"
#include "doom/doomstat.h"


typedef enum {
    piconet_msg_none,
    piconet_msg_client_lobby,
    piconet_msg_host_lobby,
    piconet_msg_game_full,
    piconet_msg_game_already_started,
    piconet_msg_client_tic,
    piconet_msg_host_tic,
    piconet_msg_game_not_compatible,
} piconet_msg_type;

// master uses this to queue stuff to send, clients use it to queue stuff from server
// note that it is a bit of an overlap with d_loop ticcmds, however combining the two seems like a fair amount of effort

// local view of the shared state
static struct
{
    enum
    {
        in_none,
        in_lobby,
        in_game
    } status;
    // this was going to be a union, but we don't have anythuing for the other states, and keeping it around always makes game start simpler
    lobby_state_t lobby;
} synced_state;

typedef struct {
    uint32_t id;
    uint32_t last_acked_by_client_seq;
    uint32_t last_rx_time;
    int last_acked_by_client_tic;
    int last_sent_by_client_tic;
    uint8_t addr;
    uint8_t abort_count; // not sure this really helps
} remote_client_t;

typedef struct
{
    enum
    {
        ps_begin,
        ps_recv_clients,
        ps_recv_poll,
        ps_send_clients,
        ps_send_poll,
        ps_end
    } protocol_state;
    // client number we are currently communicating iwth in m_recv_clients or m_send_clients
    int8_t client_num;
    // rotating poll address (looking for new clients) - used in game too so we can inform new clients what's going on
    uint8_t poll_addr;
    // client id we got from a polling client
    uint32_t poll_client_id;
    // highest tic which has data from all clients
    int last_complete_tic;
    int limit_tic; // tic we cannot receive data beyond or write over our local state
    // if non zero, error code for polling clients (they don't get to join)
    piconet_msg_type poll_error_response;
    remote_client_t clients[NET_MAXPLAYERS];
} host_state_t;

typedef struct
{
    // related to i2c transfers
    uint32_t last_tx_time;
    uint32_t last_rx_time;
    uint8_t rx_pos;
    uint8_t rx_length;
    int8_t tx_pos;
    int8_t player_num;
    int last_server_acked_tic;
    int last_local_tic;
    int last_received_from_server_tic;
    int limit_tic;
} client_state_t;

static enum {
    role_none,
    role_host,
    role_client
} role;

static union
{
    host_state_t host;
    client_state_t client;
} local_state;

typedef struct {
    piconet_msg_type msg_type;
    uint32_t client_id;
} host_packet_header_t;

typedef struct {
    uint8_t msg_type;
    uint32_t compat_hash:24;
    uint32_t client_id;
} client_packet_header_t;

// todo we need different buffer sizes depending on role, but just do max for now
static __aligned(4) uint16_t small_buffer_16[TO_HOST_LENGTH];
static __aligned(4) uint16_t large_buffer_16[TO_CLIENT_LENGTH+1];

#define small_buffer_8 ((uint8_t*)small_buffer_16)
#define large_buffer_8 ((uint8_t*)large_buffer_16)
static critical_section_t critsec;
#define I2C_IRQ __CONCAT(I2C, __CONCAT(PICO_DEFAULT_I2C, _IRQ))

#define FAST 1

#define ADDR_LOW 0x20
#if !FAST
#define piconet_debug piconet_info
#define PERIOD_MS 1000
#define ADDR_COUNT 8
#else
#define piconet_debug(fmt, ...) ((void)0)
#define PERIOD_MS 10
#define ADDR_COUNT 0x40
#endif
#define IN_GAME_CLIENT_ABORT_MAX 16

static struct {
    uint32_t client_id;
    enum
    {
        i2c_none,
        i2c_send,
        i2c_receive,
        i2c_send_dma,
        i2c_receive_dma,
    } activity;
    bool dma_active;
    uint8_t client_addr;
} i2c_state;

static void i2c_locked_cancel_dma() {
    if (i2c_state.dma_active) {
        dma_hw->abort = I2C_DMA_CHANNEL_READ | I2C_DMA_CHANNEL_WRITE;
        while (dma_hw->abort & (I2C_DMA_CHANNEL_READ | I2C_DMA_CHANNEL_WRITE)) tight_loop_contents();
        i2c_state.dma_active = false;
    }
}

static void i2c_dma_read(uint8_t *buffer, int len) {
    dma_channel_config c = dma_channel_get_default_config(I2C_DMA_CHANNEL_READ);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, i2c_get_dreq(i2c_default, false));
    i2c_state.dma_active = true;
    dma_channel_configure(I2C_DMA_CHANNEL_READ, &c,
                          buffer, &i2c_get_hw(i2c_default)->data_cmd, len, true);
}

static void i2c_dma_write(uint16_t *buffer, int len) {
    dma_channel_config c = dma_channel_get_default_config(I2C_DMA_CHANNEL_WRITE);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, i2c_get_dreq(i2c_default, true));
    i2c_state.dma_active = true;
    dma_channel_configure(I2C_DMA_CHANNEL_WRITE, &c,
                          &i2c_get_hw(i2c_default)->data_cmd, buffer, len, true);
}

static void host_advance_protocol_state_locked();
static void host_check_tic_advance_locked();
typedef struct {
    client_packet_header_t hdr;
    uint32_t last_rx_seq;
    char player_name[MAXPLAYERNAME];
} client_lobby_msg_t;
static_assert(sizeof(client_lobby_msg_t) <= TO_HOST_LENGTH, "");

typedef struct {
    client_packet_header_t hdr;
    int last_rx_tic; // last received from the server
    int enclosed_tic; // -1 for none
    ticcmd_t ticcmd;
} client_tic_msg_t;
static_assert(sizeof(client_tic_msg_t) <= TO_HOST_LENGTH, "");

typedef struct {
    host_packet_header_t hdr;
    lobby_state_t lobby_state;
} host_lobby_msg_t;
static_assert(sizeof(host_lobby_msg_t) <= count_of(large_buffer_16), "");

typedef struct {
    client_packet_header_t hdr;
    int client_ack_tic; // last received from client
    int enclosed_tic; // -1 for none
} host_tic_msg_hdr_t;

// if hdr.enclosed_tic != -1 then use this structure
typedef struct {
    host_tic_msg_hdr_t hdr;
    ticcmd_set_t cmds;
} host_tic_msg_t;

static_assert(sizeof(host_tic_msg_t) <= count_of(large_buffer_16), "");

#if DOOM_DEBUG_INFO
int foo_receive_count;
#endif

static uint32_t get_compat_hash() {
    return (PICONET_VERSION * 31 + whdheader->hash)&0xffffff;
}


void i2c_irq_handler() {
    uint32_t status = i2c_get_hw(i2c_default)->intr_stat;
    piconet_debug("STATUS %04x %04x %04x\n", status, i2c_get_hw(i2c_default)->raw_intr_stat, i2c_get_hw(i2c_default)->intr_mask);
    critical_section_enter_blocking(&critsec);
    if (status & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
        piconet_debug("ABORTED %d %08x!\n", time_us_32() - i2c_state.t0, i2c_get_hw(i2c_default)->tx_abrt_source);
        i2c_locked_cancel_dma();
        i2c_get_hw(i2c_default)->clr_tx_abrt;
        if (role == role_host) {
            if (i2c_state.activity == i2c_none) {
                // actually we can get this from our own abort
                //printf("WHAT? got abort when not doing anything\n");
            } else {
                if (local_state.host.protocol_state == ps_recv_clients) {
                    piconet_info("*** I2C ABORT RECEIVING FROM %d (%02x) ***\n", local_state.host.client_num, local_state.host.clients[local_state.host.client_num].addr);
                    if (synced_state.status == in_lobby) {
                        // move any below players up
                        for(int i=local_state.host.client_num; i < NET_MAXPLAYERS - 1; i++) {
                            synced_state.lobby.players[i] = synced_state.lobby.players[i+1];
                            local_state.host.clients[i] = local_state.host.clients[i+1];
                        }
                        // update the lobby
                        synced_state.lobby.nplayers--;
                        synced_state.lobby.players[NET_MAXPLAYERS-1].client_id = 0;
                        synced_state.lobby.seq++;
                        // and remove the client from our tracking
                        local_state.host.clients[NET_MAXPLAYERS-1].id = 0;
                        local_state.host.clients[NET_MAXPLAYERS-1].addr = 0;
                        // we don't want to skip the next up client
                        local_state.host.client_num--;
                    } else {
                        if (++local_state.host.clients[local_state.host.client_num].abort_count > IN_GAME_CLIENT_ABORT_MAX) {
                            piconet_info("Lost client %d\n", local_state.host.client_num);
                            local_state.host.clients[local_state.host.client_num].id = 0;
                            local_state.host.clients[local_state.host.client_num].addr = 0;
                            host_check_tic_advance_locked();
                        }
                    }
                }
                i2c_state.activity = i2c_none;
                host_advance_protocol_state_locked();
            }
        } else {
            piconet_debug("CLIENT ABORTED\n");
        }
        i2c_state.activity = i2c_none;
    } else if (status & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
        // we are a client and need to be sending
        piconet_assert(role == role_client);
        if (i2c_get_hw(i2c_default)->raw_intr_stat & I2C_IC_RAW_INTR_STAT_START_DET_BITS) {
            // this is the first byte of a new read
            piconet_debug("NEW RX TRANSMISSION\n");
            i2c_get_hw(i2c_default)->clr_start_det;

            i2c_state.activity = i2c_send;
            local_state.client.tx_pos = 0;
            client_packet_header_t *pkt = (client_packet_header_t *)large_buffer_8;
            pkt->client_id = i2c_state.client_id;
            pkt->compat_hash = get_compat_hash();
            if (synced_state.status == in_lobby) {
                client_lobby_msg_t *lobby_msg = (client_lobby_msg_t *)pkt;
                pkt->msg_type = piconet_msg_client_lobby;
                lobby_msg->last_rx_seq = synced_state.lobby.seq;
                memcpy(lobby_msg->player_name, player_name, MAXPLAYERNAME);
            } else if (synced_state.status == in_game) {
                client_tic_msg_t *tic_msg = (client_tic_msg_t *)pkt;
                pkt->msg_type = piconet_msg_client_tic;
                tic_msg->last_rx_tic = local_state.client.last_received_from_server_tic;
                if (local_state.client.last_server_acked_tic < local_state.client.last_local_tic) {
                    tic_msg->enclosed_tic = local_state.client.last_server_acked_tic + 1;
                    tic_msg->ticcmd = ticdata[tic_msg->enclosed_tic % BACKUPTICS].cmds[consoleplayer];
//                    printf("SEND TICK %d to server\n", tic_msg->enclosed_tic);
                } else {
                    tic_msg->enclosed_tic = -1;
                }
            } else {
                pkt->msg_type = piconet_msg_none;
            }
        }
        i2c_get_hw(i2c_default)->clr_rd_req;
        while (i2c_get_write_available(i2c_default) && local_state.client.tx_pos < TO_HOST_LENGTH) {
            i2c_get_hw(i2c_default)->data_cmd = large_buffer_8[local_state.client.tx_pos++];
        }
    } else if (status & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
        // we have incoming date (from the host)
        piconet_assert(role == role_client);
        int val = i2c_get_hw(i2c_default)->data_cmd;
        if (val & I2C_IC_DATA_CMD_FIRST_DATA_BYTE_BITS) {
            // first byte is size
            i2c_state.activity = i2c_receive;
            local_state.client.rx_pos = 0;
            local_state.client.rx_length = val & 0xff;
            piconet_debug("RX START, length = %d\n", val & 0xff);
        } else {
            if (local_state.client.rx_pos < local_state.client.rx_length) {
                large_buffer_8[local_state.client.rx_pos++] = val;
                piconet_debug("RX %d/%d %04x\n", count, length, val);
//                if (val & I2C_IC_DATA_CMD_STOP_BITS) {
//                    piconet_debug("STOP BIT\n");
//                    i2c_state.activity = i2c_none;
//                }
            } else {
                piconet_warning("RX out of band %04x\n", val);
            }
        }
    } else if (status & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
        // end of a transmission
        i2c_get_hw(i2c_default)->clr_stop_det;
        if (role == role_host) {
            int dma_count = 0;
            if (i2c_state.activity == i2c_receive_dma) {
                dma_count = dma_hw->ch[I2C_DMA_CHANNEL_READ].write_addr - (uintptr_t)large_buffer_8;
                if (dma_count == TO_HOST_LENGTH) { // (hopefully) valid message
                    client_packet_header_t *pkt = (client_packet_header_t *)large_buffer_8;
                    if (pkt->msg_type == piconet_msg_client_lobby) {
                        client_lobby_msg_t *lobby_msg = (client_lobby_msg_t *) pkt;
                        if (local_state.host.protocol_state == ps_recv_poll) {
                            // this is data from a polled client
                            local_state.host.poll_client_id = pkt->client_id;
                            if (synced_state.status == in_lobby) {
                                if (pkt->compat_hash == get_compat_hash()) {
                                    if (synced_state.lobby.nplayers < 4) {
                                        piconet_info("I2C RECORDING CLIENT AT %d %02x\n", synced_state.lobby.nplayers,
                                                     local_state.host.poll_addr);
                                        local_state.host.clients[synced_state.lobby.nplayers].id = lobby_msg->hdr.client_id;
                                        local_state.host.clients[synced_state.lobby.nplayers].addr = local_state.host.poll_addr;
                                        // update the lobby
                                        synced_state.lobby.players[synced_state.lobby.nplayers].client_id = lobby_msg->hdr.client_id;
                                        memcpy(synced_state.lobby.players[synced_state.lobby.nplayers].name,
                                               lobby_msg->player_name, MAXPLAYERNAME);
                                        synced_state.lobby.nplayers++;
                                        synced_state.lobby.seq++;
                                    } else {
                                        local_state.host.poll_error_response = piconet_msg_game_full;
                                    }
                                } else {
                                    local_state.host.poll_error_response = piconet_msg_game_not_compatible;
                                }
                            } else {
                                local_state.host.poll_error_response = piconet_msg_game_already_started;
                            }
                        } else if (local_state.host.protocol_state == ps_recv_clients) {
                            // this is data from a current client
                            if (synced_state.status == in_lobby) {
                                // record what the client last received
                                local_state.host.clients[local_state.host.client_num].last_rx_time = time_us_32();
                                local_state.host.clients[local_state.host.client_num].last_acked_by_client_seq = lobby_msg->last_rx_seq;
                            }
                        }
                    } else if (pkt->msg_type == piconet_msg_client_tic) {
                        client_tic_msg_t *tic_msg = (client_tic_msg_t *)pkt;
                        local_state.host.clients[local_state.host.client_num].abort_count = 0;
                        if (local_state.host.protocol_state != ps_recv_clients || synced_state.status != in_game) {
                            piconet_warning("unexpected client tic ps %d ss %d\n", local_state.host.protocol_state, synced_state.status);
                        } else {
//                            printf("CLIENT TIC enclosed %d ack %d\n", tic_msg->enclosed_tic, tic_msg->last_rx_tic);
                            local_state.host.clients[local_state.host.client_num].last_rx_time = time_us_32();
                            local_state.host.clients[local_state.host.client_num].last_acked_by_client_tic = tic_msg->last_rx_tic;
                            if (tic_msg->enclosed_tic != -1) {
                                //
                                if (tic_msg->enclosed_tic > local_state.host.last_complete_tic &&
                                    tic_msg->enclosed_tic < local_state.host.limit_tic) {
//                                    printf("GOT TIC DATA FOR %d from client %d\n", tic_msg->enclosed_tic, local_state.host.client_num);
//                                    printf("ticdata %d (slot %d) from %d %02x\n", tic_msg->enclosed_tic, tic_msg->enclosed_tic % BACKUPTICS, local_state.host.client_num, tic_msg->ticcmd.consistancy);
                                    ticdata[tic_msg->enclosed_tic % BACKUPTICS].cmds[local_state.host.client_num] = tic_msg->ticcmd;
                                    local_state.host.clients[local_state.host.client_num].last_sent_by_client_tic = tic_msg->enclosed_tic;
                                    host_check_tic_advance_locked();
                                } else {
                                    piconet_warning("CLIENT TIC %d out of window %d->%d\n", tic_msg->enclosed_tic,
                                                    local_state.host.last_complete_tic,
                                                    local_state.host.limit_tic);
                                }
                            }
                        }
                    } else {
                        piconet_warning("unexpected msg type %d from client\n", pkt->msg_type);
                    }
                } else {
                    piconet_warning("msg received from client is too short %d\n", dma_count);
                }
#if DOOM_DEBUG_INFO
                foo_receive_count++;
#endif
            } else if (i2c_state.activity == i2c_send_dma) {
                dma_count = (uint16_t *)dma_hw->ch[I2C_DMA_CHANNEL_WRITE].read_addr - large_buffer_16;
                piconet_debug("HOST SEND DONE %d %d\n",  time_us_32() - i2c_state.t0, dma_count);
            }
            i2c_state.activity = i2c_none;
            host_advance_protocol_state_locked();
        } else if (role == role_client) {
            if (i2c_state.activity == i2c_send) {
                piconet_debug("SEND DONE %d\n", client_tx_pos);
                local_state.client.last_tx_time = time_us_32();
            } else if (i2c_state.activity == i2c_receive) {
                piconet_debug("RECEIVE DONE %d\n", client_rx_count);
                if (local_state.client.rx_pos == local_state.client.rx_length) {
                    host_packet_header_t *hdr = (host_packet_header_t*)large_buffer_8;
                    if (i2c_state.client_id == hdr->client_id) {
                        local_state.client.last_rx_time = time_us_32();
                        if (hdr->msg_type == piconet_msg_host_lobby) {
                            if (synced_state.status == in_lobby) {
                                host_lobby_msg_t *lobby_msg = (host_lobby_msg_t *) hdr;
                                synced_state.lobby = lobby_msg->lobby_state;
                                // update our player number
                                local_state.client.player_num = -1;
                                for (int i = 0; i < NET_MAXPLAYERS; i++) {
                                    if (synced_state.lobby.players[i].client_id == i2c_state.client_id) {
                                        local_state.client.player_num = (int8_t)i;
                                    }
                                }
                            }
                        } else if (hdr->msg_type == piconet_msg_host_tic) {
                            synced_state.status = in_game; // cause auto launch
                            host_tic_msg_t *tic_msg = (host_tic_msg_t *) hdr;
                            if (tic_msg->hdr.enclosed_tic != -1) {
                                local_state.client.last_server_acked_tic = tic_msg->hdr.client_ack_tic;
                                if (tic_msg->hdr.enclosed_tic == local_state.client.last_received_from_server_tic + 1) {
                                    if (tic_msg->hdr.enclosed_tic < local_state.client.limit_tic) {
//                                        printf("  storing\n");
                                        local_state.client.last_received_from_server_tic = tic_msg->hdr.enclosed_tic;
                                        ticdata[tic_msg->hdr.enclosed_tic % BACKUPTICS] = tic_msg->cmds;
//                                        printf("ticdata %d (slot %d) %02x %02x\n", tic_msg->hdr.enclosed_tic, tic_msg->hdr.enclosed_tic % BACKUPTICS, tic_msg->cmds.cmds[0].consistancy, tic_msg->cmds.cmds[1].consistancy);
                                    } else {
                                        // probably should not happen!
                                        piconet_warning("no room for host TIC %d ack %d\n", tic_msg->hdr.enclosed_tic, tic_msg->hdr.client_ack_tic);
                                    }
                                } else {
                                    piconet_warning("  expected tic %d but got %d\n", local_state.client.last_received_from_server_tic + 1, tic_msg->hdr.enclosed_tic);
                                }
                            }
                        } else if (synced_state.status == in_lobby) {
                            if (hdr->msg_type == piconet_msg_game_already_started) {
                                synced_state.lobby.status = lobby_game_started;
                            } else if (hdr->msg_type == piconet_msg_game_not_compatible) {
                                synced_state.lobby.status = lobby_game_not_compatible;
                            }
                        }
                    }
                }
            }
            i2c_state.activity = i2c_none;
        }
    }
    critical_section_exit(&critsec);
}

static void host_send_locked(int addr, int len) {
#if DOOM_DEBUG_INFO
    if (i2c_state.activity != i2c_none) {
        piconet_info("MSEND: BADSTATE: was in %d foo %d\n", i2c_state.activity, foo_receive_count);
    }
#endif
    i2c_state.activity = i2c_send_dma;
    // todo can we release the critsec earlier?
    assert(len < count_of(large_buffer_16));
    // backwards to allow use of the same buffer
    for(int i=len-1;i>=0;i--) {
        large_buffer_16[i+1] = large_buffer_8[i];
    }
    large_buffer_16[0] = I2C_IC_DATA_CMD_RESTART_BITS | len;
    large_buffer_16[len] |= I2C_IC_DATA_CMD_STOP_BITS;

    i2c_get_hw(i2c_default)->enable = 0;
    i2c_get_hw(i2c_default)->tar = addr;
    i2c_get_hw(i2c_default)->enable = 1;
    i2c_dma_write(large_buffer_16, len+1);
}

static void host_receive_locked(int addr) {
    if (i2c_state.activity != i2c_none) {
        piconet_info("MREC: BADSTATE: was in %d\n", i2c_state.activity);
    }
    i2c_state.activity = i2c_receive_dma;
    // todo can we release the critsec earlier?
    for(int i=0;i<TO_HOST_LENGTH;i++) {
        small_buffer_16[i] = I2C_IC_DATA_CMD_CMD_BITS;
    }
    small_buffer_16[0] |= I2C_IC_DATA_CMD_RESTART_BITS;
    small_buffer_16[TO_HOST_LENGTH-1] |= I2C_IC_DATA_CMD_STOP_BITS;

    i2c_get_hw(i2c_default)->enable = 0;
    i2c_get_hw(i2c_default)->tar = addr;
    i2c_get_hw(i2c_default)->enable = 1;
    i2c_dma_read(large_buffer_8, TO_HOST_LENGTH);
    i2c_dma_write(small_buffer_16, TO_HOST_LENGTH);
}

static void host_advance_protocol_state_locked() {
    if (local_state.host.protocol_state == ps_begin) {
        local_state.host.protocol_state = ps_recv_clients;
        local_state.host.client_num = 0; // note we want to start at 1 and we pre-increment later
    }
    if (local_state.host.protocol_state == ps_recv_clients) {
        local_state.host.client_num++;
        for(; local_state.host.client_num < NET_MAXPLAYERS; local_state.host.client_num++) {
            if (local_state.host.clients[local_state.host.client_num].addr) {
                piconet_debug("TRY RECV FROM %02x\n", local_state.host.clients[local_state.host.client_num].addr);
                host_receive_locked(local_state.host.clients[local_state.host.client_num].addr);
                return;
            }
        }
        local_state.host.protocol_state = ps_recv_poll;
        local_state.host.poll_error_response = piconet_msg_none;
        piconet_debug("POLL %02x\n", host_protocol_state.poll_addr);
        host_receive_locked(local_state.host.poll_addr);
        return;
    }
    if (local_state.host.protocol_state == ps_recv_poll) {
        local_state.host.protocol_state = ps_send_clients;
        local_state.host.client_num = 0; // note we want to start at 1 and we pre-increment later
    }
    if (local_state.host.protocol_state == ps_send_clients) {
        local_state.host.client_num++;
        for(; local_state.host.client_num < NET_MAXPLAYERS; local_state.host.client_num++) {
            if (local_state.host.clients[local_state.host.client_num].id) {
                piconet_debug("SEND TO %02x\n", local_state.host.clients[local_state.host.client_num].addr);
                host_packet_header_t *hdr = (host_packet_header_t *)large_buffer_8;
                hdr->client_id = local_state.host.clients[local_state.host.client_num].id;
                int len = 0;
                if (synced_state.status == in_lobby || local_state.host.clients[local_state.host.client_num].last_sent_by_client_tic == -1) {
                    // note we send lobby message until the client acknowledges the game start (by sending a tic)
                    if (local_state.host.clients[local_state.host.client_num].last_acked_by_client_seq != synced_state.lobby.seq) {
                        hdr->msg_type = piconet_msg_host_lobby;
                        ((host_lobby_msg_t *) hdr)->lobby_state = synced_state.lobby;
//                        printf("SENDING UPDATED LOBBY (%d) TO CLIENT %d\n", (int) synced_state.lobby.seq,
//                               local_state.host.client_num);
                        len = sizeof(host_lobby_msg_t);
                    } else {
                        // just send dummy message
                    }
                } else if (synced_state.status == in_game) {
                    hdr->msg_type = piconet_msg_host_tic;
                    host_tic_msg_hdr_t *tichdr = (host_tic_msg_hdr_t *)hdr;
                    tichdr->client_ack_tic = local_state.host.clients[local_state.host.client_num].last_sent_by_client_tic;
                    if (local_state.host.clients[local_state.host.client_num].last_acked_by_client_tic < local_state.host.last_complete_tic) {
                        tichdr->enclosed_tic = local_state.host.clients[local_state.host.client_num].last_acked_by_client_tic + 1;
                        ((host_tic_msg_t *)tichdr)->cmds = ticdata[tichdr->enclosed_tic % BACKUPTICS];
//                        printf("SENDING TIC %d to client %d\n", tichdr->enclosed_tic, local_state.host.client_num);
                        len = sizeof(host_tic_msg_t);
                    } else {
//                        printf("NOT SENDING TIC to client %d acked = %d complete = %d\n", local_state.host.client_num, local_state.host.clients[local_state.host.client_num].last_acked_by_client_tic, local_state.host.last_complete_tic);
                        tichdr->enclosed_tic = -1;
                        len = sizeof(host_tic_msg_hdr_t);
                    }
                }
                if (!len) {
                    hdr->msg_type = piconet_msg_none;
                    len = sizeof(host_packet_header_t);
                }
                host_send_locked(local_state.host.clients[local_state.host.client_num].addr, len);
                return;
            }
        }
        local_state.host.protocol_state = ps_send_poll;
        if (local_state.host.poll_error_response) {
            piconet_debug("POLL RESPONSE %02x\n", local_state.host.poll_addr);
            host_packet_header_t *hdr = (host_packet_header_t *)large_buffer_8;
            hdr->msg_type = local_state.host.poll_error_response;
            hdr->client_id = local_state.host.poll_client_id;
            host_send_locked(local_state.host.poll_addr, sizeof(host_packet_header_t));
            local_state.host.poll_error_response = 0;
            return;
        }
    }
    if (local_state.host.protocol_state == ps_send_poll) {
        local_state.host.protocol_state = ps_end;
    }
}

static void periodic_tick(uint timer) {
//    printf("TICK\n");
    critical_section_enter_blocking(&critsec);
    if (i2c_state.activity != i2c_none) {
        piconet_warning("OOPS BAD STATE %d %08x\n", i2c_state.activity, (int)i2c_get_hw(i2c_default)->raw_intr_stat);
        i2c_state.activity = i2c_none;
        hw_set_bits(&i2c_get_hw(i2c_default)->enable, I2C_IC_ENABLE_ABORT_BITS);
    }
    do {
        local_state.host.poll_addr++;
        if (local_state.host.poll_addr == ADDR_LOW + ADDR_COUNT) local_state.host.poll_addr = ADDR_LOW;
        bool found = false;
        for(int i=0;i<NET_MAXPLAYERS;i++) {
            if (local_state.host.clients[i].addr == local_state.host.poll_addr) found = true;
        }
        if (!found) break;
    } while (true);
    local_state.host.protocol_state = ps_begin;
    host_advance_protocol_state_locked();
    critical_section_exit(&critsec);
    bool missed = hardware_alarm_set_target(PERIODIC_ALARM_NUM, make_timeout_time_ms(PERIOD_MS));
//    printf("SET TIMER %d %d\n", missed, timer_hw->ints & 1u << PERIODIC_ALARM_NUM);
}


void piconet_init() {
    bi_decl_if_func_used(bi_program_feature("I2C multi-player"));
    bi_decl_if_func_used(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    critical_section_init(&critsec);
    i2c_init(i2c_default, 800 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    irq_set_exclusive_handler(I2C_IRQ, i2c_irq_handler);
    hardware_alarm_set_callback(PERIODIC_ALARM_NUM, periodic_tick);
    irq_set_priority(TIMER_IRQ_0 + PERIODIC_ALARM_NUM, 0xc0); // don't want timer pre-empting the other IRQs
    irq_set_enabled(TIMER_IRQ_0 + PERIODIC_ALARM_NUM, true); // no harm turning it on
}

static void clear_state() {
    memset(&i2c_state, 0, sizeof(i2c_state));
    memset(&local_state, 0, sizeof(local_state));
    memset(&synced_state, 0, sizeof(synced_state));
}

void piconet_start_host(int8_t deathmatch, int8_t epi, int8_t skill) {
    critical_section_enter_blocking(&critsec);
    hardware_alarm_cancel(PERIODIC_ALARM_NUM);
#if PICO_DOOM_INFO
    printf("START HOST\n");
#endif
    clear_state();
    role = role_host;
    // host is client 0
    memcpy(synced_state.lobby.players[0].name, player_name, MAXPLAYERNAME);
    uint32_t client_id = time_us_32();
    synced_state.status = in_lobby;
    synced_state.lobby.players[0].client_id = client_id;
    synced_state.lobby.status = lobby_waiting_for_start;
    synced_state.lobby.nplayers = 1;
    synced_state.lobby.deathmatch = deathmatch;
    synced_state.lobby.epi = epi;
    synced_state.lobby.skill = skill;
    synced_state.lobby.seq = 1;

    local_state.host.poll_addr = ADDR_LOW;
    local_state.host.clients[0].id = client_id;
    local_state.host.last_complete_tic = -1;
    for(int i=0;i<NET_MAXPLAYERS;i++) {
        local_state.host.clients[i].last_sent_by_client_tic = -1;
        local_state.host.clients[i].last_acked_by_client_tic = -1;
    }

    i2c_get_hw(i2c_default)->enable = 0;
    i2c_set_slave_mode(i2c_default, false, 0);
    i2c_get_hw(i2c_default)->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS | I2C_IC_INTR_STAT_R_TX_ABRT_BITS;
    i2c_get_hw(i2c_default)->enable = 1;
    critical_section_exit(&critsec);

    hardware_alarm_set_target(PERIODIC_ALARM_NUM, make_timeout_time_ms(PERIOD_MS));
    irq_set_enabled(I2C_IRQ, true);
}

static void client_new_random_addr_locked() {
    i2c_state.client_addr = ADDR_LOW + time_us_32()%ADDR_COUNT;
    i2c_state.client_id = time_us_32();
    piconet_info("CLIENT ADDR %02x\n", i2c_state.client_addr);
}

void piconet_start_client() {
    critical_section_enter_blocking(&critsec);
    hardware_alarm_cancel(PERIODIC_ALARM_NUM);
    clear_state();
    synced_state.status = in_lobby;
    role = role_client;
    local_state.client.last_local_tic = local_state.client.last_server_acked_tic = local_state.client.last_received_from_server_tic = -1;
    client_new_random_addr_locked();
    i2c_set_slave_mode(i2c_default, true, i2c_state.client_addr);
    i2c_get_hw(i2c_default)->enable = 0;
    hw_set_bits(&i2c_get_hw(i2c_default)->con, I2C_IC_CON_STOP_DET_IFADDRESSED_BITS);
    i2c_get_hw(i2c_default)->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS | I2C_IC_INTR_STAT_R_RD_REQ_BITS | I2C_IC_INTR_STAT_R_TX_ABRT_BITS | I2C_IC_INTR_STAT_R_RX_FULL_BITS;
    i2c_get_hw(i2c_default)->enable = 1;
    critical_section_exit(&critsec);
    irq_set_enabled(I2C_IRQ, true);
}

bool piconet_client_check_for_dropped_connection() {
    bool rc = false;
    critical_section_enter_blocking(&critsec);
    if (role == role_client && i2c_state.activity == i2c_none) {
        // use long timeout if we aren't accepted as the server will only be talking to us once per round of polling
        if (time_us_32() - local_state.client.last_tx_time > (synced_state.lobby.status == lobby_waiting_for_start ? CLIENT_TIMEOUT_SHORT_US : CLIENT_TIMEOUT_LONG_US)) {
            client_new_random_addr_locked();
            if (synced_state.status == in_lobby) {
                synced_state.lobby.status = lobby_no_connection;
                synced_state.lobby.seq = 0;
            }
            // todo leave game
            i2c_get_hw(i2c_default)->enable = false;
            i2c_get_hw(i2c_default)->sar = i2c_state.client_addr;
            i2c_get_hw(i2c_default)->enable = true;
            rc = true;
        }
    }
    critical_section_exit(&critsec);
    return rc;
}

void piconet_stop() {
    critical_section_enter_blocking(&critsec);
    if (role != role_none) {
        piconet_info("I2C STOP\n");
        i2c_locked_cancel_dma();
        hardware_alarm_cancel(PERIODIC_ALARM_NUM);
        irq_set_enabled(I2C_IRQ, false);
        i2c_get_hw(i2c_default)->enable = 0;
        role = role_none;
    }
    critical_section_exit(&critsec);
}

int piconet_get_lobby_state(lobby_state_t *state) {
    critical_section_enter_blocking(&critsec);
    int rc;
    if (role == role_client) {
        rc = local_state.client.player_num;
    } else if (role == role_host) {
        rc = 0;
    } else {
        rc = -1;
    }
    memcpy(state, &synced_state.lobby, sizeof(lobby_state_t));
    critical_section_exit(&critsec);
    return rc;
}

void piconet_start_game() {
    critical_section_enter_blocking(&critsec);
    piconet_assert(synced_state.status == in_lobby);
    synced_state.status = in_game;
    synced_state.lobby.status = lobby_game_started;
    if (role == role_host) {
        synced_state.lobby.seq++;
    }
    critical_section_exit(&critsec);
}

void piconet_new_local_tic(int tic) {
    critical_section_enter_blocking(&critsec);
    piconet_assert(synced_state.status == in_game);
//    printf("NEWTIC %d\n", tic);
    if (role == role_client) {
        piconet_assert(tic == local_state.client.last_local_tic + 1);
        local_state.client.last_local_tic = tic;
    } else {
        local_state.host.clients[0].last_sent_by_client_tic = tic;
        host_check_tic_advance_locked();
    }
    critical_section_exit(&critsec);
}

int piconet_maybe_recv_tic(int fromtic) {
    critical_section_enter_blocking(&critsec);
    if (role == role_host) {
        if (fromtic < local_state.host.last_complete_tic) {
            fromtic++;
#if DEBUG_CONSISTENCY
            printf("Accept tic %d %d,%d,%d:%02x %d,%d,%d:%02x limit was %d\n", fromtic,
                   ticdata[fromtic%BACKUPTICS].cmds[0].angleturn, ticdata[fromtic%BACKUPTICS].cmds[0].forwardmove, ticdata[fromtic%BACKUPTICS].cmds[0].buttons, ticdata[fromtic%BACKUPTICS].cmds[0].consistancy,
                   ticdata[fromtic%BACKUPTICS].cmds[1].angleturn, ticdata[fromtic%BACKUPTICS].cmds[1].forwardmove, ticdata[fromtic%BACKUPTICS].cmds[1].buttons, ticdata[fromtic%BACKUPTICS].cmds[1].consistancy,
                   local_state.host.limit_tic);
#endif
        } else {
            uint32_t now = time_us_32();
            for(int i=1;i<NET_MAXPLAYERS;i++) {
                if (local_state.host.clients[i].id && now - local_state.host.clients[i].last_rx_time > CLIENT_TIMEOUT_SHORT_US) {
                    piconet_info("KICKING CLIENT %d out\n", i);
                    local_state.host.clients[i].id = local_state.host.clients[i].addr = 0;
                    host_check_tic_advance_locked();
                }
            }
        }
        local_state.host.limit_tic = fromtic + BACKUPTICS - 1;
    } else if (role == role_client) {
        if (fromtic < local_state.client.last_received_from_server_tic) {
            fromtic++;
#if DEBUG_CONSISTENCY
            printf("Accept tic %d %d,%d,%d:%02x %d,%d,%d:%02x limit was %d\n", fromtic,
                   ticdata[fromtic%BACKUPTICS].cmds[0].angleturn, ticdata[fromtic%BACKUPTICS].cmds[0].forwardmove, ticdata[fromtic%BACKUPTICS].cmds[0].buttons, ticdata[fromtic%BACKUPTICS].cmds[0].consistancy,
                   ticdata[fromtic%BACKUPTICS].cmds[1].angleturn, ticdata[fromtic%BACKUPTICS].cmds[1].forwardmove, ticdata[fromtic%BACKUPTICS].cmds[1].buttons, ticdata[fromtic%BACKUPTICS].cmds[1].consistancy,
                   local_state.client.limit_tic);
#endif
        } else if (time_us_32() - local_state.client.last_rx_time > CLIENT_TIMEOUT_SHORT_US) {
            net_client_connected = false;
            for(int i=0;i<NET_MAXPLAYERS;i++) {
                if (i != consoleplayer) {
                    for(int j=0;j<BACKUPTICS;j++) {
                        ticdata[j].cmds[i].ingame = false;
                    }
                }
            }
            net_client_connected = false;
        }
        local_state.client.limit_tic = fromtic + BACKUPTICS - 1;
    }
    critical_section_exit(&critsec);
    return fromtic;
}

static void host_check_tic_advance_locked() {
    // we have received a tic
    bool advance;
    do {
        advance = true;
        for(int i=0;i<NET_MAXPLAYERS;i++) {
            if (local_state.host.clients[i].id) {
                if (local_state.host.clients[i].last_sent_by_client_tic <= local_state.host.last_complete_tic) {
                    advance = false;
                    break;
                }
            }
        }
        if (advance) {
            local_state.host.last_complete_tic++;
//            printf("ADVANCE TO %d\n", local_state.host.last_complete_tic);
            for(int i=0;i<NET_MAXPLAYERS;i++) {
                ticdata[local_state.host.last_complete_tic % BACKUPTICS].cmds[i].ingame = local_state.host.clients[i].id != 0;
            }
        }
    } while (advance);
}
#else
static enum {
    none,
    client,
    host,
    host_game,
} state;
static int host_tic;

void piconet_init() {}
void piconet_start_host(int8_t deathmatch, int8_t epi, int8_t skill) {
    state = host;
}
void piconet_start_client() {
    state = client;
}
void piconet_stop() {
    state = none;
}
// periodically poll to check connection hasn't dropped
bool piconet_client_check_for_dropped_connection() {
    return true;
}

void piconet_start_game() {
    if (state == host) {
        state = host_game;
    }
}

void piconet_new_local_tic(int tic) {
    host_tic = tic;
}

int piconet_maybe_recv_tic(int fromtic) {
    if (host_tic > fromtic) fromtic++;
    return fromtic;
}

// returns which player you are
int piconet_get_lobby_state(lobby_state_t *ls) {
    memset(ls, 0, sizeof(lobby_state_t));
    if (state == host || state == host_game) {
        ls->status = state == host ? lobby_waiting_for_start : lobby_game_started;
        ls->players[0].client_id = 1;
        ls->nplayers = 1;
        memcpy(ls->players[0].name, player_name, MAXPLAYERNAME);
        return 0;
    }
    ls->status = lobby_no_connection;
    return -1;
}

#endif