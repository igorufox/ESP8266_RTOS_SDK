// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <reent.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

#include "FreeRTOS.h"
#include "task.h"

#include "esp_libc.h"
#include "esp_log.h"
#include "esp_vfs.h"

/* ESPurna (FreeRTOS-Kernel V11): the last of the SDK's portTLS_POINTERS pointers (0 lwIP, 1 pthread, 2 errno); the
   pointers after them hold their delete callbacks (freertos_tasks_c_additions.h) */
#define ERRNO_TLS_INDEX (portTLS_POINTERS - 1)

void *_malloc_r(struct _reent *r, size_t n)
{
    void *return_addr = (void *)__builtin_return_address(0);

    return _heap_caps_malloc(n, MALLOC_CAP_32BIT, return_addr, 0);
}

void *_realloc_r(struct _reent *r, void *old_ptr, size_t n)
{
    void *return_addr = (void *)__builtin_return_address(0);

    return _heap_caps_realloc(old_ptr, n, MALLOC_CAP_32BIT, return_addr, 0);
}

void *_calloc_r(struct _reent *r, size_t c, size_t s)
{
    void *return_addr = (void *)__builtin_return_address(0);

    return _heap_caps_calloc(c, s, MALLOC_CAP_32BIT, return_addr, 0);
}

void _free_r(struct _reent *r, void *ptr)
{
    void *return_addr = (void *)__builtin_return_address(0);

    _heap_caps_free(ptr, return_addr, 0);
}

void abort(void)
{
#ifndef CONFIG_ESP_PANIC_SILENT_REBOOT
    ets_printf("abort() was called at PC %p on core %d\r\n", __builtin_return_address(0) - 3, xPortGetCoreID());
#endif

    /* cause a exception to jump into panic function */
    while (1) {
        *((int *)NULL) = 0;
    }
}

void _exit(int status)
{
    abort();
}

void *_sbrk_r(struct _reent *r, ptrdiff_t incr)
{
    abort();
}

int _getpid_r(struct _reent *r)
{
    __errno_r(r) = ENOSYS;
    return -1;
}

/* espurna: newlib 4.3 (GCC 14.2) keeps _getpid_r and _kill_r in one member (signalr.o): the SDK's own _kill_r as well,
 * else abort() / raise() pulled that member and _getpid_r was defined twice (as ESP-IDF 5 has both) */
int _kill_r(struct _reent *r, int pid, int sig)
{
    __errno_r(r) = ENOSYS;
    return -1;
}

/* espurna: newlib 4.3 asks it for the buffering of a FILE (its own _isatty_r wants an _isatty): the console is 0..2 */
int _isatty_r(struct _reent *r, int fd)
{
    if ((fd >= 0) && (fd <= 2)) {
        return 1;
    }

    __errno_r(r) = ENOTTY;
    return 0;
}

int *__errno(void)
{
    return (int *)pvTaskGetThreadLocalStorageBufferPointer(NULL, ERRNO_TLS_INDEX);
}
