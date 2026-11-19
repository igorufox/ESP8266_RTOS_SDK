/*
 * ESPurna: the ESP8266 port's additions to FreeRTOS-Kernel V11 tasks.c (configINCLUDE_FREERTOS_TASK_C_ADDITIONS_H),
 * included at the end of tasks.c: what the SDK's 10.0.1 kernel had in its own patched tasks.c, now without touching the
 * kernel source (the one kernel package of the three chips).
 *
 * The thread local storage delete callbacks: configNUM_THREAD_LOCAL_STORAGE_POINTERS is twice portTLS_POINTERS, the
 * pointers first, their callbacks in the slots after them.
 */

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != ( 2 * portTLS_POINTERS ) )
    #error "configNUM_THREAD_LOCAL_STORAGE_POINTERS: the pointers and their delete callbacks (FreeRTOSConfig.h)"
#endif

void vTaskSetThreadLocalStoragePointerAndDelCallback( TaskHandle_t xTaskToSet, BaseType_t xIndex, void * pvValue,
                                                      TlsDeleteCallbackFunction_t pvDelCallback )
{
    TCB_t * pxTCB;

    if( ( xIndex >= 0 ) && ( xIndex < portTLS_POINTERS ) )
    {
        taskENTER_CRITICAL();
        pxTCB = prvGetTCBFromHandle( xTaskToSet );
        pxTCB->pvThreadLocalStoragePointers[ xIndex ] = pvValue;
        pxTCB->pvThreadLocalStoragePointers[ portTLS_POINTERS + xIndex ] = ( void * ) pvDelCallback;
        taskEXIT_CRITICAL();
    }
}

void ** pvTaskGetThreadLocalStorageBufferPointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex )
{
    TCB_t * pxTCB;

    if( ( xIndex >= 0 ) && ( xIndex < portTLS_POINTERS ) )
    {
        pxTCB = prvGetTCBFromHandle( xTaskToQuery );
        return &pxTCB->pvThreadLocalStoragePointers[ xIndex ];
    }

    return NULL;
}

#if ( configUSE_TICKLESS_IDLE != 0 )
    TickType_t xPortGetExpectedIdleTime( void )
    {
        return prvGetExpectedIdleTime();
    }
#endif


/* ESPurna: os_cpu_a.S puts the end-of-stack watchpoint on the word at offset 48 of the TCB (pxStack); a TCB of another
   layout (list integrity bytes, other config) watches some other field and every task "overflows" */
#include <stddef.h>
_Static_assert( offsetof( TCB_t, pxStack ) == 48, "os_cpu_a.S: pxStack is not at offset 48 of the TCB" );
