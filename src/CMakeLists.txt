if (PICO_SDK)
    set(BINARIES doom)
else()
    # comment out heretic/hexen/strife for now, as whilst they may build, how well they work is unclear
#    set(BINARIES doom heretic hexen strife setup)
    set(BINARIES doom setup)
endif()

add_subdirectory(adpcm-xq)

foreach(SUBDIR ${BINARIES})
    add_subdirectory("${SUBDIR}")
endforeach()

# Common source files used by absolutely everything:
cmake_policy(SET CMP0076 NEW)

if (NOT PICO_SDK)
    set(I_PLATFORM sdl)
else()
    set(I_PLATFORM pico)
endif()

add_library(common INTERFACE)
target_sources(common INTERFACE
    i_main.c
    i_system.h
    m_argv.c             m_argv.h
    m_misc.c             m_misc.h)

target_compile_definitions(common INTERFACE
        #HACK_FINALE_E1M1=1
        #HACK_FINALE_SHAREWARE=1
#        DEBUG_MOBJ=1
)

if (NOT PICO_SDK)
    add_library(common_sdl INTERFACE)
    target_sources(common_sdl INTERFACE
            i_system.c
    )
else()
    add_subdirectory(pico)
endif()

target_link_libraries(common INTERFACE common_${I_PLATFORM})

