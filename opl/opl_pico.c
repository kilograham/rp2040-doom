//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     OPL SDL interface.
//

// todo replace opl_queue with pheap
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "pico/mutex.h"
#include "pico/audio_i2s.h"
#include "pico/util/pheap.h"
#include "hardware/gpio.h"
#include "i_picosound.h"

#if USE_WOODY_OPL
#include "woody_opl.h"
#elif USE_EMU8950_OPL
#include "emu8950.h"
#else
#include "opl3.h"
#endif

#include "opl.h"
#include "opl_internal.h"

#include "opl_queue.h"

#define MAX_SOUND_SLICE_TIME 100 /* ms */

typedef struct
{
    unsigned int rate;        // Number of times the timer is advanced per sec.
    unsigned int enabled;     // Non-zero if timer is enabled.
    unsigned int value;       // Last value that was set.
    uint64_t expire_time;     // Calculated time that timer will expire.
} opl_timer_t;

// When the callback mutex is locked using OPL_Lock, callback functions
// are not invoked.

static mutex_t callback_mutex;

// Queue of callbacks waiting to be invoked.

static opl_callback_queue_t *callback_queue;

// Mutex used to control access to the callback queue.

static mutex_t callback_queue_mutex;

// Current time, in us since startup:

static uint64_t current_time;

// If non-zero, playback is currently paused.

static int opl_pico_paused;

// Time offset (in us) due to the fact that callbacks
// were previously paused.

static uint64_t pause_offset;

// OPL software emulator structure.

#if USE_WOODY_OPL
// todo configure this based on woody build flag
#define opl_op3mode 0
#elif USE_EMU8950_OPL
#define opl_op3mode 0
static OPL *emu8950_opl;
#else
static opl3_chip opl_chip;
static int opl_opl3mode;
#endif

// Register number that was written.

static int register_num = 0;

#if !EMU8950_NO_TIMER
// Timers; DBOPL does not do timer stuff itself.

static opl_timer_t timer1 = { 12500, 0, 0, 0 };
static opl_timer_t timer2 = { 3125, 0, 0, 0 };
#endif

// SDL parameters.

static bool audio_was_initialized = 0;

static inline void Pico_LockMutex(mutex_t *mutex) {
    
}

static inline void Pico_UnlockMutex(mutex_t *mutex) {

}

// Advance time by the specified number of samples, invoking any
// callback functions as appropriate.

static void AdvanceTime(unsigned int nsamples)
{
    opl_callback_t callback;
    void *callback_data;
    uint64_t us;

    Pico_LockMutex(&callback_queue_mutex);

    // Advance time.

    us = ((uint64_t) nsamples * OPL_SECOND) / PICO_SOUND_SAMPLE_FREQ;
    current_time += us;

    if (opl_pico_paused)
    {
        pause_offset += us;
    }

    // Are there callbacks to invoke now?  Keep invoking them
    // until there are no more left.

    while (!OPL_Queue_IsEmpty(callback_queue)
        && current_time >= OPL_Queue_Peek(callback_queue) + pause_offset)
    {
        // Pop the callback from the queue to invoke it.

        if (!OPL_Queue_Pop(callback_queue, &callback, &callback_data))
        {
            break;
        }

        // The mutex stuff here is a bit complicated.  We must
        // hold callback_mutex when we invoke the callback (so that
        // the control thread can use OPL_Lock() to prevent callbacks
        // from being invoked), but we must not be holding
        // callback_queue_mutex, as the callback must be able to
        // call OPL_SetCallback to schedule new callbacks.

        Pico_UnlockMutex(&callback_queue_mutex);
        
        Pico_LockMutex(&callback_mutex);
        callback(callback_data);
        Pico_UnlockMutex(&callback_mutex);

        Pico_LockMutex(&callback_queue_mutex);
    }

    Pico_UnlockMutex(&callback_queue_mutex);
}

// Call the OPL emulator code to fill the specified buffer.

// Callback function to fill a new sound buffer:

#define LIMITED_CALLBACK_TYPES 1

#if LIMITED_CALLBACK_TYPES
extern void RestartSong(void *unused);
extern void TrackTimerCallback(void *track);
#endif

#if DOOM_TINY
extern uint8_t restart_song_state;
#endif

