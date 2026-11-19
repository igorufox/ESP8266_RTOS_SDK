set(CMAKE_SYSTEM_NAME Generic)

# espurna: GCC 14.2, the one compiler of the three chips (regress-todo.md item 35). Espressif's xtensa-esp-elf 14.2
# compiles for the ESP8266 core with its xtensa_esp8266 configuration (the environment's XTENSA_GNU_CONFIG, set by
# scripts/esp8266_rtos_sdk.py: -mdynconfig= with a full path does not load on Windows); the target libraries of the
# lx106 (call0 ABI: libgcc, newlib and newlib-nano, libstdc++) and their headers come from the package of our build
# (ESPURNA_LX106_LIBS, toolchain-xtensa-lx106-libs), not from the esp32 multilibs of the compiler's package
set(CMAKE_C_COMPILER xtensa-esp-elf-gcc)
set(CMAKE_CXX_COMPILER xtensa-esp-elf-g++)
set(CMAKE_ASM_COMPILER xtensa-esp-elf-gcc)

if(NOT DEFINED ENV{ESPURNA_LX106_LIBS})
    message(FATAL_ERROR "ESPURNA_LX106_LIBS: the folder of the lx106 target libraries (toolchain-xtensa-lx106-libs)")
endif()
file(TO_CMAKE_PATH "$ENV{ESPURNA_LX106_LIBS}" LX106_LIBS)
set(LX106_TARGET "${LX106_LIBS}/xtensa-lx106-elf")
set(LX106_CXX "${LX106_TARGET}/include/c++/14.2.0")

# -isystem joined to its folder: scripts/esp8266_rtos_sdk.py takes the SDK's flags for ESPurna's code token by token
set(LX106_C_HEADERS "-isystem${LX106_TARGET}/include")
set(LX106_CXX_HEADERS "-nostdinc++ -isystem${LX106_CXX} -isystem${LX106_CXX}/xtensa-lx106-elf -isystem${LX106_TARGET}/include")

# (int32_t / uint32_t of xtensa-esp-elf are long, GCC 8.4 lx106: int - tools/cmake/build.cmake keeps the SDK's %d of a
# uint32_t a warning)
set(LX106_WARNINGS "-Wno-frame-address")
set(CMAKE_C_FLAGS "-mlongcalls ${LX106_WARNINGS} ${LX106_C_HEADERS}" CACHE STRING "C Compiler Base Flags")
set(CMAKE_CXX_FLAGS "-mlongcalls ${LX106_WARNINGS} ${LX106_CXX_HEADERS}" CACHE STRING "C++ Compiler Base Flags")
# no -mlongcalls for the assembler (as the SDK had it): the exception vectors are fixed-size slots, a call0 widened
# to l32r + callx0 overflowed .DebugExceptionVector (16 bytes)
set(CMAKE_ASM_FLAGS "${LX106_C_HEADERS}" CACHE STRING "ASM Compiler Base Flags")

# Can be removed after gcc 5.2.0 support is removed (ref GCC_NOT_5_2_0)
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -Wl,--gc-sections -L${LX106_TARGET}/lib -L${LX106_LIBS}/lib/gcc/xtensa-lx106-elf/14.2.0" CACHE STRING "Linker Base Flags")
