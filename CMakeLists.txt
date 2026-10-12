cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD adafruit_fruit_jam CACHE STRING "Adafruit Fruit Jam" FORCE)
set(PICO_PSRAM_CS_PIN 47 CACHE STRING "Fruit Jam PSRAM CS" FORCE)
set(PICO_PSRAM_SIZE_BYTES 0x800000 CACHE STRING "Fruit Jam PSRAM size" FORCE)
set(PICO_CXX_ENABLE_EXCEPTIONS 1 CACHE BOOL "plus4emu uses exceptions" FORCE)
set(PICO_DEFAULT_UART 0 CACHE STRING "Diagnostic UART" FORCE)
set(PICO_DEFAULT_UART_TX_PIN 44 CACHE STRING "Diagnostic UART TX" FORCE)
set(PICO_DEFAULT_UART_RX_PIN 45 CACHE STRING "Diagnostic UART RX" FORCE)
if(NOT DEFINED PICO_SDK_PATH OR NOT DEFINED PICO_PIO_USB_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH and PICO_PIO_USB_PATH")
endif()
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(picoPlus4 C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(PLUS4_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PLUS4_CORE "${PLUS4_ROOT}/deps/plus4emu/src")
set(PLUS4_FIRMWARE "${PLUS4_ROOT}/firmware")
set(WAVE_ROOT "${PLUS4_ROOT}/platform/wave1")
set(PICO_SHARED_PATH "${PLUS4_ROOT}/deps/pico_shared" CACHE PATH "Pinned public pico_shared")
set(PLUS4_VERSION "0.4.14-fruit-jam-source-candidate")

add_library(plus4_ted STATIC
    "${PLUS4_CORE}/cpu.cpp" "${PLUS4_CORE}/cpuoptbl.cpp"
    "${PLUS4_CORE}/memory.cpp" "${PLUS4_CORE}/render.cpp"
    "${PLUS4_CORE}/ted_api.cpp" "${PLUS4_CORE}/ted_init.cpp"
    "${PLUS4_CORE}/ted_main.cpp" "${PLUS4_CORE}/ted_read.cpp"
    "${PLUS4_CORE}/ted_snd.cpp" "${PLUS4_CORE}/ted_write.cpp"
    "${PLUS4_FIRMWARE}/plus4_system_embedded.cpp")
target_include_directories(plus4_ted PUBLIC "${PLUS4_CORE}")
target_compile_definitions(plus4_ted PUBLIC
    PLUS4EMU_EMBEDDED_TED_FAST_MEMORY=1
    PLUS4EMU_EMBEDDED_TED_DIRECT_MEMORY=1
    PLUS4EMU_EMBEDDED_NO_CALLBACKS=1
    PLUS4EMU_EMBEDDED_NO_DEBUGGER=0
    PLUS4_HOT_CODE_SRAM=0
    PLUS4_CLOCK037=0 PLUS4_SPAN038=0 PLUS4_SPLIT039=0 PLUS4_RENDER032=0
    PLUS4_SHIFT034=1 PLUS4_SHIFT051=1 PLUS4_BITMAP053=1 PLUS4_RAM054=1)
target_compile_options(plus4_ted PRIVATE -ffunction-sections -fdata-sections
    -include "${PLUS4_FIRMWARE}/plus4_pthread_compat.h")
set_property(TARGET plus4_ted PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

set(JACK_WAVE1_HDMI_AUDIO ON)
set(JACK_WAVE1_HDMI_DRIVER_ROOT "${PLUS4_ROOT}/platform/hdmi")
set(JACK_WAVE1_HDMI_VIDEO_SOURCE "${PLUS4_ROOT}/platform/hdmi/video_output.c")
set(JACK_WAVE1_HDMI_ADAPTER "${PLUS4_FIRMWARE}/plus4_audio045_transport.c")
add_subdirectory("${WAVE_ROOT}" "${CMAKE_BINARY_DIR}/wave1")
target_sources(jack_wave1_runtime PRIVATE
    "${PLUS4_FIRMWARE}/plus4_audio045.cpp"
    "${PLUS4_FIRMWARE}/plus4_log045.cpp")
target_include_directories(jack_wave1_runtime PUBLIC "${PLUS4_FIRMWARE}")
target_compile_definitions(jack_wave1_runtime PUBLIC
    PLUS4_AUDIO045=1 PLUS4_AUDIO046=1 PLUS4_AUDIO045_SILENT_CONTROL=0
    JACK_DI_RING_CAPACITY=32 JACK_BYTEPUSHER_HSTX_DIAG_R12=1
    JACK_BYTEPUSHER_QUIET_CORE1_R15=1)
target_compile_definitions(jack_wave1_runtime PRIVATE
    JACK_WAVE1_USB_NONBLOCKING=1 JACK_WAVE1_QUIET_INPUT_LOG=1)

add_executable(picoPlus4
    "${PLUS4_FIRMWARE}/playable_platform.cpp"
    "${PLUS4_FIRMWARE}/profile050_sampler.cpp")
pico_set_program_name(picoPlus4 "picoPlus4")
pico_set_program_version(picoPlus4 "${PLUS4_VERSION}")
pico_enable_stdio_uart(picoPlus4 1)
pico_enable_stdio_usb(picoPlus4 0)
target_include_directories(picoPlus4 PRIVATE "${PLUS4_FIRMWARE}")
target_compile_definitions(picoPlus4 PRIVATE
    PLUS4_BUILD_VERSION="${PLUS4_VERSION}"
    PLUS4_RAM_IN_SRAM=1 PLUS4_DIRECT_MEMORY=1 PLUS4_NO_CALLBACKS=1
    PLUS4_NO_DEBUGGER=0 PLUS4_CORE_LTO=1 PLUS4_VIDEO031=1
    PLUS4_MANUAL041=1 PLUS4_MANUAL042=1 PLUS4_MANUAL043=1
    PLUS4_PROFILE050=1 PLUS4_PROFILE052=1
    HW_CONFIG=8 PICO_STACK_SIZE=4096 JACK_WAVE1_PERF_INTERVAL_US=1000000u)
target_link_libraries(picoPlus4 PRIVATE
    pico_stdlib hardware_psram hardware_exception plus4_ted jack_wave1_runtime)
target_link_options(picoPlus4 PRIVATE -Wl,--gc-sections -Wl,--print-memory-usage)
include("${PLUS4_ROOT}/cmake/LoaderPartition.cmake")
plus4_link_for_loader(picoPlus4)
pico_add_extra_outputs(picoPlus4)
