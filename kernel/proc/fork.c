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

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


// /*
//  * The implementation of fork(2). Once this works,
//  * you're practically home free. This is what the
//  * entirety of Weenix has been leading up to.
//  * Go forth and conquer.
//  */
int
do_fork(struct regs *regs)
{
        vmarea_t *vma, *clone_vma;
        pframe_t *pf;
        mmobj_t *to_delete, *new_shadowed;

        
        KASSERT(regs != NULL); /* the function argument must be non-NULL */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc != NULL); /* the parent process, which is curproc, must be non-NULL */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc->p_state == PROC_RUNNING); /* the parent process must be in the running state and not in the zombie state */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        /* Bullet 1) Allocate a proc_t out of the procs structure using proc_create(). */
        /* Bullet 7) Set the child's working directory to point to the parent's working directory (once again, remember reference counts). */   
        proc_t *child_proc = proc_create("child_proc"); // creats one ref

        KASSERT(child_proc->p_state == PROC_RUNNING); /* new child process starts in the running state */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        

       

        /* Bullet 2) Copy the vmmap_t from the parent process into the child using vmmap_clone(). 
        * Remember to increase the reference counts on the underlying mmobj_ts. */
        child_proc->p_vmmap = vmmap_clone(curproc->p_vmmap);
        child_proc->p_vmmap->vmm_proc = child_proc;


        /* Bullet 3) For each private mapping, point the vmarea_t at the new shadow object, 
        * which in turn should point to the original mmobj_t for the vmarea_t. 
        * This is how you know that the pages corresponding to this mapping are copy-on-write.
        * Be careful with reference counts. Also note that for shared mappings, there is no need to copy the mmobj_t. */
        vmmap_t *parent_vmm_map = curproc->p_vmmap;
        vmmap_t *child_vmm_map = child_proc->p_vmmap;
        list_iterate_begin(&parent_vmm_map->vmm_list, vmarea_t *parent_vma, vmarea_t, vma_plink) { 

            vmarea_t *child_vma = vmmap_lookup(child_vmm_map, parent_vma->vma_start);

            if (!(parent_vma->vma_flags & MAP_SHARED)) {

                mmobj_t *shadow_mmobj = parent_vma->vma_obj;
                mmobj_t *bottom_mmobj = mmobj_bottom_obj(shadow_mmobj);

                mmobj_t *child_shadow = shadow_create();
                mmobj_t *parent_shadow = shadow_create();

                child_shadow->mmo_shadowed = shadow_mmobj;
                child_shadow->mmo_un.mmo_bottom_obj = bottom_mmobj;

                parent_shadow->mmo_shadowed = shadow_mmobj;
                parent_shadow->mmo_un.mmo_bottom_obj = bottom_mmobj;
             
                list_insert_head(&bottom_mmobj->mmo_un.mmo_vmas, &child_vma->vma_olink);

                child_vma->vma_obj = child_shadow;
                parent_vma->vma_obj = parent_shadow;
                dbg(DBG_PRINT, "(GRADING3A)\n");
            }
            dbg(DBG_PRINT, "(GRADING3A)\n");

        } list_iterate_end();


        /* Bullet 6) Copy the file descriptor table of the parent into the child. Remember to use fref() here. */
        for (int i = 0 ; i < NFILES ; i++) {
            if (curproc->p_files[i]) {
                child_proc->p_files[i] = curproc->p_files[i];
                fref(child_proc->p_files[i]);
                dbg(DBG_PRINT, "(GRADING3A)\n");
            }
            dbg(DBG_PRINT, "(GRADING3A)\n");
        }


        /* Bullet 8) Use kthread_clone() to copy the thread from the parent process into the child process. */
        kthread_t *parent_thrd = list_item(curproc->p_threads.l_next, kthread_t, kt_plink);
        kthread_t *child_thd = kthread_clone(parent_thrd);
        child_thd->kt_proc = child_proc;
        list_insert_head(&child_proc->p_threads, &child_thd->kt_plink);


        /* Bullet 5) Set up the new process thread context (kt_ctx). You will need to set the following:
                    c_pdptr - the page table pointer
                    c_eip - function pointer for the userland_entry() function
                    c_esp - the value returned by fork_setup_stack()
                    c_kstack - the top of the new thread's kernel stack
                    c_kstacksz - size of the new thread's kernel stack
                    Remember to set the return value in the child process! */

        regs->r_eax = 0;
        child_thd->kt_ctx.c_pdptr = child_proc->p_pagedir;
        child_thd->kt_ctx.c_eip = (uintptr_t)userland_entry;
        child_thd->kt_ctx.c_esp = fork_setup_stack(regs, child_thd->kt_kstack);
        child_thd->kt_ctx.c_ebp = curthr->kt_ctx.c_ebp;

        child_proc->p_brk = curproc->p_brk;
        child_proc->p_start_brk = curproc->p_start_brk;


        KASSERT(child_proc->p_pagedir != NULL); /* new child process must have a valid page table */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(child_thd->kt_kstack != NULL); /* thread in the new child process must have a valid kernel stack */
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        
        /* Bullet 4) Unmap the user land page table entries and flush the TLB (using pt_unmap_range() and tlb_flush_all()). 
        * This is necessary because the parent process might still have some entries marked as "writable", 
        * but since we are implementing copy-on-write we would like access to these pages to cause a trap. */
        pagedir_t *pagedir = pt_get();
        pt_unmap_range(pagedir, USER_MEM_LOW, USER_MEM_HIGH);
        tlb_flush_all();
        
        /* Bullet 10)  Make the new thread runnable. */
        sched_make_runnable(child_thd);

        dbg(DBG_PRINT, "(GRADING3A)\n");
        return child_proc->p_pid;


}

