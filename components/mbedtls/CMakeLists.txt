# ESPurna: mbedTLS 3.6.6 of the one mbedTLS package (framework-mbedtls, MBEDTLS_DIR given by scripts/esp8266_rtos_sdk.py,
# the version in code/libraries.ini): the same source tree the ESP32 (ESP-IDF 5.5) and the Beken BDK build. The SDK's
# 2.16.5 submodule and its 2.x port (AES/SHA/MD5/ARC4 *_ALT, dynamic buffers, net_sockets, debug) are gone: the code
# of mbedtls itself, configured by port/include/mbedtls/esp_config.h (sdkconfig's CONFIG_MBEDTLS_* still apply).
if(NOT MBEDTLS_DIR)
    # the requirements pass of the build runs this file in a script of its own, without the cache: the environment
    set(MBEDTLS_DIR "$ENV{MBEDTLS_DIR}")
endif()
if(NOT MBEDTLS_DIR AND NOT CMAKE_BUILD_EARLY_EXPANSION)
    message(FATAL_ERROR "MBEDTLS_DIR: the mbedTLS package (framework-mbedtls) is not given")
endif()

file(GLOB mbedtls_srcs "${MBEDTLS_DIR}/library/*.c")
list(FILTER mbedtls_srcs EXCLUDE REGEX "/(net_sockets|psa_its_file)\.c$")

idf_component_register(SRCS ${mbedtls_srcs}
                            "port/esp_hardware.c"
                            "port/esp_mem.c"
                            "port/esp_platform_time.c"
                    INCLUDE_DIRS "port/include" "${MBEDTLS_DIR}/include"
                    PRIV_INCLUDE_DIRS "${MBEDTLS_DIR}/library"
                    PRIV_REQUIRES esp8266)

target_compile_definitions(${COMPONENT_LIB} PUBLIC -DMBEDTLS_CONFIG_FILE="mbedtls/esp_config.h" -DCONFIG_SSL_USING_MBEDTLS)
