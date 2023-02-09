set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.13)

# We use PICO_SDK_PATH being set as an indicator that we're doing a Pico BUILD
if (PICO_SDK_PATH)
    include(pico_sdk_import.cmake)
    include(pico_extras_import.cmake)
endif()

project("Chocolate Doom" VERSION 3.0.0 LANGUAGES C CXX)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 14)

if (PICO_SDK_PATH)
    # we are using git@github.com:liamfraser/tinyusb.git as it has some RP2040 fixes that aren't upstreamed yet
    #set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/3rdparty/tinyusb)
    # this only affects device builds device, but we want to use zone for malloc in this case so we don't have two separate elastic spaces and can fit more in
    set(SKIP_PICO_MALLOC 1)
    pico_sdk_init()
    if (PICO_ON_DEVICE AND NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        message(WARNING "You should do a MinSizeRel build when targeting the RP2040
        (with -DCMAKE_BUILD_TYPE=MinSizeRel)")
    endif()
endif()

set(CMAKE_C_STANDARD 11)

# Autotools variables
set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR})
set(top_builddir ${CMAKE_CURRENT_BINARY_DIR})

# AC_INIT variables
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_TARNAME "chocolate-doom")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "chocolate-doom-dev-list@chocolate-doom.org")

string(REGEX REPLACE " Doom$" "" PACKAGE_SHORTNAME "${PACKAGE_NAME}")
set(PACKAGE_COPYRIGHT "Copyright (C) 1993-2017")
set(PACKAGE_LICENSE "GNU General Public License, version 2")

# Any settings that should apply to all targets in this directory and all
# subdirectories should go here.  Use judiciously.
if(MSVC)
    add_definitions("/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_SECURE_NO_DEPRECATE"
                    "/D_CRT_NONSTDC_NO_DEPRECATE")
else()
    #add_compile_options("-Wall" "-Wdeclaration-after-statement" "-Wredundant-decls")
    add_compile_options("-Wall" "-Wredundant-decls")
endif()

# Note PICO_SDK path is set by the SDK initialization if it occurs above
if (NOT PICO_SDK)
    find_package(SDL2 2.0.1 REQUIRED)
    find_package(SDL2_mixer 2.0.0 REQUIRED)
    find_package(SDL2_net 2.0.0 REQUIRED)

    # Check for libsamplerate.
    find_package(samplerate)
    if(SAMPLERATE_FOUND)
        set(HAVE_LIBSAMPLERATE TRUE)
    endif()

    # Check for libpng.
    find_package(PNG)
    if(PNG_FOUND)
        set(HAVE_LIBPNG TRUE)
    endif()
endif()

set(HAVE_MMAP 1)
find_package(m)

include(CheckSymbolExists)
include(CheckIncludeFile)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)
check_include_file("dirent.h" HAVE_DIRENT_H)

string(CONCAT WINDOWS_RC_VERSION "${PROJECT_VERSION_MAJOR}, "
    "${PROJECT_VERSION_MINOR}, ${PROJECT_VERSION_PATCH}, 0")

# Without a hyphen. This is used for the bash-completion scripts.
string(TOLOWER "${PACKAGE_SHORTNAME}" PROGRAM_SPREFIX)

# With a hyphen, used almost everywhere else.
set(PROGRAM_PREFIX "${PROGRAM_SPREFIX}-")

configure_file(cmake/config.h.cin config.h)

configure_file(src/resource.rc.in src/resource.rc)
configure_file(src/setup-res.rc.in src/setup-res.rc)
configure_file(src/setup/setup-manifest.xml.in src/setup/setup-manifest.xml)

foreach(SUBDIR textscreen midiproc opl pcsound src)
    add_subdirectory("${SUBDIR}")
endforeach()
