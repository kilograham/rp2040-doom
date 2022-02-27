/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "pico.h"
#include "net_defs.h"

typedef struct {
    uint32_t client_id;
    char name[MAXPLAYERNAME];
} lobby_player_t;

typedef enum {
    lobby_no_connection,
    lobby_waiting_for_start,
    lobby_game_started,
    lobby_game_not_compatible,
} piconet_lobby_status_t;

typedef struct {
    uint32_t compat_hash; // version and whd hash
    uint32_t seq;
    piconet_lobby_status_t status;
    uint8_t nplayers;
    int8_t deathmatch;
    int8_t epi;
    int8_t skill;
    lobby_player_t players[NET_MAXPLAYERS];
} lobby_state_t;

// one time initialization (set pulls etc)
void piconet_init();
void piconet_start_host(int8_t deathmatch, int8_t epi, int8_t skill);
void piconet_start_client();
void piconet_stop();
// periodically poll to check connection hasn't dropped
bool piconet_client_check_for_dropped_connection();
void piconet_start_game();
// returns which player you are
int piconet_get_lobby_state(lobby_state_t *state);
void piconet_new_local_tic(int tic);
int piconet_maybe_recv_tic(int fromtic);

extern char player_name[MAXPLAYERNAME];

#if USE_PICO_NET
// same var as used by regular networking
extern boolean net_client_connected; // basically whether events are sync-ing amongst the players
#endif