# todo chocolate-server also doesn't currently build
if (NOT PICO_SDK)
    # Dedicated server (chocolate-server):

    set(DEDSERV_FILES
        d_dedicated.c
        d_mode.c             d_mode.h
        i_timer.c            i_timer.h
        net_common.c         net_common.h
        net_dedicated.c      net_dedicated.h
        net_io.c             net_io.h
        net_packet.c         net_packet.h
        net_sdl.c            net_sdl.h
        net_query.c          net_query.h
        net_server.c         net_server.h
        net_structrw.c       net_structrw.h
        z_native.c           z_zone.h)

    add_executable("${PROGRAM_PREFIX}server" WIN32 ${COMMON_SOURCE_FILES} ${DEDSERV_FILES})
    target_include_directories("${PROGRAM_PREFIX}server"
                               PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
    target_link_libraries("${PROGRAM_PREFIX}server" common SDL2::SDL2main SDL2::net)
endif()

# Source files used by the game BINARIES (chocolate-doom, etc.)

add_library(game INTERFACE)
target_sources(game INTERFACE
    aes_prng.c          aes_prng.h
    d_event.c           d_event.h
                        doomkeys.h
                        doomtype.h
    d_iwad.c            d_iwad.h
    d_loop.c            d_loop.h
    d_mode.c            d_mode.h
                        d_ticcmd.h
    deh_str.c           deh_str.h
    gusconf.c           gusconf.h
    i_cdmus.h
    i_endoom.h
    i_glob.h
    i_input.h
    i_joystick.h
    i_swap.h
    i_midipipe.h
    i_sound.h
    i_timer.h
    i_video.h
    i_videohr.h
    midifile.c          midifile.h
    mus2mid.c           mus2mid.h
    m_bbox.c            m_bbox.h
    m_cheat.c           m_cheat.h
    m_config.c          m_config.h
    m_controls.c        m_controls.h
    m_fixed.c           m_fixed.h

    net_client.c        net_client.h
    sha1.c              sha1.h
    memio.c             memio.h
    tables.c            tables.h
    v_diskicon.c        v_diskicon.h
    v_video.c           v_video.h
                        v_patch.h
    w_checksum.c        w_checksum.h
    w_main.c            w_main.h
    w_wad.c             w_wad.h
    w_file.c            w_file.h
        w_file_posix.c
        w_file_memory.c

    w_merge.c           w_merge.h
    z_zone.c            z_zone.h

    i_oplmusic.c
    i_sound.c

)

if (NOT PICO_SDK)
    add_library(game_sdl INTERFACE)
    target_sources(game_sdl INTERFACE
            # todo maybe a separate network library
            net_common.c        net_common.h
            net_dedicated.c     net_dedicated.h
            net_defs.h
            net_gui.c           net_gui.h
            net_io.c            net_io.h
            net_loop.c          net_loop.h
            net_packet.c        net_packet.h
            net_query.c         net_query.h
            net_sdl.c           net_sdl.h
            net_server.c        net_server.h
            net_structrw.c      net_structrw.h

            w_file_stdc.c
            w_file_win32.c

            i_cdmus.c
            i_endoom.c
            i_glob.c
            i_input.c
            i_joystick.c

            i_midipipe.c
            i_musicpack.c
            i_pcsound.c
            i_sdlmusic.c
            i_sdlsound.c
            i_timer.c
            i_video.c
            i_videohr.c
        )
else()
    add_library(game_pico INTERFACE)
endif()

target_link_libraries(game INTERFACE game_${I_PLATFORM})
if(MSVC)
    target_sources(game INTERFACE
         "../win32/win_opendir.c" "../win32/win_opendir.h")
endif()

target_link_libraries(game INTERFACE common opl)

add_library(game_deh INTERFACE)
target_sources(game_deh INTERFACE
                        deh_defs.h
    deh_io.c            deh_io.h
    deh_main.c          deh_main.h
    deh_mapping.c       deh_mapping.h
    deh_text.c)

target_link_libraries(game_deh INTERFACE game)

if (NOT PICO_SDK)
    set(EXTRA_LIBS SDL2::SDL2main SDL2::SDL2 SDL2::mixer SDL2::net textscreen pcsound)
    if(SAMPLERATE_FOUND)
        list(APPEND EXTRA_LIBS samplerate::samplerate)
    endif()
    if(PNG_FOUND)
        list(APPEND EXTRA_LIBS PNG::PNG)
    endif()

    if ("doom" IN_LIST BINARIES)
        if(WIN32)
            add_executable("${PROGRAM_PREFIX}doom" WIN32 "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
        else()
            add_executable("${PROGRAM_PREFIX}doom")
        endif()
        target_link_libraries("${PROGRAM_PREFIX}doom" PRIVATE game_deh)

        target_include_directories("${PROGRAM_PREFIX}doom"
                                   PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
        target_link_libraries("${PROGRAM_PREFIX}doom" PRIVATE doom ${EXTRA_LIBS})

        if(MSVC)
            set_target_properties("${PROGRAM_PREFIX}doom" PROPERTIES
                                  LINK_FLAGS "/MANIFEST:NO")
        endif()
    endif()

    if ("heretic" IN_LIST BINARIES)
        if(WIN32)
            add_executable("${PROGRAM_PREFIX}heretic" WIN32 "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
        else()
            add_executable("${PROGRAM_PREFIX}heretic" )
        endif()
        target_link_libraries("${PROGRAM_PREFIX}heretic" PRIVATE game_deh)

        target_include_directories("${PROGRAM_PREFIX}heretic"
                                   PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
        target_link_libraries("${PROGRAM_PREFIX}heretic" PRIVATE heretic ${EXTRA_LIBS})

        if(MSVC)
            set_target_properties("${PROGRAM_PREFIX}heretic" PROPERTIES
                                  LINK_FLAGS "/MANIFEST:NO")
        endif()
    endif()

    if ("hexen" IN_LIST BINARIES)
        if(WIN32)
            add_executable("${PROGRAM_PREFIX}hexen" WIN32 "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
        else()
            add_executable("${PROGRAM_PREFIX}hexen")
        endif()
        target_link_libraries("${PROGRAM_PREFIX}hexen" PRIVATE game)

        target_include_directories("${PROGRAM_PREFIX}hexen"
                                   PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
        target_link_libraries("${PROGRAM_PREFIX}hexen" PRIVATE hexen ${EXTRA_LIBS})

        if(MSVC)
            set_target_properties("${PROGRAM_PREFIX}hexen" PROPERTIES
                                  LINK_FLAGS "/MANIFEST:NO")
        endif()
    endif()

    if ("strife" IN_LIST BINARIES)
        if(WIN32)
            add_executable("${PROGRAM_PREFIX}strife" WIN32 ${SOURCE_FILES_WITH_DEH} "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
        else()
            add_executable("${PROGRAM_PREFIX}strife" ${SOURCE_FILES_WITH_DEH})
        endif()
        target_link_libraries("${PROGRAM_PREFIX}strife" PRIVATE game_deh)

        target_include_directories("${PROGRAM_PREFIX}strife"
                                   PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
        target_link_libraries("${PROGRAM_PREFIX}strife" PRIVATE strife ${EXTRA_LIBS})

        if(MSVC)
            set_target_properties("${PROGRAM_PREFIX}strife" PROPERTIES
                                  LINK_FLAGS "/MANIFEST:NO")
        endif()
    endif()

    # Source files needed for chocolate-setup:

    if ("setup" IN_LIST BINARIES)
        set(SETUP_FILES
            deh_str.c           deh_str.h
            d_mode.c            d_mode.h
            d_iwad.c            d_iwad.h
            i_timer.c           i_timer.h
            m_config.c          m_config.h
            m_controls.c        m_controls.h
            net_io.c            net_io.h
            net_packet.c        net_packet.h
            net_sdl.c           net_sdl.h
            net_query.c         net_query.h
            net_structrw.c      net_structrw.h
            z_native.c          z_zone.h)

        if(WIN32)
            add_executable("${PROGRAM_PREFIX}setup" WIN32 ${SETUP_FILES} "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
        else()
            add_executable("${PROGRAM_PREFIX}setup" ${SETUP_FILES} )
        endif()
        target_link_libraries("${PROGRAM_PREFIX}setup" PRIVATE setup common)

        target_include_directories("${PROGRAM_PREFIX}setup"
                                   PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
        if (NOT PICO_SDK)
            target_link_libraries("${PROGRAM_PREFIX}setup" PRIVATE SDL2::SDL2main SDL2::SDL2 SDL2::mixer SDL2::net)
        endif()
        target_link_libraries("${PROGRAM_PREFIX}setup" PRIVATE textscreen)

        if(MSVC)
            set_target_properties("${PROGRAM_PREFIX}setup" PROPERTIES
                                  LINK_FLAGS "/MANIFEST:NO")
        endif()
    endif()

    add_executable(midiread midifile.c z_native.c i_system.c m_argv.c m_misc.c)
    target_compile_definitions(midiread PRIVATE "-DTEST")
    target_include_directories(midiread PRIVATE "." "${CMAKE_CURRENT_BINARY_DIR}/../")

    if (NOT PICO_SDK)
        target_link_libraries(midiread SDL2::SDL2main SDL2::SDL2)
    endif()

    add_executable(mus2mid mus2mid.c memio.c z_native.c i_system.c m_argv.c m_misc.c)
    target_compile_definitions(mus2mid PRIVATE "-DSTANDALONE")
    target_include_directories(mus2mid PRIVATE "." "${CMAKE_CURRENT_BINARY_DIR}/../")
    if (NOT PICO_SDK)
        target_link_libraries(mus2mid SDL2::SDL2main SDL2::SDL2)
    endif()
endif() # NOT PICO_SDK
add_library(small_doom_common INTERFACE)

target_link_libraries(small_doom_common INTERFACE adpcm-lib) # handy to have about

target_compile_definitions(small_doom_common INTERFACE
        SHRINK_MOBJ=1 # may break saved games

        DOOM_ONLY=1 # don't support hexxen etc.
        DOOM_SMALL=1
        DOOM_CONST=1 # thread const thru lots of places

        SOUND_LOW_PASS=1
        NUM_SOUND_CHANNELS=8 # sounds ok (actually that is how many are used by default)

        # functionality
        NO_USE_CHECKSUM=1
        NO_USE_RELOAD=1
        USE_SINGLE_IWAD=1
        NO_USE_WIPE=1 #memory if nothing else - we can fix this (you can do it)
        NO_USE_JOYSTICK=1
        NO_USE_DEH=1
        NO_USE_MUSIC_PACKS=1

        USE_FLAT_MAX_256=1
        USE_MEMMAP_ONLY=1

        USE_LIGHTMAP_INDEXES=1 # saves about 9K

        #FRACBITS=15 # ha ha doesn't seem to be passed thru everywhere!
        #NO_USE_ENDDOOM=1
        #NO_DRAW_VISPLANES=1

        USE_ERASE_FRAME=1 #
        NO_DRAW_MID=1
        NO_DRAW_TOP=1
        NO_DRAW_BOTTOM=1
        NO_DRAW_MASKED=1
        NO_DRAW_SKY=1
        #NO_DRAW_SPANS=1
        NO_DRAW_SPRITES=1
        NO_DRAW_PSPRITES=1

#        NO_VISPLANES=1
        NO_VISPLANE_GUTS=1
        NO_VISPLANE_CACHES=1 # todo these might be fine as they can be temporary
        NO_DRAWSEGS=1
        NO_VISSPRITES=1


        NO_MASKED_FLOOR_CLIP=1 # not needed with PD_ rendering and with floor clip it we have occasional visual glitches with sprites split across visplanes

        PD_DRAW_COLUMNS=1
        PD_DRAW_MARKERS=1
        PD_DRAW_PLANES=1

        PD_SCALE_SORT=1

        PD_CLIP_WALLS=1
        PD_QUANTIZE=1

        PD_SANITY=1

        #MU_STATS=1
        PD_COLUMNS=1
        PICO_DOOM=1
        NO_USE_DS_COLORMAP=1
        NO_USE_DC_COLORMAP=1

        #PRINT_COLORMAPS=1
        #PRINT_PALETTE=1
        USE_READONLY_MMAP=1

# -----------------------------------------------------------------
# MUSIC RELATED
# -----------------------------------------------------------------
        NO_USE_TIMIDITY=1
        NO_USE_GUS=1
        NO_USE_LIBSAMPLERATE=1

        # slightly slower but only uses 1K of sin tables vs 9K
        EMU8950_NO_WAVE_TABLE_MAP=1

        EMU8950_NO_TLL=1 # don't use lookup table for total level
        EMU8950_NO_FLOAT=1 # double check there is no float
        EMU8950_NO_TIMER=1 # disable timer which isn't used
        EMU8950_NO_TEST_FLAG=1 # disable test flags (which aren't used)
        EMU8950_SIMPLER_NOISE=1 # only generate noise bit when needed
        EMU8950_SHORT_NOISE_UPDATE_CHECK=1 # only update short noise if it is used
        # actually this doesn't make things faster
        #EMU8950_LINEAR_NEG_NOT_NOT=1 # negative values rounded towrds -infinity not 0 without this; does it matter?

        EMU8950_LINEAR_SKIP=1 # skip silent slots
        EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION # early out envelope when DECAY/SUSTAIN/RELEASE envelope reaches mute
        EMU8950_NO_PERCUSSION_MODE=1 # rhythm only mode (doom doesn't use percussion; whhhaaaah!?)
        EMU8950_LINEAR=1 # reorganize to do linear runs of channels

        # things we really don't care about but need for diff-ing with non linear version
        # BEWARE - need to turn this off for comparison with on device version which doesn't do nitpicks (though could i guess)

        EMU8950_ASM=1
#        EMU8950_NIT_PICKS=1
#        DUMPO=1

# -----------------------------------------------------------------
# FLASH SIZE
# -----------------------------------------------------------------
        NO_USE_STATE_MISC #doesn't appear to be used anyway - perhaps in doom only

# -----------------------------------------------------------------
# RAM SIZE
# -----------------------------------------------------------------
        USE_RAW_MAPNODE=1
        USE_RAW_MAPVERTEX=1
        USE_RAW_MAPSEG=1
        USE_RAW_MAPLINEDEF=1
        # USE_RAW_MAPSUBSECTOR=1 # obsolete... now WHD
        # USE_RAW_MAPSIDEDEF=1 obsolete
        USE_RAW_MAPTHING=1

        USE_INDEX_LINEBUFFER=1
        NO_USE_ZLIGHT=1
        NO_Z_ZONE_ID=1 # seems unused

        Z_MALOOC_EXTRA_DATA=1
        USE_THINKER_POOL=1
        NO_INTERCEPTS_OVERRUN=1
#        INCLUDE_SOUND_C_IN_S_SOUND=1 # avoid issues with non static const array
# -----------------------------------------------------------------
# IMMUTABLE
# -----------------------------------------------------------------

        TEMP_IMMUTABLE_DISABLED=1
        USE_CONST_SFX=1
        USE_CONST_MUSIC=1

        NO_DEMO_RECORDING=1
        PICO_NO_TIMING_DEMO=1
        NO_USE_EXIT=1 # not sure whether we have an exit... if so what does it need to do?

        PICO_DEBUG_PIN_BASE=18
        PICO_DEBUG_PIN_COUNT=2
)

target_compile_options(small_doom_common INTERFACE
        -Wall
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-unused-variable
        )

if (TARGET chocolate-doom)
    target_compile_definitions(chocolate-doom PRIVATE
        USE_FLAT_MAX_256=1
    )
endif()

function(add_doom_tiny SUFFIX RENDER_LIB)
    add_executable(doom_tiny${SUFFIX})
    target_link_libraries(doom_tiny${SUFFIX} PRIVATE game)

    target_sources(doom_tiny${SUFFIX} PRIVATE
            tiny_huff.c
            musx_decoder.c
            image_decoder.c
            )
    target_include_directories(doom_tiny${SUFFIX} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
    target_link_libraries(doom_tiny${SUFFIX} PRIVATE doom ${EXTRA_LIBS})

    target_link_libraries(doom_tiny${SUFFIX} PRIVATE small_doom_common)

    if (PICO_SDK)
        pico_set_float_implementation(doom_tiny${SUFFIX} none)
        pico_set_double_implementation(doom_tiny${SUFFIX} none)
        pico_add_extra_outputs(doom_tiny${SUFFIX})
    endif()

    target_compile_options(doom_tiny${SUFFIX} PRIVATE
            -Wno-format-truncation)
    target_compile_definitions(doom_tiny${SUFFIX} PRIVATE
        DOOM_TINY=1
        EMU8950_SLOT_RENDER=1

        SHRINK_MOBJ=1 # may break saved games
        NO_RDRAW=1

        #NO_DRAW_MENU=1
        # SDK minimilization
        PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1
        PICO_TIME_DEFAULT_ALARM_POOL_MAX_TIMERS=1
        PICO_DISABLE_SHARED_IRQ_HANDLERS=1
        PICO_USE_OPTIMISTIC_SBRK=1

        # MUSIC
        USE_EMU8950_OPL=1
        USE_DIRECT_MIDI_LUMP=1

        NO_USE_NET=1 # standard networking
        USE_PICO_NET=1
        # for vgaboard
        PICO_DEFAULT_I2C=1
        PICO_DEFAULT_I2C_SDA_PIN=18
        PICO_DEFAULT_I2C_SCL_PIN=19

        #NO_USE_LOAD=1
        #NO_USE_SAVE=1
        $<$<BOOL:${PICO_ON_DEVICE}>:NO_FILE_ACCESS=1>

        SAVE_COMPRESSED=1
        LOAD_COMPRESSED=1

        NO_USE_ARGS=1
        NO_USE_SAVE_CONFIG=1
        NO_USE_FLOAT=1

        USE_VANILLA_KEYBOARD_MAPPING_ONLY=1

        #FORCE_NODRAW=1

        NO_USE_LOADING_DISK=1

        # -- these aren't used any more, we adapt based on DOOM_TINY
        #NO_USE_ST=1 # for now skip all the status crap
        #NO_USE_DEFAULT_ST_LIB=1

        #NO_PAGE_DRAWER=1 # splash screens
        #NO_HU_DRAWER=1

        USE_WHD=1


        NO_Z_MALLOC_USER_PTR=1 # onyl needed for freeing textures i think

        FIXED_SCREENWIDTH=1
        FLOOR_CEILING_CLIP_8BIT=1

#        NO_USE_WI=1
        USE_MUSX=1
        MUSX_COMPRESSED=1

        NO_SCREENSHOT=1

        NO_USE_BOUND_CONFIG=1

        USE_FPS=1

#        PICO_DOOM_INFO=1
    )
    target_compile_definitions(doom_tiny${SUFFIX} PRIVATE
        #FORCE_NODRAW=1
        USE_MEMORY_WAD=1
        PICO_DEBUG_MALLOC=1
        EMU8950_NO_RATECONV=1

        PICO_CORE1_STACK_SIZE=0x4f8

        NO_IERROR=1
    )
    if (PICO_ON_DEVICE)
        target_compile_options(doom_tiny${SUFFIX} PRIVATE -fno-common -fdata-sections -Wl,--sort-section=alignment)
        target_compile_definitions(doom_tiny${SUFFIX} PRIVATE
                NO_ZONE_DEBUG=1
                )
        #target_link_libraries(doom_tiny${SUFFIX} PRIVATE hardware_flash)
    endif()
    if (FORCE_DEBUG)
        target_compile_options(doom_tiny${SUFFIX} PRIVATE -g)
    endif()
    target_link_libraries(doom_tiny${SUFFIX} PRIVATE ${RENDER_LIB})
    set(PICO_HACK 0)
    set(STAMP_HACK 0)
    if (PICO_HACK)
        target_compile_definitions(doom_tiny${SUFFIX} PRIVATE
                PICO_AUDIO_I2S_DATA_PIN=20
                PICO_AUDIO_I2S_CLOCK_PIN_BASE=21
                NO_USE_UART=1
                )
        pico_enable_stdio_uart(doom_tiny${SUFFIX} 0)
    elseif(STAMP_HACK)
        target_compile_definitions(doom_tiny${SUFFIX} PRIVATE
                PICO_AUDIO_I2S_DATA_PIN=22
                PICO_AUDIO_I2S_CLOCK_PIN_BASE=23
                NO_USE_UART=1
                INCREASE_I2S_DRIVE_STRENGTH=1
        )
        pico_enable_stdio_uart(doom_tiny${SUFFIX} 0)
    endif()
endfunction()

add_subdirectory(whd_gen)

add_library(render_newhope INTERFACE)
target_sources(render_newhope INTERFACE
        pd_render.cpp
        )
target_compile_definitions(render_newhope INTERFACE
        PICODOOM_RENDER_NEWHOPE=1
        MERGE_DISTSCALE0_INTO_VIEWCOSSINANGLE=1
        NO_USE_ZLIGHT=1
        PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=4
        PICO_SCANVIDEO_LINKED_SCANLINE_BUFFERS=1
        PICO_SCANVIDEO_SCANLINE_RELEASE_FUNCTION=1
        )
if (PICO_ON_DEVICE)
    target_compile_definitions(render_newhope INTERFACE
            PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=164
    )
else()
    # we cannot rewrite the buffers in host mode so we need them to be big enough for text mode
    target_compile_definitions(render_newhope INTERFACE
            PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=324
    )
endif()

if (PICO_SDK)
    add_doom_tiny("" render_newhope)
    add_library(tiny_settings INTERFACE)
    target_compile_definitions(tiny_settings INTERFACE
            WHD_SUPER_TINY=1
            DEMO1_ONLY=1
            NO_USE_FINALE_CAST=1
            NO_USE_FINALE_BUNNY=1
    )
    target_compile_definitions(doom_tiny PRIVATE
            TINY_WAD_ADDR=0x10040000
    )
    target_link_libraries(doom_tiny PRIVATE tiny_settings)
    add_doom_tiny(_nost render_newhope)

    target_compile_definitions(doom_tiny_nost PRIVATE
            # NO_USE_FINALE_CAST=1 # note this is only used by doom II but doesnt take that much more space
            TINY_WAD_ADDR=0x10048000
            )
    if (PICO_ON_DEVICE)
        suppress_tinyusb_warnings()
        add_doom_tiny(_usb render_newhope)
        target_link_libraries(doom_tiny_usb PRIVATE tiny_settings pico_cd)
        target_compile_definitions(doom_tiny_usb PRIVATE
                USB_SUPPORT=1
                TINY_WAD_ADDR=0x10042000 # ugh tinyusb is >8K currently!!
        )
        add_doom_tiny(_nost_usb render_newhope)
        target_link_libraries(doom_tiny_nost_usb PRIVATE pico_cd)
        target_compile_definitions(doom_tiny_nost_usb PRIVATE
                USB_SUPPORT=1
                TINY_WAD_ADDR=0x10048000
                )
    endif()
endif()


