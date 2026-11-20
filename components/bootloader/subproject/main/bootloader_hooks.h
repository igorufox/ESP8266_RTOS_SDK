// Hooks of the bootloader for the firmware that links it (as ESP-IDF's bootloader_hooks.h): weak and empty here, the
// firmware's own definition replaces one when its object is linked before the bootloader's libraries
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// right after the hardware is initialized (bootloader_init), before the image to boot is chosen
void bootloader_after_init(void);

#ifdef __cplusplus
}
#endif