void OPL_Pico_Mix_callback(audio_buffer_t *audio_buffer)
{
    unsigned int filled, buffer_samples;
#if DOOM_TINY
    if (restart_song_state == 2) {
        RestartSong(0);
    }
#endif

        // Repeatedly call the OPL emulator update function until the buffer is
        // full.
        filled = 0;
        buffer_samples = audio_buffer->max_sample_count;

//#if PICO_ON_DEVICE
//        absolute_time_t t0 = get_absolute_time();
//        gpio_set_mask(1);
//#endif
        while (filled < buffer_samples) {
//#if PICO_ON_DEVICE
//            gpio_set_mask(32);
//#endif
            uint64_t next_callback_time;
            uint64_t nsamples;

            Pico_LockMutex(&callback_queue_mutex);

            // Work out the time until the next callback waiting in
            // the callback queue must be invoked.  We can then fill the
            // buffer with this many samples.

            if (opl_pico_paused || OPL_Queue_IsEmpty(callback_queue)) {
                nsamples = buffer_samples - filled;
            } else {
                next_callback_time = OPL_Queue_Peek(callback_queue) + pause_offset;

                nsamples = (next_callback_time - current_time) * PICO_SOUND_SAMPLE_FREQ;
                nsamples = (nsamples + OPL_SECOND - 1) / OPL_SECOND;

                if (nsamples > buffer_samples - filled) {
                    nsamples = buffer_samples - filled;
                }
            }

            Pico_UnlockMutex(&callback_queue_mutex);

            // Add emulator output to buffer.

            //OPL3_GenerateStream(&opl_chip, (Bit16s *) (audio_buffer->buffer->bytes + filled * 4), nsamples);
#if USE_WOODY_OPL
            int16_t *sndptr = (int16_t *) (audio_buffer->buffer->bytes + filled * 4);
            // todo store in stereo?
            adlib_getsample(sndptr, nsamples);
            for(int i=nsamples-1; i>=0; i--) {
                sndptr[i*2] = sndptr[i*2 + 1] = sndptr[i];
            }
#elif USE_EMU8950_OPL
            if (nsamples) {
                int32_t *sndptr32 = (int32_t *) (audio_buffer->buffer->bytes + filled * 4);
                OPL_calc_buffer_stereo(emu8950_opl, sndptr32, nsamples);
            }
#else
            int16_t *sndptr = (int16_t *) (audio_buffer->buffer->bytes + filled * 4);
            for(int i = 0; i < nsamples; i++)
            {
                OPL3_GenerateResampled(&opl_chip, sndptr);
                sndptr += 2;
            }
#endif
            filled += nsamples;

            // Invoke callbacks for this point in time.

//#if PICO_ON_DEVICE
//            gpio_clr_mask(32);
//#endif
            AdvanceTime(nsamples);
        }
        audio_buffer->sample_count = audio_buffer->max_sample_count;
#if !USE_WOODY_OPL
        int16_t *samples = (int16_t *)audio_buffer->buffer->bytes;
        for(uint i=0;i<audio_buffer->sample_count * 2; i++) {
            samples[i] <<= 3;
        }
#endif
//#if PICO_ON_DEVICE
//        gpio_clr_mask(1);
//        int32_t t = (int32_t)absolute_time_diff_us(t0, get_absolute_time());
//        static int max_t;
//        static int ii;
//        static int total;
//        total += t;
//        if (t > max_t) {
//            max_t = t;
//        }
//        ii++;
//        if (!(ii &127)) {
//            printf("AVG %d MAX %d\n", total / 128, max_t);
//            max_t = 0;
//            total = 0;
//        }
//#endif
}

static void OPL_Pico_Shutdown(void)
{
    if (audio_was_initialized)
    {
        I_PicoSoundSetMusicGenerator(NULL);
        OPL_Queue_Destroy(callback_queue);
        audio_was_initialized = 0;
    }
}

static int OPL_Pico_Init(unsigned int port_base)
{
    if (I_PicoSoundIsInitialized()) {
        opl_pico_paused = 0;
        pause_offset = 0;

        // Queue structure of callbacks to invoke.

        callback_queue = OPL_Queue_Create();
        current_time = 0;


#if USE_WOODY_OPL
        adlib_init(mixing_freq);
#elif USE_EMU8950_OPL
        emu8950_opl = OPL_new(3579552, PICO_SOUND_SAMPLE_FREQ); // todo check rate
#else
        OPL3_Reset(&opl_chip, PICO_SOUND_SAMPLE_FREQ);
        opl_opl3mode = 0;
#endif

        //    // Set postmix that adds the OPL music. This is deliberately done
        //    // as a postmix and not using Mix_HookMusic() as the latter disables
        //    // normal Pico_mixer music mixing.
        //    Mix_SetPostMix(OPL_Mix_Callback, NULL);
        I_PicoSoundSetMusicGenerator(OPL_Pico_Mix_callback);
        audio_was_initialized = 1;
    } else {
        audio_was_initialized = 0;
    }
    return 1;
}

