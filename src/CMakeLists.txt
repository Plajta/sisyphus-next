cmake_minimum_required(VERSION 3.13)

set(DEVICE_NAME "Sisyphus" CACHE STRING "The device name used for the \"info\" command")

# Hardware Feature Selection (Automatic based on Board)
if(PICO_BOARD STREQUAL "plajta_sisyfoss_beta")
    set(KEYBOARD_DEFAULT ON)
    set(CHARGER_DEFAULT ON)
    set(COLOR_SENSOR_DEFAULT ON)
else()
    set(KEYBOARD_DEFAULT OFF)
    set(CHARGER_DEFAULT OFF)
    set(COLOR_SENSOR_DEFAULT OFF)
endif()

option(SISYFOSS_USE_KEYBOARD "Use TCA8418 keyboard controller" ${KEYBOARD_DEFAULT})
option(SISYFOSS_USE_CHARGER "Use BQ25619 charger" ${CHARGER_DEFAULT})
option(SISYFOSS_USE_COLOR_SENSOR "Use VEML3328 color sensor" ${COLOR_SENSOR_DEFAULT})

option(SISYPHUS_DEBUG "Enable debug flag" OFF)
option(USE_ETERNITY "Enable usage of Eternity serial bootloader" OFF)

# Git info for info command
if(NOT DEFINED GIT_COMMIT_SHA OR GIT_COMMIT_SHA STREQUAL "")
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND git diff --quiet
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_DIFF_RESULT
        ERROR_QUIET
    )

    # If there are uncommitted changes, append -dirty to the commit SHA
    if(NOT GIT_DIFF_RESULT EQUAL 0)
        set(GIT_COMMIT_SHA "${GIT_COMMIT_SHA}-dirty")
    endif()
endif()

# Get the current build date and time
execute_process(
    COMMAND date "+%Y-%m-%d,%H:%M:%S"
    OUTPUT_VARIABLE BUILD_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(PICO_SDK_FETCH_FROM_GIT ON)
set(PICO_EXTRAS_FETCH_FROM_GIT ON)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Required for clangd

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(sisyphus C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Executable and Sources
add_executable(${PROJECT_NAME}
    main.c
)

target_sources(${PROJECT_NAME} PRIVATE
    audio/audio.c
    color/color.c
    protocol/protocol.c
    usb/usb_descriptors.c
    util/lightshow/lightshow.c
)

target_include_directories(${PROJECT_NAME} PUBLIC protocol audio usb color util/lightshow)

# Compile Definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PICO_AUDIO_I2S_MONO_INPUT=1
    PICO_AUDIO_I2S_MONO_OUTPUT=1
    GIT_COMMIT_SHA="${GIT_COMMIT_SHA}"
    DEVICE_NAME="${DEVICE_NAME}"
    BUILD_DATE="${BUILD_DATE}"
    USE_ETERNITY=$<BOOL:${USE_ETERNITY}>
    SISYPHUS_DEBUG_STATUS=$<BOOL:${SISYPHUS_DEBUG}>
)

if(SISYPHUS_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SISYPHUS_DEBUG)
endif()

# Storage definitions
if(NOT DEFINED LITTLEFS_SIZE_BYTES_STRING)
    set(LITTLEFS_SIZE_BYTES_STRING "(1024 * 1024)")
endif()

math(EXPR LITTLEFS_SIZE_BYTES "${LITTLEFS_SIZE_BYTES_STRING}")
math(EXPR FLASH_SIZE_BYTES "${PICO_FLASH_SIZE_BYTES}")

if(LITTLEFS_SIZE_BYTES GREATER_EQUAL FLASH_SIZE_BYTES)
    message(FATAL_ERROR "LITTLEFS Partition size (${LITTLEFS_SIZE_BYTES}) must be smaller than flash size (${FLASH_SIZE_BYTES})!")
endif()

# Subdirectories and Libraries
add_subdirectory(util)
add_subdirectory(battery)
add_subdirectory(color/sensors)

# Base libraries
set(SISYPHUS_LIBS
    pico_stdlib
    pico_audio_i2s
    pico_bootrom
    hardware_i2c
    littlefs_pico
    tinyusb_device
    tinyusb_board
    error
    ws2812
)

# Conditional components
if(SISYFOSS_USE_KEYBOARD)
    add_subdirectory(keyboard)
    list(APPEND SISYPHUS_LIBS keyboard_tca8418)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SISYFOSS_HAS_KEYBOARD_CONTROLLER)
