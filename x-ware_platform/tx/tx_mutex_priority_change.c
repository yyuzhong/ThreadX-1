/**************************************************************************/ 
/*                                                                        */ 
/*            Copyright (c) 1996-2015 by Express Logic Inc.               */ 
/*                                                                        */ 
/*  This software is copyrighted by and is the sole property of Express   */ 
/*  Logic, Inc.  All rights, title, ownership, or other interests         */ 
/*  in the software remain the property of Express Logic, Inc.  This      */ 
/*  software may only be used in accordance with the corresponding        */ 
/*  license agreement.  Any unauthorized use, duplication, transmission,  */ 
/*  distribution, or disclosure of this software is expressly forbidden.  */ 
/*                                                                        */
/*  This Copyright notice may not be removed or modified without prior    */ 
/*  written consent of Express Logic, Inc.                                */ 
/*                                                                        */ 
/*  Express Logic, Inc. reserves the right to modify this software        */ 
/*  without notice.                                                       */ 
/*                                                                        */ 
/*  Express Logic, Inc.                     info@expresslogic.com         */
/*  11423 West Bernardo Court               www.expresslogic.com          */
/*  San Diego, CA  92127                                                  */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */ 
/** ThreadX Component                                                     */ 
/**                                                                       */
/**   Mutex                                                               */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define TX_SOURCE_CODE


/* Include necessary system files.  */

