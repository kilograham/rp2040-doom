add_library(adpcm-lib INTERFACE)
target_sources(adpcm-lib INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/adpcm-lib.c)

target_include_directories(adpcm-lib INTERFACE ${CMAKE_CURRENT_LIST_DIR})

#if (NOT PICO_ON_DEVICE)
#    add_executable(adpcm-xq adpcm-xq.c)
#    target_link_libraries(adpcm-xq adpcm-lib)
#endif()
