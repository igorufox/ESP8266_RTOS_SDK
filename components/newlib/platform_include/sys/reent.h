// espurna: newlib 4.3 of GCC 14.2 (regress-todo.md item 35). struct _reent has no __sdidinit / __sglue any more: the
// "stdio initialized" flag is the _reserved_0 of the binary compatible layout (newlib built with
// --enable-newlib-reent-binary-compat, sys/config.h defines _REENT_BACKWARD_BINARY_COMPAT) and the FILE glue is one
// global __sglue, as ESP-IDF 5 has it (components/newlib/platform_include/sys/reent.h)
#pragma once

#define _REENT_SDIDINIT(_ptr)  ((_ptr)->_reserved_0)
#define _REENT_SGLUE(_ptr)     (__sglue)

#include_next <sys/reent.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void __sinit(struct _reent *);

extern struct _glue __sglue;
extern struct _reent *_global_impure_ptr;

#ifdef __cplusplus
}
#endif
