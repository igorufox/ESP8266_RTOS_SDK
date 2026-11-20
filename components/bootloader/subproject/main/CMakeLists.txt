# bootloader_hooks.c: the weak hooks of bootloader_hooks.h, the firmware that links the bootloader defines its own
idf_component_register(SRCS "bootloader_start.c" "bootloader_hooks.c"
                    INCLUDE_DIRS "."
                    REQUIRES bootloader bootloader_support spi_flash)

idf_build_get_property(target IDF_TARGET)
set(scripts "${target}.bootloader.ld"
            "${target}.bootloader.rom.ld"
            "${ESP8266_BOOTLOADER_LINKER_SCRIPTS}")

target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")

target_link_libraries(${COMPONENT_LIB} INTERFACE "${ESP8266_BOOTLOADER_LIBS}")

# espurna: GCC 14.2 calls libgcc where GCC 8.4 expanded inline (__bswapsi2 of __builtin_bswap32 in bootloader_flash.c):
# the lx106 libgcc of the toolchain package (the -L of tools/cmake/toolchain-esp8266.cmake)
target_link_libraries(${COMPONENT_LIB} INTERFACE gcc)
