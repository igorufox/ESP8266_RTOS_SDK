// The default hooks of the bootloader (bootloader_hooks.h): nothing. A file of their own, so that the call in
// bootloader_start.c always goes through the symbol the firmware may define
#include "bootloader_hooks.h"

void __attribute__((weak)) bootloader_after_init(void)
{
}
