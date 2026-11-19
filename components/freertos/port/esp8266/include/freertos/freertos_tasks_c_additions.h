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


/* DIAG 11-18 (ESPurna, temporary): the nesting of vTaskSuspendAll(), for port.c vPortTraceBlocking() */
UBaseType_t uxPortSchedulerSuspended( void )
{
    return uxSchedulerSuspended;
}

/* DIAG 11-19 (ESPurna, temporary, regress-todo.md items 3/17): the invariants of the event list items (FreeRTOSConfig.h
   traceENTER_* hooks). A task blocks with its event item in no list; the head of an event list belongs to that list; a
   task on the pending ready list has its event item there and its state item in a delayed or the suspended list. The
   crash of 11-18 (xTaskResumeAll, pending item with no container) breaks one of these earlier - the first one found
   prints and aborts, so the panic backtrace shows the path that broke it */
extern int ets_printf( const char * fmt, ... );
extern int xPortInIsrContext( void );
extern UBaseType_t uxPortCriticalNesting( void );
extern uint8_t NMIIrqIsOn;
extern void abort( void );

static void prvPortListBroken( const char * what, const TCB_t * pxTCB, const void * pxList, const void * pxContainer,
                               void * caller )
{
    portDISABLE_INTERRUPTS();
    ets_printf( "DIAG list %s: task %s tcb %p list %p container %p (pending %p delayed %p/%p suspended %p) "
                "current %s susp %u crit %u isr %d nmi %d from %p\n",
                what, pxTCB ? pxTCB->pcTaskName : "-", pxTCB, pxList, pxContainer, &xPendingReadyList,
                &xDelayedTaskList1, &xDelayedTaskList2, &xSuspendedTaskList,
                pxCurrentTCB ? pxCurrentTCB->pcTaskName : "-", ( unsigned ) uxSchedulerSuspended,
                ( unsigned ) uxPortCriticalNesting(), xPortInIsrContext(), NMIIrqIsOn, caller );
    abort();
}

void vPortCheckPlaceOnEventList( const char * what, void * pxEventList, void * caller )
{
    const void * pxContainer = listLIST_ITEM_CONTAINER( &( pxCurrentTCB->xEventListItem ) );

    if( pxContainer != NULL )
    {
        prvPortListBroken( what, pxCurrentTCB, pxEventList, pxContainer, caller );
    }

    /* blocking with the interrupts closed: the yield cannot happen, the queue functions loop and place the task on the
       event list once more (that is the check above, on the next pass) */
    if( uxPortCriticalNesting() != 0U )
    {
        prvPortListBroken( "place in critical", pxCurrentTCB, pxEventList, pxContainer, caller );
    }
}

void vPortCheckRemoveFromEventList( void * pxEventList, void * caller )
{
    List_t * const pxList = ( List_t * ) pxEventList;

    if( listLIST_IS_EMPTY( pxList ) != pdFALSE )
    {
        return;
    }

    const TCB_t * const pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxList );

    if( ( pxTCB == NULL ) || ( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != pxList ) )
    {
        prvPortListBroken( "remove", pxTCB, pxList,
                           pxTCB ? listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) : NULL, caller );
    }
}

void vPortCheckPendingReady( void * caller )
{
    taskENTER_CRITICAL();
    {
        const ListItem_t * pxItem = listGET_HEAD_ENTRY( &xPendingReadyList );
        const ListItem_t * const pxEnd = listGET_END_MARKER( &xPendingReadyList );
        UBaseType_t uxCount = 0U;

        while( pxItem != pxEnd )
        {
            const TCB_t * const pxTCB = listGET_LIST_ITEM_OWNER( pxItem );

            if( ( pxTCB == NULL ) || ( pxItem != &( pxTCB->xEventListItem ) ) ||
                ( listLIST_ITEM_CONTAINER( pxItem ) != &xPendingReadyList ) )
            {
                prvPortListBroken( "pending", pxTCB, &xPendingReadyList, listLIST_ITEM_CONTAINER( pxItem ), caller );
            }

            if( listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) ) == NULL )
            {
                prvPortListBroken( "pending state", pxTCB, &xPendingReadyList, NULL, caller );
            }

            if( ++uxCount > listCURRENT_LIST_LENGTH( &xPendingReadyList ) )
            {
                prvPortListBroken( "pending length", pxTCB, &xPendingReadyList, listLIST_ITEM_CONTAINER( pxItem ), caller );
            }

            pxItem = listGET_NEXT( pxItem );
        }

        if( uxCount != listCURRENT_LIST_LENGTH( &xPendingReadyList ) )
        {
            prvPortListBroken( "pending count", NULL, &xPendingReadyList, NULL, caller );
        }
    }
    taskEXIT_CRITICAL();
}

/* ESPurna: os_cpu_a.S puts the end-of-stack watchpoint on the word at offset 48 of the TCB (pxStack); a TCB of another
   layout (list integrity bytes, other config) watches some other field and every task "overflows" */
#include <stddef.h>
_Static_assert( offsetof( TCB_t, pxStack ) == 48, "os_cpu_a.S: pxStack is not at offset 48 of the TCB" );
