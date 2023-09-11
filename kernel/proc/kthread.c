/******************************************************************************/
/* Important Fall 2022 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "config.h"
#include "globals.h"

#include "errno.h"

#include "util/init.h"
#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"

#include "mm/slab.h"
#include "mm/page.h"

kthread_t *curthr; /* global */
static slab_allocator_t *kthread_allocator = NULL;

#ifdef __MTP__
/* Stuff for the reaper daemon, which cleans up dead detached threads */
static proc_t *reapd = NULL;
static kthread_t *reapd_thr = NULL;
static ktqueue_t reapd_waitq;
static list_t kthread_reapd_deadlist; /* Threads to be cleaned */

static void *kthread_reapd_run(int arg1, void *arg2);
#endif

void
kthread_init()
{
        kthread_allocator = slab_allocator_create("kthread", sizeof(kthread_t));
        KASSERT(NULL != kthread_allocator);
}

/**
 * Allocates a new kernel stack.
 *
 * @return a newly allocated stack, or NULL if there is not enough
 * memory available
 */
static char *
alloc_stack(void)
{
        /* extra page for "magic" data */
        char *kstack;
        int npages = 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT);
        kstack = (char *)page_alloc_n(npages);

        return kstack;
}

/**
 * Frees a stack allocated with alloc_stack.
 *
 * @param stack the stack to free
 */
static void
free_stack(char *stack)
{
        page_free_n(stack, 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT));
}

void
kthread_destroy(kthread_t *t)
{
        KASSERT(t && t->kt_kstack);
        free_stack(t->kt_kstack);
        if (list_link_is_linked(&t->kt_plink))
                list_remove(&t->kt_plink);

        slab_obj_free(kthread_allocator, t);
}

/*
 * Allocate a new stack with the alloc_stack function. The size of the
 * stack is DEFAULT_STACK_SIZE.
 *
 * Don't forget to initialize the thread context with the
 * context_setup function. The context should have the same pagetable
 * pointer as the process.
 */
kthread_t *
kthread_create(struct proc *p, kthread_func_t func, long arg1, void *arg2)
{
        
        KASSERT(NULL != p); /* the p argument of this function must be a valid process */
        dbg(DBG_PRINT, "(GRADING1A 3.a)\n");
        
        kthread_t *newThrd;
        newThrd = slab_obj_alloc(kthread_allocator);
        KASSERT(newThrd !=NULL);

        newThrd->kt_proc = p;                       /* the thread's process */
        newThrd->kt_cancelled=0;                    /* Initialize the cancell flag*/
        
        char* thr_stack = alloc_stack();
        newThrd->kt_kstack= thr_stack;          /* the thread's stack */
        newThrd->kt_state = KT_RUN;                 /* the thread's state */           
        list_init(&newThrd->kt_qlink);              /* Init thread's link on ktqueue */ 
        list_init(&newThrd->kt_plink);              /* Init thread's link on kt_plink (process) */
        newThrd->kt_wchan = NULL;        /* Init thread's wchan queue */
        list_insert_tail(&p->p_threads, &newThrd->kt_plink);    /* Append p_threads list of process */
        context_setup(&(newThrd->kt_ctx), func, (int)arg1, arg2, (void*)thr_stack, DEFAULT_STACK_SIZE, p->p_pagedir);    /* setup thread's context */
        
        dbg(DBG_PRINT, "(GRADING1Cc)\n");

        return newThrd;
}

/*
 * If the thread to be cancelled is the current thread, this is
 * equivalent to calling kthread_exit. Otherwise, the thread is
 * sleeping (either on a waitqueue or a runqueue) 
 * and we need to set the cancelled and retval fields of the
 * thread. On wakeup, threads should check their cancelled fields and
 * act accordingly. 
 *
 * If the thread's sleep is cancellable, cancelling the thread should
 * wake it up from sleep.
 *
 * If the thread's sleep is not cancellable, we do nothing else here.
 *
 */
