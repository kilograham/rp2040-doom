//
// Copyright(C) 1993-1996 Id Software, Inc.
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
//

#include "pico.h"
#include "pico/stdio.h"
#if PICO_ON_DEVICE
#include "hardware/watchdog.h"
#include "doomtype.h"
#include "doom/p_saveg.h"
#endif
#if USB_SUPPORT
#include "tusb.h"
#endif
extern void I_InputInit();

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>

#include <unistd.h>
#include <pico/mutex.h>

#include "config.h"

#include "deh_str.h"
#include "doomtype.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "i_joystick.h"
#include "i_sound.h"
#include "i_timer.h"
#include "i_video.h"

#include "i_system.h"
#include "i_input.h"
#include "doomkeys.h"

#include "w_wad.h"
#include "z_zone.h"
#include "picodoom.h"

#define DEFAULT_RAM 16 /* MiB */
#define MIN_RAM     4  /* MiB */


typedef struct atexit_listentry_s atexit_listentry_t;

struct atexit_listentry_s
{
    atexit_func_t func;
    boolean run_on_error;
    atexit_listentry_t *next;
};

static atexit_listentry_t *exit_funcs = NULL;

void I_AtExit(atexit_func_t func, boolean run_on_error)
{
    atexit_listentry_t *entry;

    entry = malloc(sizeof(*entry));

    entry->func = func;
    entry->run_on_error = run_on_error;
    entry->next = exit_funcs;
    exit_funcs = entry;
}

// Tactile feedback function, probably used for the Logitech Cyberman

void I_Tactile(int on, int off, int total)
{
}

// Zone memory auto-allocation function that allocates the zone size
// by trying progressively smaller zone sizes until one is found that
// works.

static byte *AutoAllocMemory(int *size, int default_ram, int min_ram)
{
    byte *zonemem;

    // Allocate the zone memory.  This loop tries progressively smaller
    // zone sizes until a size is found that can be allocated.
    // If we used the -mb command line parameter, only the parameter
    // provided is accepted.

    zonemem = NULL;

    while (zonemem == NULL)
    {
        // We need a reasonable minimum amount of RAM to start.

        if (default_ram < min_ram)
        {
            I_Error("Unable to allocate %i MiB of RAM for zone", default_ram);
        }

#if !USE_ZONE_FOR_MALLOC
        // Try to allocate the zone memory.

        *size = default_ram * 1024 * 1024;

#if DOOM_SMALL
//        *size = (384+64) * 1024;
#if DOOM_TINY
#if PICO_ON_DEVICE
#if PICODOOM_RENDER_BABY
        // todo temp since we put the buffers here
        *size = 160 *1024;
#else
        *size = 40 * 1024;
#endif
#else
//        *size = 384 *1024;
        *size = 256 *1024;
#endif
#endif
#endif
        zonemem = malloc(*size);
#else
#if PICO_ON_DEVICE
        // we have set heap size to 0, so __HeapLimit is a good value
        extern char __HeapLimit;
        zonemem = (uint8_t *)(((uintptr_t)&__HeapLimit)&~3);
        *size = ((uint8_t *)SRAM4_BASE) - zonemem;
#else
#error use zone for malloc only on device
#endif
//        zonemem = static_zone_mem;
//        *size = count_of(static_zone_mem);
#endif
        // Failed to allocate?  Reduce zone size until we reach a size
        // that is acceptable.

        if (zonemem == NULL)
        {
            default_ram -= 1;
        }
    }

    return zonemem;
}

byte *I_ZoneBase (int *size)
{
    byte *zonemem;
    int min_ram, default_ram;
    int p;

    //!
    // @category obscure
    // @arg <mb>
    //
    // Specify the heap size, in MiB (default 16).
    //

#if !NO_USE_ARGS
    p = M_CheckParmWithArgs("-mb", 1);
    if (p > 0)
    {
        default_ram = atoi(myargv[p+1]);
        min_ram = default_ram;
    }
    else
#endif
    {
        default_ram = DEFAULT_RAM;
        min_ram = MIN_RAM;
    }

    zonemem = AutoAllocMemory(size, default_ram, min_ram);

    printf("zone memory: %p, %x allocated for zone\n", 
           zonemem, *size);

    return zonemem;
}

void I_PrintBanner(const char *msg)
{
    int i;
    int spaces = 35 - (strlen(msg) / 2);

    for (i=0; i<spaces; ++i)
        putchar(' ');

    puts(msg);
}

