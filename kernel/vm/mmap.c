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

#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
        /* See https://man7.org/linux/man-pages/man2/mmap.2.html */
        
        uint32_t lopage;
        uint32_t hipage = (uint32_t) addr + len;

        if (!((flags & MAP_PRIVATE) ^ (flags & MAP_SHARED)))
        {
                /* The negation of XOR is the logical biconditional, which yields true if and only if the two inputs are the same */
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }

        if (!PAGE_ALIGNED(off) || (!PAGE_ALIGNED(addr)) || len == 0) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }

        if ((size_t) - 1 == len) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }

 
        if (flags & MAP_FIXED)
        {       
                /* if User memory low (inclusive) > addr OR user memory hi is < our hipage then lopage is accaptable */
                if ((USER_MEM_LOW > (uint32_t) addr) || (USER_MEM_HIGH < hipage))
                {
                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        return -EINVAL;
                }
                lopage = ADDR_TO_PN(addr);
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
        } else {
                /* otherwise use 0 */
                lopage = 0;
                dbg(DBG_PRINT, "(GRADING3A)\n");
        }

        int remainder = (len % PAGE_SIZE != 0);  /* make (len / PAGE_SIZE) valid */
        vmarea_t *user_vma = NULL;
        int rv;

        if (flags & MAP_ANON) {
                rv = vmmap_map(curproc->p_vmmap, 0, lopage,  len / PAGE_SIZE + remainder, 
                prot, flags, off, VMMAP_DIR_HILO, &user_vma);  /* may need to check 3rd parameter of vmmap_map */
                dbg(DBG_PRINT, "(GRADING3D 3)\n");
        } 
        else {
                if (fd > NFILES || fd < 0)
                {
                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        return -EBADF;
                }

                file_t *file = fget(fd);
                if (file == NULL)
                {
                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        return -EBADF;
                }
 
                if ((prot & PROT_WRITE) && (flags & MAP_SHARED) && (file->f_mode == FMODE_READ))
                {       
                        /* invalid combination */
                        fput(file);
                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        return -EINVAL;
                }

                rv = vmmap_map(curproc->p_vmmap, file->f_vnode, lopage, len / PAGE_SIZE + remainder, 
                prot, flags, off, VMMAP_DIR_HILO, &user_vma);
                fput(file);
                dbg(DBG_PRINT, "(GRADING3A)\n");
        }

        if (rv != 0)
        {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return rv;
        }

        pt_unmap_range(curproc->p_pagedir,
                        (uintptr_t) PN_TO_ADDR(user_vma->vma_start), 
                        (uintptr_t) PN_TO_ADDR(user_vma->vma_start) + (uintptr_t) PAGE_ALIGN_UP(len));
        tlb_flush_range((uintptr_t) PN_TO_ADDR(user_vma->vma_start), (uint32_t) PAGE_ALIGN_UP(len) / PAGE_SIZE);


        *ret = PN_TO_ADDR(user_vma->vma_start);
        

        KASSERT(NULL != curproc->p_pagedir); /* page table must be valid after a memory segment is mapped into the address space */
        dbg(DBG_PRINT, "(GRADING3A 2.a)\n");
        
        dbg(DBG_PRINT, "(GRADING3A)\n");
        return rv;
        
}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
        /* see https://linux.die.net/man/2/munmap  */

        int remainder = (len % PAGE_SIZE != 0);  /* make (len / PAGE_SIZE) valid */
        uint32_t npages = len / PAGE_SIZE + remainder;
        uint32_t lopage = ADDR_TO_PN((uint32_t) addr);
        uint32_t hipage = lopage + npages;
        uintptr_t loaddr = (uintptr_t)addr;
        uintptr_t hiaddr = loaddr + len;
        int loaddr_valid = loaddr < USER_MEM_HIGH && loaddr >= USER_MEM_LOW;
        int hiaddr_valid = hiaddr <= USER_MEM_HIGH && hiaddr > USER_MEM_LOW;
        int valid_region = loaddr && hiaddr_valid;

        if (!valid_region) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }

 
        if (len <= 0 || len > (USER_MEM_HIGH - USER_MEM_LOW))
        {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL; 
        }

         if (USER_MEM_LOW > (uint32_t) addr)
        {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }
 
        if (USER_MEM_HIGH < npages)
        {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }

        if (!PAGE_ALIGNED(addr)) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }
        

        if (vmmap_is_range_empty(curproc->p_vmmap, lopage, npages)) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return 0;
        }           
        int rv = vmmap_remove(curproc->p_vmmap, lopage, npages);
        tlb_flush_range((uintptr_t)addr, npages);
        pt_unmap_range(pt_get(), (uintptr_t)addr, (uintptr_t)PN_TO_ADDR(hipage));
        dbg(DBG_PRINT, "(GRADING3A)\n");

        return rv;
}