void
kthread_cancel(kthread_t *kthr, void *retval)
{
        KASSERT(NULL != kthr); /* the kthr argument of this function must be a valid thread */

        if(curthr == kthr) {
        /* If the thread to be cancelled is the current thread, this is equivalent to calling kthread_exit. */ 
                kthread_exit(retval);
        }
        else {
        /* set the cancelled and retval fields of the thread. On wakeup, threads should check their cancelled fields and act accordingly.  */
                kthr->kt_retval=retval;
                kthr->kt_cancelled = 1;
                sched_cancel(kthr);
        }
}

/*
 * You need to set the thread's retval field and alert the current
 * process that a thread is exiting via proc_thread_exited. You should
 * refrain from setting the thread's state to KT_EXITED until you are
 * sure you won't make any more blocking calls before you invoke the
 * scheduler again.
 *
 * It may seem unneccessary to push the work of cleaning up the thread
 * over to the process. However, if you implement MTP, a thread
 * exiting does not necessarily mean that the process needs to be
 * cleaned up.
 *
 * The void * type of retval is simply convention and does not necessarily
 * indicate that retval is a pointer
 */
void
kthread_exit(void *retval)
{

        curthr->kt_retval = retval;     /* Store return value */
        curthr->kt_wchan = NULL;        /* Get out from any queues */
        KASSERT(!curthr->kt_wchan);
        dbg(DBG_PRINT, "(GRADING1A 3.c)\n");

        proc_thread_exited(retval);           /* Clean up process */
        KASSERT(!curthr->kt_qlink.l_next && !curthr->kt_qlink.l_prev);
        KASSERT(curthr->kt_proc == curproc);
        dbg(DBG_PRINT, "(GRADING1A 3.c)\n");

        sched_switch();                 /* Exit from current thread */

}

/*
 * The new thread will need its own context and stack. Think carefully
 * about which fields should be copied and which fields should be
 * freshly initialized.
 *
 * You do not need to worry about this until VM.
 */
kthread_t *
kthread_clone(kthread_t *thr)
{
    
    KASSERT(KT_RUN == thr->kt_state); /* the thread you are cloning must be in the running or runnable state */
    dbg(DBG_PRINT, "(GRADING3A 8.a)\n");

    kthread_t *child_thd = slab_obj_alloc(kthread_allocator);

    child_thd->kt_kstack = alloc_stack();


    child_thd->kt_errno = thr->kt_errno;
    child_thd->kt_retval = thr->kt_retval;
    
    void *ctx_stack = alloc_stack();
    child_thd->kt_ctx.c_kstack = (uintptr_t) ctx_stack;
    child_thd->kt_ctx.c_kstacksz = DEFAULT_STACK_SIZE;

    child_thd->kt_state = thr->kt_state;
    child_thd->kt_cancelled = thr->kt_cancelled;
    child_thd->kt_wchan = thr->kt_wchan;

    list_link_init(&child_thd->kt_qlink);
    list_link_init(&child_thd->kt_plink);

    KASSERT(KT_RUN == child_thd->kt_state); /* new thread starts in the runnable state */
    dbg(DBG_PRINT, "(GRADING3A 8.a)\n");

    return child_thd;
        
}

/*
 * The following functions will be useful if you choose to implement
 * multiple kernel threads per process. This is strongly discouraged
 * unless your weenix is perfect.
 */
#ifdef __MTP__
int
kthread_detach(kthread_t *kthr)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_detach");
        return 0;
}

int
kthread_join(kthread_t *kthr, void **retval)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_join");
        return 0;
}

/* ------------------------------------------------------------------ */
/* -------------------------- REAPER DAEMON ------------------------- */
/* ------------------------------------------------------------------ */
static __attribute__((unused)) void
kthread_reapd_init()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_init");
}
init_func(kthread_reapd_init);
init_depends(sched_init);

void
kthread_reapd_shutdown()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_shutdown");
}

static void *
kthread_reapd_run(int arg1, void *arg2)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_run");
        return (void *) 0;
}
#endif