static unsigned int OPL_Pico_PortRead(opl_port_t port)
{
    unsigned int result = 0;

    if (port == OPL_REGISTER_PORT_OPL3)
    {
        return 0xff;
    }

#if !EMU8950_NO_TIMER
    if (timer1.enabled && current_time > timer1.expire_time)
    {
        result |= 0x80;   // Either have expired
        result |= 0x40;   // Timer 1 has expired
    }

    if (timer2.enabled && current_time > timer2.expire_time)
    {
        result |= 0x80;   // Either have expired
        result |= 0x20;   // Timer 2 has expired
    }
#endif

    return result;
}

static void OPLTimer_CalculateEndTime(opl_timer_t *timer)
{
    int tics;

    // If the timer is enabled, calculate the time when the timer
    // will expire.

    if (timer->enabled)
    {
        tics = 0x100 - timer->value;
        timer->expire_time = current_time
                           + ((uint64_t) tics * OPL_SECOND) / timer->rate;
    }
}

static void WriteRegister(unsigned int reg_num, unsigned int value)
{
    switch (reg_num)
    {
#if !EMU8950_NO_TIMER
        case OPL_REG_TIMER1:
            timer1.value = value;
            OPLTimer_CalculateEndTime(&timer1);
            break;

        case OPL_REG_TIMER2:
            timer2.value = value;
            OPLTimer_CalculateEndTime(&timer2);
            break;

        case OPL_REG_TIMER_CTRL:
            if (value & 0x80)
            {
                timer1.enabled = 0;
                timer2.enabled = 0;
            }
            else
            {
                if ((value & 0x40) == 0)
                {
                    timer1.enabled = (value & 0x01) != 0;
                    OPLTimer_CalculateEndTime(&timer1);
                }

                if ((value & 0x20) == 0)
                {
                    timer1.enabled = (value & 0x02) != 0;
                    OPLTimer_CalculateEndTime(&timer2);
                }
            }

            break;
#endif
        case OPL_REG_NEW:
#if !USE_WOODY_OPL && !USE_EMU8950_OPL
            opl_opl3mode = value & 0x01;
#endif
        default:
#if USE_WOODY_OPL
            adlib_write(reg_num, value);
#elif USE_EMU8950_OPL
            OPL_writeReg(emu8950_opl, reg_num, value);
#else
            OPL3_WriteRegBuffered(&opl_chip, reg_num, value);
#endif
            break;
    }
}

static void OPL_Pico_PortWrite(opl_port_t port, unsigned int value)
{
    if (port == OPL_REGISTER_PORT)
    {
        register_num = value;
    }
    else if (port == OPL_REGISTER_PORT_OPL3)
    {
        register_num = value | 0x100;
    }
    else if (port == OPL_DATA_PORT)
    {
        WriteRegister(register_num, value);
    }
}

static void OPL_Pico_SetCallback(uint64_t us, opl_callback_t callback,
                                void *data)
{
    Pico_LockMutex(&callback_queue_mutex);
    OPL_Queue_Push(callback_queue, callback, data,
                   current_time - pause_offset + us);
    Pico_UnlockMutex(&callback_queue_mutex);
}

static void OPL_Pico_ClearCallbacks(void)
{
    Pico_LockMutex(&callback_queue_mutex);
    OPL_Queue_Clear(callback_queue);
    Pico_UnlockMutex(&callback_queue_mutex);
}

static void OPL_Pico_Lock(void)
{
    Pico_LockMutex(&callback_mutex);
}

static void OPL_Pico_Unlock(void)
{
    Pico_UnlockMutex(&callback_mutex);
}

static void OPL_Pico_SetPaused(int paused)
{
    opl_pico_paused = paused;
}

static void OPL_Pico_AdjustCallbacks(unsigned int old_tempo, unsigned int new_tempo)
{
    Pico_LockMutex(&callback_queue_mutex);
    OPL_Queue_AdjustCallbacks(callback_queue, current_time, old_tempo, new_tempo);
    Pico_UnlockMutex(&callback_queue_mutex);
}

