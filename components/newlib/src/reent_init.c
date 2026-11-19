// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/reent.h>
#include <sys/lock.h>
#include "esp_attr.h"
#include "esp_vfs_dev.h"

#define _STR(_s)    #_s
#define STR(_s)     _STR(_s)

/* espurna: newlib 4.3 (GCC 14.2): the global struct _reent is newlib's own _impure_data (_GLOBAL_REENT); newlib no
 * longer defines _global_impure_ptr, the platform does (as ESP-IDF 5) */
struct _reent *_global_impure_ptr;

/* This function is not part on newlib API, it is defined in libc/stdio/local.h
 * There is no nice way to get __cleanup member populated while avoiding __sinit,
 * so extern declaration is used here.
 */
extern void _cleanup_r(struct _reent* r);

/**
 * This is the replacement for newlib's _REENT_INIT_PTR and __sinit.
 * The problem with __sinit is that it allocates three FILE structures
 * (stdin, stdout, stderr). Having individual standard streams for each task
 * is a bit too much on a small embedded system. So we point streams
 * to the streams of the global struct _reent, which are initialized in
 * startup code.
 */
void esp_reent_init(struct _reent* r)
{
    memset(r, 0, sizeof(*r));
    r->_stdout = _GLOBAL_REENT->_stdout;
    r->_stderr = _GLOBAL_REENT->_stderr;
    r->_stdin  = _GLOBAL_REENT->_stdin;
#ifndef ESPURNA_NO_STDIO
    r->__cleanup = &_cleanup_r;
#endif
    _REENT_SDIDINIT(r) = 1;
}

/* only declared in private stdio header file, local.h */
extern void __sfp_lock_acquire(void);
extern void __sfp_lock_release(void);

void esp_reent_cleanup(void)
{
    struct _reent* r = __getreent();
    /* Clean up storage used by mprec functions */
    if (r->_mp) {
        if (_REENT_MP_FREELIST(r)) {
            for (int i = 0; i < _Kmax; ++i) {
                struct _Bigint *cur, *next;
                next = _REENT_MP_FREELIST(r)[i];
                while (next) {
                    cur = next;
                    next = next->_next;
                    free(cur);
                }
            }
        }
        free(_REENT_MP_FREELIST(r));
        free(_REENT_MP_RESULT(r));
    }

    /* Clean up "glue" (lazily-allocated FILE objects) */
    struct _glue* prev = &_REENT_SGLUE(_GLOBAL_REENT);
    for (struct _glue* cur = _REENT_SGLUE(_GLOBAL_REENT)._next; cur != NULL;) {
        if (cur->_niobs == 0) {
            cur = cur->_next;
            continue;
        }
        bool has_open_files = false;
        for (int i = 0; i < cur->_niobs; ++i) {
            FILE* fp = &cur->_iobs[i];
            if (fp->_flags != 0) {
                has_open_files = true;
                break;
            }
        }
        if (has_open_files) {
            prev = cur;
            cur = cur->_next;
            continue;
        }
        struct _glue* next = cur->_next;
        prev->_next = next;
        free(cur);
        cur = next;
    }

    /* Clean up various other buffers */
    free(r->_mp);
    r->_mp = NULL;
    free(r->_r48);
    r->_r48 = NULL;
    free(r->_localtime_buf);
    r->_localtime_buf = NULL;
    free(r->_asctime_buf);
    r->_asctime_buf = NULL;
}

/*
 * @brief Initialize newlib's platform object data
 */
int esp_newlib_init(void)
{
    const char *default_uart_dev = "/dev/uart/" STR(CONFIG_CONSOLE_UART_NUM);

#if defined(_RETARGETABLE_LOCKING)
    /* espurna: newlib 4.3 (GCC 14.2): the mutexes of its static locks first */
    esp_newlib_locks_init();
#endif

    memset(&__sglue, 0, sizeof(__sglue));
    _global_impure_ptr = _GLOBAL_REENT;
    esp_reent_init(_GLOBAL_REENT);

#ifndef ESPURNA_NO_STDIO
    esp_vfs_dev_uart_register();

    _GLOBAL_REENT->_stdout = fopen(default_uart_dev, "w");
    if (!_GLOBAL_REENT->_stdout)
        goto err;

    _GLOBAL_REENT->_stderr = fopen(default_uart_dev, "w");
    if (!_GLOBAL_REENT->_stderr)
        goto err_fail;

    _GLOBAL_REENT->_stdin = fopen(default_uart_dev, "r");
    if (!_GLOBAL_REENT->_stdin)
        goto err_in;
#else
    /* ESPurna: no stdio streams over the VFS UART driver. The SDK logs through ets_printf (ROM / esp_libc) and ESPurna
     * writes its own UART; printf / puts / putchar / assert go to ets_vprintf (ESPurna's arch/esp8266/stdio_esp8266.c),
     * the newlib FILE layer and the VFS stay out of the image */
    (void) default_uart_dev;
#endif

    environ = malloc(sizeof(char*));
    if (!environ)
        goto err_env;
    environ[0] = NULL;

    return 0;

err_env:
#ifndef ESPURNA_NO_STDIO
    fclose(_GLOBAL_REENT->_stdin);
err_in:
    fclose(_GLOBAL_REENT->_stderr);
err_fail:
    fclose(_GLOBAL_REENT->_stdout);
err:
#endif
    return -1;
}

struct _reent* __getreent()
{
    return _GLOBAL_REENT;
}