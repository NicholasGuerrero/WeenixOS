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

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        vmmap_t *new_vmmap = slab_obj_alloc(vmmap_allocator);
        list_init(&new_vmmap->vmm_list);
        new_vmmap->vmm_proc = NULL;
        dbg(DBG_PRINT, "(GRADING3A)\n"); 
        return new_vmmap;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
        KASSERT(NULL != map); /* function argument must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.a)\n");

        list_iterate_begin(&map->vmm_list, vmarea_t *vma_iter, vmarea_t, vma_plink) {

                vmmap_remove(map, vma_iter->vma_start, vma_iter->vma_end - vma_iter->vma_start);
                dbg(DBG_PRINT, "(GRADING3A)\n"); 

        } list_iterate_end();

        slab_obj_free(vmmap_allocator, map);
        dbg(DBG_PRINT, "(GRADING3A)\n"); 
}


/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
        KASSERT(NULL != map && NULL != newvma); /* both function arguments must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(NULL == newvma->vma_vmmap); /* newvma must be newly create and must not be part of any existing vmmap */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(newvma->vma_start < newvma->vma_end); /* newvma must not be empty */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end); /* addresses in this memory segment must lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");


        if list_empty(&map->vmm_list) {
                newvma->vma_vmmap = map;
                list_insert_head(&map->vmm_list, &newvma->vma_plink);
                dbg(DBG_PRINT, "(GRADING3A)\n"); 
                return;
        }
        
        list_iterate_begin(&map->vmm_list, vmarea_t *vma_iter, vmarea_t, vma_plink) {
                if (vma_iter->vma_plink.l_prev != &map->vmm_list) {
                        /* prev vmaarea exists */
                        vmarea_t *prev_vma = list_item(vma_iter->vma_plink.l_prev, vmarea_t, vma_plink);
                        if (!(newvma->vma_end > vma_iter->vma_start || newvma->vma_start < prev_vma->vma_end)) {
                                list_insert_before(&vma_iter->vma_plink, &newvma->vma_plink);
                                newvma->vma_vmmap = map;
                                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                                return;
                        }
                        dbg(DBG_PRINT, "(GRADING3A)\n"); 
                } 
                else {
                        /*prev vmarea doe not exist */
                        if (newvma->vma_end <= vma_iter->vma_start) {
                                list_insert_head(&map->vmm_list, &newvma->vma_plink);
                                newvma->vma_vmmap = map;
                                dbg(DBG_PRINT, "(GRADING3A)\n"); 
                                return;
                        }
                        dbg(DBG_PRINT, "(GRADING3A)\n"); 
                }
                if (newvma->vma_end <= vma_iter->vma_start) {
                        list_insert_head(&map->vmm_list, &newvma->vma_plink);
                        newvma->vma_vmmap = map;
                        dbg(DBG_PRINT, "(GRADING3A)\n");
                        return;
                }
                dbg(DBG_PRINT, "(GRADING3A)\n"); 
        } list_iterate_end();

        list_insert_tail(&map->vmm_list, &newvma->vma_plink);
        newvma->vma_vmmap = map;
        dbg(DBG_PRINT, "(GRADING3A)\n"); 
}


