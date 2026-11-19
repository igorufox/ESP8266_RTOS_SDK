// espurna: newlib 4.3 of GCC 14.2 (regress-todo.md item 35) is built with _RETARGETABLE_LOCKING and has no ESP-specific
// sys/lock.h any more; the SDK's code and its src/locks.c use the legacy _lock_t API. As ESP-IDF 5 does
// (components/newlib/platform_include/sys/lock.h): the platform's struct __lock (a FreeRTOS mutex fits in it, checked
// in src/locks.c) and _lock_t as the newlib's _LOCK_T, a pointer to it
#pragma once

#include_next <sys/lock.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_RETARGETABLE_LOCKING)

// a StaticSemaphore_t of FreeRTOS-Kernel V11 on the ESP8266 (with the trace facility): checked by a static assertion
struct __lock {
    int reserved[24];
};

typedef _LOCK_T _lock_t;

void _lock_init(_lock_t *plock);
void _lock_init_recursive(_lock_t *plock);
void _lock_close(_lock_t *plock);
void _lock_close_recursive(_lock_t *plock);
void _lock_acquire(_lock_t *plock);
void _lock_acquire_recursive(_lock_t *plock);
int _lock_try_acquire(_lock_t *plock);
int _lock_try_acquire_recursive(_lock_t *plock);
void _lock_release(_lock_t *plock);
void _lock_release_recursive(_lock_t *plock);

// the two mutexes behind newlib's static locks, before anything of newlib locks (esp_newlib_init())
void esp_newlib_locks_init(void);

#endif // _RETARGETABLE_LOCKING

#ifdef __cplusplus
}
#endif