const opl_driver_t opl_pico_driver =
{
    "Pico",
    OPL_Pico_Init,
    OPL_Pico_Shutdown,
    OPL_Pico_PortRead,
    OPL_Pico_PortWrite,
    OPL_Pico_SetCallback,
    OPL_Pico_ClearCallbacks,
    OPL_Pico_Lock,
    OPL_Pico_Unlock,
    OPL_Pico_SetPaused,
    OPL_Pico_AdjustCallbacks,
};

void OPL_Delay(uint64_t us) {
    sleep_us(us); // todo not sure we want to block
}

// todo really limited to 1 event per track i think, so could go smaller
#define MAX_OPL_QUEUE 10
PHEAP_DEFINE_STATIC(opl_heap, MAX_OPL_QUEUE + 1);

// todo note also the callback is likely only one of a couple of functions
typedef struct queue_entry {
    uint64_t time; // todo graham can likely be 32 bit, too much work atm
#if !LIMITED_CALLBACK_TYPES
    opl_callback_t callback;
#endif
    void *data;
} queue_entry_t;

struct opl_callback_queue_s {
    queue_entry_t entries[MAX_OPL_QUEUE];
};

static struct opl_callback_queue_s queue;

static inline queue_entry_t *get_entry(opl_callback_queue_t *queue, pheap_node_id_t id) {
    assert(id && id <= opl_heap.max_nodes);
    return queue->entries + id - 1;
}

bool opl_queue_comparator(void *user_data, pheap_node_id_t a, pheap_node_id_t b) {
    opl_callback_queue_t *q = (opl_callback_queue_t *)user_data;
    return get_entry(q, a)->time < get_entry(q, b)->time;
}

opl_callback_queue_t *OPL_Queue_Create(void) {
    ph_post_alloc_init(&opl_heap, MAX_OPL_QUEUE, opl_queue_comparator, &queue);
    return &queue;
}

int OPL_Queue_IsEmpty(opl_callback_queue_t *queue) {
    return ph_peek_head(&opl_heap) == 0;
}

void OPL_Queue_Clear(opl_callback_queue_t *queue) {
    ph_clear(&opl_heap);
}

void OPL_Queue_Destroy(opl_callback_queue_t *queue) {

}

void OPL_Queue_Push(opl_callback_queue_t *queue,
                    opl_callback_t callback, void *data,
                    uint64_t time) {
    pheap_node_id_t id = ph_new_node(&opl_heap);
    assert(id); // check not full
    queue_entry_t *qe = get_entry(queue, id);
    qe->time = time;
#if !LIMITED_CALLBACK_TYPES
    qe->callback = callback
#else
    assert(data || callback == RestartSong);
    assert(!data || callback == TrackTimerCallback);
#endif
    qe->data = data;
    ph_insert_node(&opl_heap, id);
}

int OPL_Queue_Pop(opl_callback_queue_t *queue,
                  opl_callback_t *callback, void **data) {
    if (!ph_peek_head(&opl_heap)) return 0;
    pheap_node_id_t id = ph_remove_head(&opl_heap, true);
    queue_entry_t *qe = get_entry(queue, id);
#if !LIMITED_CALLBACK_TYPES
    *callback = qe->callback;
#else
    *callback = qe->data ? TrackTimerCallback : RestartSong;
#endif
    *data = qe->data;
    return 1;
}

uint64_t OPL_Queue_Peek(opl_callback_queue_t *queue) {
    pheap_node_id_t head = ph_peek_head(&opl_heap);
    if (head) {
        return get_entry(queue, head)->time;
    } else {
        return 0;
    }
}

static uint AdjustCallbacks(pheap_node_id_t id, uint64_t time, unsigned int old_tempo, unsigned int new_tempo) {
    uint count = 0;
    if (id) {
        pheap_node_t *node = ph_get_node(&opl_heap, id);
        queue_entry_t *entry = get_entry(&queue, id);
        uint64_t offset = entry->time - time;
        entry->time = time + (offset * new_tempo) / old_tempo;
        AdjustCallbacks(node->child, time, old_tempo, new_tempo);
        AdjustCallbacks(node->sibling, time, old_tempo, new_tempo);
    }
    return count;
}

void OPL_Queue_AdjustCallbacks(opl_callback_queue_t *_queue,
                               uint64_t time, unsigned int old_tempo, unsigned int new_tempo)
{
    assert(_queue == &queue);
    AdjustCallbacks(ph_peek_head(&opl_heap), time, old_tempo, new_tempo);
}