void I_PrintDivider(void)
{
    int i;

    for (i=0; i<75; ++i)
    {
        putchar('=');
    }

    putchar('\n');
}

void I_PrintStartupBanner(const char *gamedescription)
{
    I_PrintDivider();
    I_PrintBanner(gamedescription);
    I_PrintDivider();
    
    printf(
    " " PACKAGE_NAME " is free software, covered by the GNU General Public\n"
    " License.  There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
    " FOR A PARTICULAR PURPOSE. You are welcome to change and distribute\n"
    " copies under certain conditions. See the source for more information.\n");

    I_PrintDivider();
}

// 
// I_ConsoleStdout
//
// Returns true if stdout is a real console, false if it is a file
//

void I_Init() {
    I_InputInit();
}
//
// I_Init
//
/*
void I_Init (void)
{
    I_CheckIsScreensaver();
    I_InitTimer();
    I_InitJoystick();
}
void I_BindVariables(void)
{
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();
}
*/

//
// I_Quit
//

#include "pico/sem.h"
#include "whddata.h"
#include "piconet.h"

int8_t at_exit_screen;

extern uint8_t *text_screen_data;

static int8_t entry_line = -1;

static void scroll_line() {
    if (entry_line == 24) {
        memmove(text_screen_data, text_screen_data + 160, 160 * 24);
        memset(text_screen_data + entry_line * 160, 0, 160);
    } else
        entry_line++;
}

static void restart() {
#if PICO_ON_DEVICE
    watchdog_reboot(0, 0, 1);
#else
    exit(0);
#endif
}

static void write_text_line(const char *txt) {
    scroll_line();
    uint8_t *dest = text_screen_data + 160 * entry_line;
    while (*txt) {
        dest[0] = *txt++;
        dest[1] = 7;
        dest += 2;
    }
}

enum {
    SCANCODE_RETURN=40,
    SCANCODE_ESCAPE=41,
    SCANCODE_BACKSPACE=42,
    SCANCODE_TAB=43,
    SCANCODE_SPACE=44,
};

void write_num(int num, char *buffer, int len) {
    int pos;
    for(pos = 0; pos < len; ) {
        buffer[len - ++pos] = '0' + (num % 10);
        num /= 10;
        if (!num) break;
        if (pos % 3 == 0) {
            buffer--;
            buffer[len - pos] = ',';
        }
    }
    for(;pos < len; pos++) {
        buffer[len - pos - 1] = ' ';
    }
}