endif()

if(SISYFOSS_USE_CHARGER)
    list(APPEND SISYPHUS_LIBS charger_bq25619)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SISYFOSS_HAS_CHARGER)
endif()

if(SISYFOSS_USE_COLOR_SENSOR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SISYFOSS_COLOR_VEML3328)
    list(APPEND SISYPHUS_LIBS color_sensor_veml3328)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${SISYPHUS_LIBS})

pico_add_extra_outputs(${PROJECT_NAME})

# Binary info
pico_set_program_name(${PROJECT_NAME} ${PROJECT_NAME})
pico_set_program_description(${PROJECT_NAME} "Basically a physical soundboard")
pico_set_program_version(${PROJECT_NAME} "0.1")
pico_set_program_url(${PROJECT_NAME} "https://plajta.eu")

# Linker scripts and Flash Setup
target_link_options(${PROJECT_NAME} PRIVATE -Wl,--print-memory-usage)

if(USE_ETERNITY)
    add_subdirectory(eternity)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/memmap_eternity.ld.template ${CMAKE_CURRENT_BINARY_DIR}/memmap_custom.ld)
else()
    configure_file(${CMAKE_CURRENT_LIST_DIR}/memmap_stage2.ld.template ${CMAKE_CURRENT_BINARY_DIR}/memmap_custom.ld)
endif()

pico_set_linker_script(${PROJECT_NAME} ${CMAKE_CURRENT_BINARY_DIR}/memmap_custom.ld)

# Flash target
set(RESET_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/sisyphus_reset.sh)

add_custom_target(flash
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND sh -c "${RESET_SCRIPT} 2>&1"
    COMMENT "Resetting, and flashing the RP2040"
    USES_TERMINAL
)

if(USE_ETERNITY)
    add_custom_command(TARGET flash POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/eternity/protocol/flasher.py ${PROJECT_NAME}.bin
    )
else()
    add_custom_command(TARGET flash POST_BUILD
        COMMAND picotool load -f -x ${PROJECT_NAME}.uf2
    )
endif()

# Eternity Combined Binary
if(USE_ETERNITY)
    if(NOT DEFINED ETERNITY_SIZE_BYTES_STRING)
        set(ETERNITY_SIZE_BYTES_STRING "(32 * 1024)") # Default if not defined
    endif()
    math(EXPR ETERNITY_SIZE_BYTES "${ETERNITY_SIZE_BYTES_STRING}")
    set(OUTPUT_BIN "${CMAKE_BINARY_DIR}/combined.bin")

    add_custom_command(
        OUTPUT ${OUTPUT_BIN}
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/eternity/merge_binaries.py
            ${CMAKE_BINARY_DIR}/eternity/eternity.bin
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
            ${ETERNITY_SIZE_BYTES}
            ${OUTPUT_BIN}
        DEPENDS ${PROJECT_NAME} eternity
        COMMENT "Generating combined binary"
        VERBATIM
    )

    if(NOT PICO_NO_PICOTOOL)
        set(OUTPUT_UF2 "${CMAKE_BINARY_DIR}/combined.uf2")
        add_custom_command(
            OUTPUT ${OUTPUT_UF2}
            COMMAND picotool uf2 convert --quiet --family rp2040 ${OUTPUT_BIN} ${OUTPUT_UF2}
            DEPENDS ${OUTPUT_BIN}
            COMMENT "Generating combined UF2"
            VERBATIM
        )
        add_custom_target(combine_eternity ALL DEPENDS ${OUTPUT_BIN} ${OUTPUT_UF2})
    else()
        add_custom_target(combine_eternity ALL DEPENDS ${OUTPUT_BIN})
    endif()
endif()