/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        /* If low to high */
        if (dir == VMMAP_DIR_LOHI) {
                /* If list is empty then & dir == VMMAP_DIR_LOHI then take the lowest address possible */
                if list_empty(&map->vmm_list) {
                        dbg(DBG_PRINT, "(GRADING3D 1)\n"); 
                        return (USER_MEM_LOW / PAGE_SIZE);
                }

                vmarea_t *vma_iter;
                list_iterate_begin(&map->vmm_list, vma_iter, vmarea_t, vma_plink) {
                        if (&map->vmm_list != vma_iter->vma_plink.l_prev) {
                                /* If the previous vma is not the map list then a previous vma exists! The difference between the prev vma's end
                                * and the cur vma's start is >= to npages then you can return that previous_vma end 
                                * as the new memory start area, since it must be big enough!
                                *  i.e LOW ){MAP}  prev_vma end ](*!return here) ---npages?-- [ vma_iter start (HIGH  */
                                vmarea_t *prev_vma = list_item(vma_iter->vma_plink.l_prev, vmarea_t, vma_plink);
                                if (npages <= vma_iter->vma_start - prev_vma->vma_end) {
                                        dbg(DBG_PRINT, "(GRADING3D 1)\n"); 
                                        return prev_vma->vma_end;
                                }
                                dbg(DBG_PRINT, "(GRADING3D 1)\n"); 
                        }
                        else {
                                /* Otherwise the vma_iter is the first vma on the vmm_list. If the difference between the first userspace's
                                * page + npages we want is <= to the vma_iter start page then we can return that starting page. 
                                * LOW) USER_MEM_LOW / PAGESIZE](*!return here) ---(npages?)---- {MAP}  [vma_iter->vma_start (HIGH*/
                                if (npages <= vma_iter->vma_start - (USER_MEM_LOW / PAGE_SIZE)) {
                                        dbg(DBG_PRINT, "(GRADING3D 3)\n"); 
                                        return (USER_MEM_LOW / PAGE_SIZE);
                                }
                                dbg(DBG_PRINT, "(GRADING3D 3)\n");
                        }
                } list_iterate_end();


                /* If the first highest page minus the last vma's end location is >= to the npages we want then return that vma's end location  */
                if(npages  <= USER_MEM_HIGH / PAGE_SIZE - vma_iter->vma_end) {
                        dbg(DBG_PRINT, "(GRADING3D 5)\n"); 
                        return vma_iter->vma_end;
                } 
                else {
                        /* otherwise we're out of luck! return -1 */
                        dbg(DBG_PRINT, "(GRADING3D 5)\n");
                         return -1;
                }
                dbg(DBG_PRINT, "(GRADING3D 5)\n"); 
        }
        /* If high to low */
        else if (dir == VMMAP_DIR_HILO) {
                /* If list is empty then & dir == VMMAP_DIR_HILO then take the highest address possible - npages since we need enough space to allocate*/
                if list_empty(&map->vmm_list) {
                        dbg(DBG_PRINT, "(GRADING3D 4)\n"); 
                        return (USER_MEM_HIGH / PAGE_SIZE - npages);
                }

                vmarea_t *vma_iter;
                list_iterate_reverse(&map->vmm_list, vma_iter, vmarea_t, vma_plink) { 
                        if (&map->vmm_list != vma_iter->vma_plink.l_next) {
                                /* If the next vma (from the back) is not the map list then a next (previous) vma exists! 
                                * The difference between the next vma's start and the cur vma's start is >= to npages 
                                * then you can return that next_vma's start - npages as the new memory start area, since it must be big enough!
                                *  i.e HIGH) {MAP} next_vma start ] ---(npages?)--- (*!return here) [ vma_iter end   (LOW */
                                vmarea_t *next_vma = list_item(vma_iter->vma_plink.l_next, vmarea_t, vma_plink);
                                if (npages <= next_vma->vma_start - vma_iter->vma_end ) {
                                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                                        return (next_vma->vma_start - npages);  /* - npages since high to low */
                                }
                                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        }
                        else {
                                /* Otherwise the vma_iter is the first vma on the vmm_list. If the difference between the first userspace's
                                * page + npages (HIGH) we want is <= to the vma_iter's end page then we can return that as the starting page. 
                                * i.e HIGH)  ----(npages?)----   (*!return here) {MAP} [ vma_iter->vma_end (LOW  */
                                if (npages <= USER_MEM_HIGH / PAGE_SIZE - vma_iter->vma_end) {
                                        dbg(DBG_PRINT, "(GRADING3A)\n");
                                        return (USER_MEM_HIGH / PAGE_SIZE - npages);
                                }
                                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        }
                } list_iterate_end();

                /* If the last highest page minus the slowest memory location is >= to the npages we want then return that vma's end location - npages  */
                if (npages <= vma_iter->vma_start - USER_MEM_LOW / PAGE_SIZE ) {
                        dbg(DBG_PRINT, "(GRADING3D 3)\n");
                        return vma_iter->vma_start - npages;
                        } 
                else {
                        /* We're out of luck! no space possible!*/
                        dbg(DBG_PRINT, "(GRADING3D 4)\n");
                        return -1;
                }
                dbg(DBG_PRINT, "(GRADING3D 3)\n");
        }
        else {
                /* no space possible */
                dbg(DBG_PRINT, "(GRADING3D 5)\n");
                return -1;
        }
        dbg(DBG_PRINT, "(GRADING3D 3)\n");
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
        KASSERT(NULL != map); /* the first function argument must not be NULL */
        dbg(DBG_PRINT, "(GRADING3A 3.c)\n");

        list_iterate_begin(&map->vmm_list, vmarea_t *vma_iter, vmarea_t, vma_plink) { 

                if (vfn >= vma_iter->vma_start && vfn < vma_iter->vma_end) {
                        dbg(DBG_PRINT, "(GRADING3A)\n");
                        return vma_iter;
                }
        } list_iterate_end();
        dbg(DBG_PRINT, "(GRADING3D 1)\n");
        return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        vmmap_t* new_vmm = vmmap_create();

        list_iterate_begin(&map->vmm_list, vmarea_t *vma_iter, vmarea_t, vma_plink) {
                vmarea_t* fresh_vma = vmarea_alloc();
                fresh_vma->vma_start = vma_iter->vma_start;
                fresh_vma->vma_end = vma_iter->vma_end;
                fresh_vma->vma_off = vma_iter->vma_off;

                fresh_vma->vma_prot = vma_iter->vma_prot;
                fresh_vma->vma_flags = vma_iter->vma_flags;
                
                fresh_vma->vma_obj = vma_iter->vma_obj;
                fresh_vma->vma_obj->mmo_ops->ref(fresh_vma->vma_obj);

                list_link_init(&fresh_vma->vma_plink);
                list_link_init(&fresh_vma->vma_olink);

                vmmap_insert(new_vmm, fresh_vma);
                dbg(DBG_PRINT, "(GRADING3A)\n");

                
        } list_iterate_end();

        dbg(DBG_PRINT, "(GRADING3A)\n");
        return new_vmm;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int 
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        KASSERT(NULL != map); /* must not add a memory segment into a non-existing vmmap */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT(0 < npages); /* number of pages of this memory segment cannot be 0 */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags)); /* must specify whether the memory segment is shared or private */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage)); /* if lopage is not zero, it must be a user space vpn */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages))); /* if lopage is not zero, the specified page range must lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT(PAGE_ALIGNED(off)); /* the off argument must be page aligned */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        if (lopage == 0) {
                /* If lopage is zero, we will find a range of virtual addresses in the
                * process that is big enough, by using vmmap_find_range with the same
                * dir argument.*/

               int vma_start = vmmap_find_range(map, npages, dir);
               lopage = vma_start;
               dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        else if (lopage != 0 && !vmmap_is_range_empty(map, lopage, npages) ) {
                /* If lopage is non-zero and the specified region
                * contains another mapping that mapping should be unmapped.*/
               vmmap_remove(map, lopage, npages);
               dbg(DBG_PRINT, "(GRADING3A)\n");

        }

        vmarea_t *new_vmarea = vmarea_alloc();
        new_vmarea->vma_start = lopage;
        new_vmarea->vma_end = lopage + npages;
        new_vmarea->vma_off = ADDR_TO_PN(off);
        new_vmarea->vma_prot = prot;
        new_vmarea->vma_flags = flags;
        new_vmarea->vma_obj = NULL;
        list_link_init(&new_vmarea->vma_plink);
        list_link_init(&new_vmarea->vma_olink);

        mmobj_t *mmobj;
        if (file == NULL)
        {
               /*  If file is NULL an anon mmobj will be used to create a mapping
                * of 0's.*/
               mmobj = anon_create();
               dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        else {
                /* If file is non-null that vnode's file will be mapped in
                * for the given range.  Use the vnode's mmap operation to get the
                * mmobj for the file; do not assume it is file->vn_obj. Make sure all
                * of the area's fields except for vma_obj have been set before
                * calling mmap. */
               
               int rv = file->vn_ops->mmap(file, new_vmarea, &mmobj); //increments ref by 1 on mmobj
               dbg(DBG_PRINT, "(GRADING3A)\n");
               
        }

        new_vmarea->vma_obj = mmobj;

        if (flags & MAP_PRIVATE)
        {
                /*If MAP_PRIVATE is specified set up a shadow object for the mmobj. */
                mmobj_t *shadow_mmobj = shadow_create();
                       
                shadow_mmobj->mmo_shadowed = mmobj;

                new_vmarea->vma_obj = shadow_mmobj;
                
                shadow_mmobj->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(mmobj);
                
                list_insert_head(&mmobj->mmo_un.mmo_vmas, &new_vmarea->vma_olink);
                dbg(DBG_PRINT, "(GRADING3A)\n");
                
        }

        if (new) {
                /*If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it. */
                *new = new_vmarea;
                dbg(DBG_PRINT, "(GRADING3A)\n");
        }

        
        vmmap_insert(map, new_vmarea); /* insert new_vmarea into map */
        dbg(DBG_PRINT, "(GRADING3A)\n");

        return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        uint32_t hipage = lopage + npages;
        list_iterate_begin(&map->vmm_list, vmarea_t *vma_iter, vmarea_t, vma_plink) {

                if (lopage > vma_iter->vma_start && hipage < vma_iter->vma_end ) {
                        /* Case 1:  [   ******    ] */
                        /* * The region to be unmapped lies completely inside the vmarea. We need to
                        * split the old vmarea into two vmareas. be sure to increment the
                        * reference count to the file associated with the vmarea. */   

                        vmarea_t* fresh_vma = vmarea_alloc(); /* clearing data with new vma */
                        fresh_vma->vma_start = hipage;
                        fresh_vma->vma_end = vma_iter->vma_end;
                        fresh_vma->vma_off = vma_iter->vma_off + hipage - vma_iter->vma_start;
                        fresh_vma->vma_prot = vma_iter->vma_prot;
                        fresh_vma->vma_flags = vma_iter->vma_flags;
                        fresh_vma->vma_vmmap = map;
                        fresh_vma->vma_obj = vma_iter->vma_obj;
                        vma_iter->vma_end = lopage;

                        list_link_init(&fresh_vma->vma_plink);
                        list_insert_before(&vma_iter->vma_plink, &fresh_vma->vma_plink);
                        list_link_init(&fresh_vma->vma_olink);


                        fresh_vma->vma_obj->mmo_ops->ref(fresh_vma->vma_obj);

                        mmobj_t *old_mmo = vma_iter->vma_obj;
                        fresh_vma->vma_obj->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(vma_iter->vma_obj);
                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                }
                else if(lopage > vma_iter->vma_start && hipage >= vma_iter->vma_end && lopage < vma_iter->vma_end) {
                        /* Case 2:  [      *******]** */
                        /* * The region overlaps the end of the vmarea. Just shorten the length of
                        * the mapping. */
                       
                       vma_iter->vma_end = lopage;
                       dbg(DBG_PRINT, "(GRADING3D 1)\n");
                      

                }
                else if (lopage <= vma_iter->vma_start && hipage < vma_iter->vma_end && lopage < vma_iter->vma_end) {
                        /* Case 3: *[*****        ] */
                        /* * The region overlaps the beginning of the vmarea. Move the beginning of
                        * the mapping (remember to update vma_off), and shorten its length. */
                       
                       vma_iter->vma_off = hipage - vma_iter->vma_start + vma_iter->vma_off;
                       vma_iter->vma_start = hipage;
                       dbg(DBG_PRINT, "(GRADING3A)\n");
                }
                else if (lopage <= vma_iter->vma_start && hipage >= vma_iter->vma_end) {
                        /* Case 4: *[*************]** */
                        /* * The region completely contains the vmarea. Remove the vmarea from the
                        * list. */
                        
                        if (list_link_is_linked(&vma_iter->vma_plink)) {
                                list_remove(&vma_iter->vma_plink); 
                                dbg(DBG_PRINT, "(GRADING3A)\n");
                        }

                        if (list_link_is_linked(&vma_iter->vma_olink)) {
                                list_remove(&vma_iter->vma_olink);
                                dbg(DBG_PRINT, "(GRADING3A)\n");
                        }
                
                        vma_iter->vma_obj->mmo_ops->put(vma_iter->vma_obj);
                        vma_iter->vma_obj = NULL;
                 
                       vmarea_free(vma_iter);
                       dbg(DBG_PRINT, "(GRADING3A)\n");  
                }
                dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();           

        pt_unmap_range(curproc->p_pagedir,(uintptr_t) PN_TO_ADDR(lopage), (uintptr_t)PN_TO_ADDR(hipage));
        dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;
}


/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        
        uint32_t endvfn = startvfn+npages;
        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn)); /* the specified page range must not be empty and lie completely within the user space */
         dbg(DBG_PRINT, "(GRADING3A 3.e)\n");

        list_iterate_begin(&map->vmm_list,vmarea_t* vma_iter, vmarea_t, vma_plink) {

                if (vma_iter->vma_start < endvfn && vma_iter->vma_end > startvfn) {
                        dbg(DBG_PRINT, "(GRADING3A)\n");
                        return 0;
                }
                dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();
        dbg(DBG_PRINT, "(GRADING3A)\n");
        return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        
        uint32_t copy_vaddr = (uint32_t) vaddr; /* shallow copy */
        uint32_t copy_buf = (uint32_t) buf; /* shallow copy */

        while (count > 0) { 
                int start_vfn = ADDR_TO_PN(copy_vaddr);
                int vma_off = PAGE_OFFSET(copy_vaddr);

                vmarea_t *vma = vmmap_lookup(map, start_vfn);
                pframe_t *pf;
                int bytes_to_read;
                int pagenum = start_vfn - vma->vma_start + vma->vma_off;
                int rv = pframe_lookup(vma->vma_obj, pagenum, 1, &pf);

                if ((PAGE_SIZE - vma_off) <= count) {
                        bytes_to_read = PAGE_SIZE - vma_off;
                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                }
                else {
                        bytes_to_read = count;
                        dbg(DBG_PRINT, "(GRADING3A)\n");
                }
                memcpy((char *)copy_buf, (char *)pf->pf_addr + vma_off, bytes_to_read);
                copy_vaddr += bytes_to_read;
                copy_buf += bytes_to_read;
                count -= bytes_to_read;
                dbg(DBG_PRINT, "(GRADING3A)\n");         
        }
        dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;    
}


