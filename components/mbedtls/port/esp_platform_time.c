/*
 * ESPurna: mbedtls_ms_time() of mbedTLS 3.x (MBEDTLS_HAVE_TIME): the newlib of the lx106 is not "unix-like" for
 * mbedtls' own one. The microseconds since the boot of esp_timer, a monotonic clock as 3.x asks for.
 */

#include "mbedtls/build_info.h"
#include "mbedtls/platform_time.h"

#include "esp_timer.h"

#ifdef MBEDTLS_PLATFORM_MS_TIME_ALT
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t) (esp_timer_get_time() / 1000);
}
#endif