// waste space much?
void handle_exit_key_down(int scancode, bool shift, uint8_t *kb_buffer, int kb_len) {
    int l = strlen((char*)kb_buffer);
    if (entry_line < 0) {
        entry_line = 24;
        if (scancode == SCANCODE_RETURN || scancode == SCANCODE_SPACE) scancode = 0; // eat first key
    }
    if (scancode == SCANCODE_RETURN) {
        (text_screen_data + 160 * entry_line)[l * 2 + 7] = 0; // remove cursor
        uint8_t *cmd = kb_buffer;
        while (*cmd == ' ') cmd++;
        uint8_t *foo = cmd;
        while (*foo && *foo != ' ') {
            if (*foo >= 'a' && *foo <= 'z') *foo -= 'a' - 'A';
            foo++;
        }
        *foo++ = 0;
        if (!strcmp((char*)cmd, "DOOM") || !strcmp((char*)cmd, "DOOM.EXE")) restart();
        else if (!strcmp((char*)kb_buffer, "CLS")) {
            memset(text_screen_data, 0, 80 * 25 * 2);
            entry_line = 0;
#if PICO_NO_HARDWARE
            } else if (!strcmp((char*)kb_buffer, "exit")) {
                exit(0);
#endif
        } else if (!strcmp((char*)cmd, "CD")) {
            while (foo < kb_buffer + l && *foo == ' ') foo++;
            if (foo >= kb_buffer + l || !*foo) {
                write_text_line("A:\\");
            } else if (*foo == '.' && (!foo[1] || foo[1]==' ')) {
            } else {
                write_text_line("Invalid directory");
            }
            scroll_line();
            scroll_line();
        } else if (!strcmp((char*)cmd, "DIR")) {
            write_text_line("Directory of A:\\");
            scroll_line();
            char buf[80];
            strcpy(buf, "11/02/1994  01:20 AM                   DOOM.EXE");
            int binsize;
#if PICO_ON_DEVICE
            extern char __flash_binary_start;
            extern char __flash_binary_end;
            binsize = &__flash_binary_end - &__flash_binary_start;
#else
            binsize = 2428760;
#endif
#if PICO_ON_DEVICE
            int disksize = (int)(get_end_of_flash() - (const uint8_t *)XIP_BASE);
#else
            int disksize = 1u << (32 - __builtin_clz(binsize + whdheader->size));
#endif
            write_num(binsize, buf + 27, 9);
            write_text_line(buf);
            sprintf(buf, "11/02/1994  07:03 PM                   %s", whdheader->name);
            write_num(whdheader->size, buf + 27, 9);
            write_text_line(buf);
            int dsg_size = 0;
#define SHOW_SLOTS 1
#if PICO_ON_DEVICE && SHOW_SLOTS
            flash_slot_info_t slots[7];
            P_SaveGameGetExistingFlashSlotAddresses(slots, 7);
            int filecount = 2;
            for(int i=0;i<7;i++) {
                if (slots[i].data) {
                    sprintf(buf, "12/11/2021  04:21 PM                   %d.DSG", i);
                    write_num(slots[i].size + 8, buf + 27, 9);
                    write_text_line(buf);
                    dsg_size += slots[i].size + 8;
                    filecount++;
                }
            }
            int filesize = whd_map_base + whdheader->size - (uint8_t *)XIP_BASE;
            scroll_line();
            sprintf(buf, "        %d File(s)                bytes", filecount);
#else
            int filesize = ((binsize + 2047) & ~2047) + ((whdheader->size + 2047) & ~2047);
            scroll_line();
            strcpy(buf, "        2 File(s)                bytes");
#endif
            write_num(binsize + whdheader->size + dsg_size, buf + 23, 9);
            write_text_line(buf);
            strcpy(buf, "        0 Dir(s)                 bytes free");
            int remaining = disksize - filesize - dsg_size;
            if (remaining < 0) remaining = 0;
            write_num(remaining, buf + 23, 9);
            write_text_line(buf);
            scroll_line();
        } else {
            if (l) {
                write_text_line("Bad command or file name");
                scroll_line();
            }
            scroll_line();
        }
        kb_buffer[0] = 0;
    } else if (scancode == SCANCODE_ESCAPE) {
        kb_buffer[0] = 0;
    } else if (scancode == SCANCODE_BACKSPACE) {
        if (l) kb_buffer[l - 1] = 0;
    } else if (scancode == SCANCODE_TAB) {
        for(int max = (l + 8) & ~7; l < max && l < 80 - 4; l++) {
            kb_buffer[l] = 32;
        }
        kb_buffer[l+1] = 0;
    } else {
        int key = GetTypedChar(scancode, shift);
        if (key && key < 127 && l < 80 - 4) {
            kb_buffer[l] = key;
            kb_buffer[l+1] = 0;
        }
    }
    uint8_t *line = text_screen_data + 160 * entry_line;
    memcpy(line, "A\007:\007\\\007", 6);
    int i = 6;
    l = strlen((char*)kb_buffer);
    for(int j=0;j<l;j++) {
        line[i] = kb_buffer[j];
        line[i+1] = 7;
        i += 2;
    }
    line[i] = '_';
    line[i+1] = 0x87;
    for(i+=2;i<160;i+=2) {
        line[i+1] = 0;
    }
}

uint8_t *exit_screen_kb_buffer_80;
void __attribute((noreturn)) I_Quit (void)
{
    extern void D_Endoom();
#if USE_PICO_NET
    piconet_stop();
#endif
    D_Endoom();
    I_StopSong();
    while (!sem_available(&display_frame_freed)) {
        I_UpdateSound();
    }
    sem_acquire_blocking(&display_frame_freed);
    next_video_type = VIDEO_TYPE_TEXT;
    sem_release(&render_frame_ready);
    at_exit_screen = 1;
    I_StartTextInput(0,0,0,0);
    uint8_t buffer[80];
    buffer[0] = 0;
    exit_screen_kb_buffer_80 = buffer; // fine as this function never returns
    while (true) {
#if PICO_ON_DEVICE
        // no idea why the default timeout of 50 ms is NOT working here, hack hack hack away!
        I_GetEventTimeout(1000);
#if USB_SUPPORT
        tuh_task();
#endif
#endif
        I_UpdateSound();
    }
}



//
// I_Error
//

#if !NO_IERROR || !PICO_ON_DEVICE
void I_Error (const char *error, ...)
{
    va_list argptr;
    va_start(argptr, error);
    //stderr_print( "\nError: ");
    vprintf(error, argptr);
    va_end(argptr);
    __breakpoint();
}
#endif

