ESP8266_RTOS_SDK master 858c7c2e for ESPurna: FreeRTOS 11.1.0, esp-lwip 2.2.0 (fd432e4e), mbedTLS 3.6.6, ESPurna bootloader step. PlatformIO package framework-esp8266-rtos-sdk.

3.4.0-espurna.2: full FreeRTOS list end markers (configUSE_MINI_LIST_ITEM 0, the GCC 14 aliasing crash on Wi-Fi disconnect), WPA2/WPA3 scan authmode for mixed SAE/PSK APs, the negotiated AKM for ESPurna's STATUS, the supplicant builds with 802.11k/v, no temporary kernel diagnostics.

3.4.0-espurna.3: the release carries the package zip (with the lwip submodule), ESPurna points at it; the SDK is 3.4.0-espurna.2.

3.4.0-espurna.4: the package is the SDK COMPILED by the release (tools/espurna/build.py, as Espressif's lib-builder for Arduino-ESP32): archives, headers, sdkconfig, linker scripts and the flags of the app and of the bootloader (flags.json); the firmware only links it. The configuration is tools/espurna/sdkconfig.defaults. The bootloader calls the weak bootloader_after_init() (bootloader_hooks.h), the firmware links its own; no ESPurna code in the SDK.

3.4.0-espurna.5: the headers the build writes (the forwards of the freertos component to the kernel) included the kernel by the full path of the CI machine: the package now carries those headers and includes them by relative paths; build.py stops on a full path left in a header, its test app includes the kernel, Wi-Fi, lwIP and Mbed TLS headers.