#include "tx_api.h"
#include "tx_thread.h"
#include "tx_mutex.h"


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _tx_mutex_priority_change                           PORTABLE C      */ 
/*                                                           5.7          */ 
/*  AUTHOR                                                                */ 
/*                                                                        */ 
/*    William E. Lamie, Express Logic, Inc.                               */ 
/*                                                                        */ 
/*  DESCRIPTION                                                           */ 
/*                                                                        */ 
/*    This function changes the priority of the specified thread for the  */ 
/*    priority inheritance option of the mutex service.                   */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    thread_ptr                            Pointer to thread to suspend  */ 
/*    new_priority                          New thread priority           */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    None                                                                */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _tx_thread_system_resume          Resume thread                     */ 
/*    _tx_thread_system_ni_resume       Non-interruptable resume thread   */ 
/*    _tx_thread_system_suspend         Suspend thread                    */ 
/*    _tx_thread_system_ni_suspend      Non-interruptable suspend thread  */ 
/*                                                                        */ 
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    _tx_mutex_get                     Inherit priority                  */ 
/*    _tx_mutex_put                     Restore previous priority         */ 
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  12-12-2005     William E. Lamie         Initial Version 5.0           */ 
/*  04-02-2007     William E. Lamie         Modified comment(s),          */ 
/*                                            resulting in version 5.1    */ 
/*  12-12-2008     William E. Lamie         Modified comment(s), added    */ 
/*                                            optional logic for          */ 
/*                                            non-interruptable operation,*/ 
/*                                            and made several            */ 
/*                                            optimizations, resulting    */ 
/*                                            in version 5.2              */ 
/*  07-04-2009     William E. Lamie         Modified comment(s),          */ 
/*                                            resulting in version 5.3    */ 
/*  12-12-2009     William E. Lamie         Modified comment(s), and      */ 
/*                                            changed priority-inheritance*/ 
/*                                            logic to distinguish user   */ 
/*                                            priority changes from mutex */ 
/*                                            priority-inheritance,       */ 
/*                                            resulting in version 5.4    */ 
/*  07-15-2011     William E. Lamie         Modified comment(s), removed  */ 
/*                                            unused new threshold        */ 
/*                                            parameter, and added logic  */ 
/*                                            to handle a lower or equal  */ 
/*                                            priority thread from        */ 
/*                                            preempting an executing     */ 
/*                                            thread changing priority,   */ 
/*                                            resulting in version 5.5    */ 
/*  11-01-2012     William E. Lamie         Modified comment(s), removed  */ 
/*                                            unnecessary code, and       */ 
/*                                            added logic to place thread */ 
/*                                            at the front of the         */ 
/*                                            execution list at updated   */ 
/*                                            priority, resulting in      */ 
/*                                            version 5.6                 */ 
/*  05-01-2015     William E. Lamie         Modified comment(s), and      */ 
/*                                            modified code for MISRA     */ 
/*                                            compliance, resulting in    */ 
/*                                            version 5.7                 */ 
/*                                                                        */ 
/**************************************************************************/ 
VOID  _tx_mutex_priority_change(TX_THREAD *thread_ptr, UINT new_priority)
{

#ifndef TX_NOT_INTERRUPTABLE

TX_INTERRUPT_SAVE_AREA
#endif

TX_THREAD       *execute_ptr;
TX_THREAD       *next_execute_ptr;
UINT            original_priority;
#ifndef TX_DISABLE_PREEMPTION_THRESHOLD
ULONG           priority_bit;
#if TX_MAX_PRIORITIES > 32
UINT            map_index;
#endif
#endif



#ifndef TX_NOT_INTERRUPTABLE

    /* Lockout interrupts while the thread is being suspended.  */
    TX_DISABLE
#endif

    /* Remember the new priority inheritance priority.  */
    thread_ptr -> tx_thread_inherit_priority =  new_priority;

    /* Determine if this thread is currently ready.  */
    if (thread_ptr -> tx_thread_state != TX_READY)
    {

        /* Change thread priority to the new mutex priority-inheritance priority.  */
        thread_ptr -> tx_thread_priority =  new_priority;
            
        /* Determine how to setup the thread's preemption-threshold.  */
        if (thread_ptr -> tx_thread_user_preempt_threshold < new_priority)
        {
        
            /* Change thread preemption-threshold to the user's preemption-threshold.  */
            thread_ptr -> tx_thread_preempt_threshold =  thread_ptr -> tx_thread_user_preempt_threshold;
        }
        else
        {
        
            /* Change the thread preemption-threshold to the new threshold.  */
            thread_ptr -> tx_thread_preempt_threshold =  new_priority;
        }

#ifndef TX_NOT_INTERRUPTABLE
        /* Restore interrupts.  */
        TX_RESTORE
#endif
    }
    else
    {

        /* Pickup the next thread to execute.  */
        execute_ptr =  _tx_thread_execute_ptr;

        /* Save the original priority.  */
        original_priority =  thread_ptr -> tx_thread_priority;

#ifdef TX_NOT_INTERRUPTABLE

        /* Increment the preempt disable flag.  */
        _tx_thread_preempt_disable++;

        /* Set the state to suspended.  */
        thread_ptr -> tx_thread_state =    TX_SUSPENDED;

        /* Call actual non-interruptable thread suspension routine.  */
        _tx_thread_system_ni_suspend(thread_ptr, ((ULONG) 0));

        /* At this point, the preempt disable flag is still set, so we still have 
           protection against all preemption.  */

        /* Change thread priority to the new mutex priority-inheritance priority.  */
        thread_ptr -> tx_thread_priority =  new_priority;
            
        /* Determine how to setup the thread's preemption-threshold.  */
        if (thread_ptr -> tx_thread_user_preempt_threshold < new_priority)
        {
        
            /* Change thread preemption-threshold to the user's preemption-threshold.  */
            thread_ptr -> tx_thread_preempt_threshold =  thread_ptr -> tx_thread_user_preempt_threshold;
        }
        else
        {
        
            /* Change the thread preemption-threshold to the new threshold.  */
            thread_ptr -> tx_thread_preempt_threshold =  new_priority;
        }

        /* Resume the thread with the new priority.  */
        _tx_thread_system_ni_resume(thread_ptr);

        /* Decrement the preempt disable flag.  */
        _tx_thread_preempt_disable--;
#else

        /* Increment the preempt disable flag.  */
        _tx_thread_preempt_disable =  _tx_thread_preempt_disable + ((UINT) 2);

        /* Set the state to suspended.  */
        thread_ptr -> tx_thread_state =    TX_SUSPENDED;

        /* Set the suspending flag. */
        thread_ptr -> tx_thread_suspending =  TX_TRUE;

        /* Setup the timeout period.  */
        thread_ptr -> tx_thread_timer.tx_timer_internal_remaining_ticks =  ((ULONG) 0);

        /* Restore interrupts.  */
        TX_RESTORE 

        /* The thread is ready and must first be removed from the list.  Call the 
           system suspend function to accomplish this.  */
        _tx_thread_system_suspend(thread_ptr);

        /* Disable interrupts.  */
        TX_DISABLE

        /* At this point, the preempt disable flag is still set, so we still have 
           protection against all preemption.  */

        /* Change thread priority to the new mutex priority-inheritance priority.  */
        thread_ptr -> tx_thread_priority =  new_priority;
            
        /* Determine how to setup the thread's preemption-threshold.  */
        if (thread_ptr -> tx_thread_user_preempt_threshold < new_priority)
        {
        
            /* Change thread preemption-threshold to the user's preemption-threshold.  */
            thread_ptr -> tx_thread_preempt_threshold =  thread_ptr -> tx_thread_user_preempt_threshold;
        }
        else
        {
        
            /* Change the thread preemption-threshold to the new threshold.  */
            thread_ptr -> tx_thread_preempt_threshold =  new_priority;
        }

        /* Restore interrupts.  */
        TX_RESTORE

        /* Resume the thread with the new priority.  */
        _tx_thread_system_resume(thread_ptr);
#endif

#ifndef TX_NOT_INTERRUPTABLE

        /* Disable interrupts.  */
        TX_DISABLE
#endif
        
        /* Pickup the next thread to execute.  */
        next_execute_ptr =  _tx_thread_execute_ptr;

        /* Determine if this thread is not the next thread to execute.  */
        if (thread_ptr != next_execute_ptr)
        {
        
            /* Make sure the thread is still ready.  */
            if (thread_ptr -> tx_thread_state == TX_READY)
            {

                /* Now check and see if this thread has an equal or higher priority.  */
                if (thread_ptr -> tx_thread_priority <= next_execute_ptr -> tx_thread_priority)
                {

                    /* Now determine if this thread was the previously executing thread.  */
                    if (thread_ptr == execute_ptr)
                    {

                        /* Yes, this thread was previously executing before we temporarily suspended and resumed
                           it in order to change the priority. A lower or same priority thread cannot be the next thread
                           to execute in this case since this thread really didn't suspend.  Simply reset the execute
                           pointer to this thread.  */
                        _tx_thread_execute_ptr =  thread_ptr;

                        /* Determine if we moved to a lower priority. If so, move the thread to the front of its priority list.  */
                        if (original_priority < new_priority)
                        {
                        
                            /* Ensure that this thread is placed at the front of the priority list.  */
                            _tx_thread_priority_list[thread_ptr -> tx_thread_priority] =  thread_ptr;
                        }
                    }
                }
                else
                {

                    /* Now determine if this thread's preemption-threshold needs to be enforced.  */
                    if (thread_ptr -> tx_thread_preempt_threshold < thread_ptr -> tx_thread_priority)
                    {

                        /* Yes, preemption-threshold is in force for this thread. */ 

                        /* Compare the next thread to execute thread's priority against the thread's preemption-threshold.  */
                        if (thread_ptr -> tx_thread_preempt_threshold <= next_execute_ptr -> tx_thread_priority)
                        {
                
                            /* We must swap execute pointers to enforce the preemption-threshold of a thread coming out of
                               priority inheritance.  */
                            _tx_thread_execute_ptr =  thread_ptr;

                            /* Determine if we moved to a lower priority. If so, move the thread to the front of its priority list.  */
                            if (original_priority < new_priority)
                            {
                        
                                /* Ensure that this thread is placed at the front of the priority list.  */
                                _tx_thread_priority_list[thread_ptr -> tx_thread_priority] =  thread_ptr;
                            }
                        }

#ifndef TX_DISABLE_PREEMPTION_THRESHOLD

                        else
                        {

                            /* In this case, we need to mark the preempted map to indicate a thread executed above the
                               preemption-threshold.  */

#if TX_MAX_PRIORITIES > 32

                            /* Calculate the index into the bit map array.  */
                            map_index =  (next_execute_ptr -> tx_thread_priority)/ ((UINT) 32);

                            /* Set the active bit to remember that the preempt map has something set.  */
                            TX_DIV32_BIT_SET(next_execute_ptr -> tx_thread_priority, priority_bit)
                            _tx_thread_preempted_map_active =  _tx_thread_preempted_map_active | priority_bit;
#endif

                            /* Remember that this thread was preempted by a thread above the thread's threshold.  */
                            TX_MOD32_BIT_SET(next_execute_ptr -> tx_thread_priority, priority_bit)
                            _tx_thread_preempted_maps[MAP_INDEX] =  _tx_thread_preempted_maps[MAP_INDEX] | priority_bit;
                        }
#endif                
                    }
                }
            }
        }

#ifndef TX_NOT_INTERRUPTABLE
        
        /* Restore interrupts.  */
        TX_RESTORE
#endif
    }
}