//
// I_Realloc
//

void *I_Realloc(void *ptr, size_t size)
{
    void *new_ptr;

    new_ptr = realloc(ptr, size);

    if (size != 0 && new_ptr == NULL)
    {
        I_Error ("I_Realloc: failed on reallocation of %" PRIuPTR " bytes", size);
    }

    return new_ptr;
}

//
// Read Access Violation emulation.
//
// From PrBoom+, by entryway.
//

// C:\>debug
// -d 0:0
//
// DOS 6.22:
// 0000:0000  (57 92 19 00) F4 06 70 00-(16 00)
// DOS 7.1:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// Win98:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// DOSBox under XP:
// 0000:0000  (00 00 00 F1) ?? ?? ?? 00-(07 00)

#define DOS_MEM_DUMP_SIZE 10

static const unsigned char mem_dump_dos622[DOS_MEM_DUMP_SIZE] = {
  0x57, 0x92, 0x19, 0x00, 0xF4, 0x06, 0x70, 0x00, 0x16, 0x00};
static const unsigned char mem_dump_win98[DOS_MEM_DUMP_SIZE] = {
  0x9E, 0x0F, 0xC9, 0x00, 0x65, 0x04, 0x70, 0x00, 0x16, 0x00};
static const unsigned char mem_dump_dosbox[DOS_MEM_DUMP_SIZE] = {
  0x00, 0x00, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};
static unsigned char mem_dump_custom[DOS_MEM_DUMP_SIZE];

static const unsigned char *dos_mem_dump = mem_dump_dos622;

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    static boolean firsttime = true;

    if (firsttime)
    {
        int p, i, val;

        firsttime = false;
        i = 0;

        //!
        // @category compat
        // @arg <version>
        //
        // Specify DOS version to emulate for NULL pointer dereference
        // emulation.  Supported versions are: dos622, dos71, dosbox.
        // The default is to emulate DOS 7.1 (Windows 98).
        //

#if !NO_USE_ARGS
        p = M_CheckParmWithArgs("-setmem", 1);

        if (p > 0)
        {
            if (!strcasecmp(myargv[p + 1], "dos622"))
            {
                dos_mem_dump = mem_dump_dos622;
            }
            if (!strcasecmp(myargv[p + 1], "dos71"))
            {
                dos_mem_dump = mem_dump_win98;
            }
            else if (!strcasecmp(myargv[p + 1], "dosbox"))
            {
                dos_mem_dump = mem_dump_dosbox;
            }
            else
            {
                for (i = 0; i < DOS_MEM_DUMP_SIZE; ++i)
                {
                    ++p;

                    if (p >= myargc || myargv[p][0] == '-')
                    {
                        break;
                    }

                    M_StrToInt(myargv[p], &val);
                    mem_dump_custom[i++] = (unsigned char) val;
                }

                dos_mem_dump = mem_dump_custom;
            }
        }
#endif
    }

    switch (size)
    {
    case 1:
        *((unsigned char *) value) = dos_mem_dump[offset];
        return true;
    case 2:
        *((unsigned short *) value) = dos_mem_dump[offset]
                                    | (dos_mem_dump[offset + 1] << 8);
        return true;
    case 4:
        *((unsigned int *) value) = dos_mem_dump[offset]
                                  | (dos_mem_dump[offset + 1] << 8)
                                  | (dos_mem_dump[offset + 2] << 16)
                                  | (dos_mem_dump[offset + 3] << 24);
        return true;
    }

    return false;
}

#if PICO_ON_DEVICE

#if USE_ZONE_FOR_MALLOC
boolean disallow_core1_malloc;
void *__wrap_malloc(size_t size) {
    hard_assert(!(get_core_num() && disallow_core1_malloc)); // there should be no allocation from core 1 after startup
    void * rc = Z_MallocNoUser((int)size, PU_STATIC);
    return rc;
}

void *__wrap_calloc(size_t count, size_t size) {
    hard_assert(!(get_core_num() && disallow_core1_malloc)); // there should be no allocation from core 1 after startup
    void * rc = Z_MallocNoUser((int)(count * size), PU_STATIC);
    memset(rc, 0, count * size);
    return rc;
}

void __wrap_free(void *mem) {
    hard_assert(!(get_core_num() && disallow_core1_malloc)); // there should be no allocation from core 1 after startup
    Z_Free(mem);
}
#endif
#endif