// /* Write from 'buf' into the virtual address space of 'map' starting at
//  * 'vaddr' for size 'count'. To do this, you will need to find the correct
//  * vmareas to write into, then find the correct pframes within those vmareas,
//  * and finally write into the physical addresses that those pframes correspond
//  * to. You should not check permissions of the areas you use. Assume (KASSERT)
//  * that all the areas you are accessing exist. Remember to dirty pages!
//  * Returns 0 on success, -errno on error.
//  */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{

    uint32_t *copy_buf = (uint32_t *)buf;
    uint32_t copy_vaddr = (uint32_t)vaddr;
    
    while (count > 0) {
        uint32_t start_vfn = ADDR_TO_PN(copy_vaddr);
        uint32_t vma_off = PAGE_OFFSET(copy_vaddr);

       
        vmarea_t *vma = vmmap_lookup(map, start_vfn);
        pframe_t *pf;
        int bytes_to_write;
        int pagenum = start_vfn - vma->vma_start + vma->vma_off;
        int rv = pframe_lookup(vma->vma_obj, pagenum, 1, &pf);

     
        if ((PAGE_SIZE - vma_off) <= count) {
                bytes_to_write = PAGE_SIZE - vma_off;
                dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        else {
                bytes_to_write = count;
                dbg(DBG_PRINT, "(GRADING3A)\n");
        }

        memcpy((char *)pf->pf_addr + vma_off, (char *)copy_buf, bytes_to_write);
        pframe_dirty(pf);        
        copy_buf += bytes_to_write;
        copy_vaddr += bytes_to_write;
        count -= bytes_to_write;
        dbg(DBG_PRINT, "(GRADING3A)\n");
    }
    dbg(DBG_PRINT, "(GRADING3A)\n");
    return 0;

}