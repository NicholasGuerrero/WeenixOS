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
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        KASSERT(NULL != dir); /* the "dir" argument must be non-NULL */
        KASSERT(NULL != name); /* the "name" argument must be non-NULL */
        KASSERT(NULL != result); /* the "result" argument must be non-NULL */

        char input_name[NAME_LEN];
        
        if(strcmp(name, "/")==0){
                strcpy(input_name, ".");
                dbg(DBG_PRINT, "(GRADING2B)\n");
                if(S_ISDIR(dir->vn_mode)){
                        dbg(DBG_PRINT, "(GRADING2B)\n");
                        return dir->vn_ops->lookup(dir, input_name, len, result);
                }
        }
        

        if(S_ISDIR(dir->vn_mode)){
                dbg(DBG_PRINT, "(GRADING2A)\n");
                return dir->vn_ops->lookup(dir, name, len, result);

        }
        
        dbg(DBG_PRINT, "(GRADING2B)\n");
        return -ENOTDIR;
        
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
        KASSERT(NULL != pathname); /* the "pathname" argument must be non-NULL */
        KASSERT(NULL != namelen); /* the "namelen" argument must be non-NULL */
        KASSERT(NULL != name); /* the "name" argument must be non-NULL */
        KASSERT(NULL != res_vnode); /* the "res_vnode" argument must be non-NULL */

        //Setting the start vnode directory
        vnode_t *dir;
        if(pathname[0] == '/'){
                dbg(DBG_PRINT, "(GRADING2A)\n");
                dir = vfs_root_vn;
        }else if(base == NULL){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                dir = curproc->p_cwd;
        }else{
                dbg(DBG_PRINT, "(GRADING2B)\n");
                dir = base;
        }

        KASSERT(NULL != dir); /* pathname resolution must start with a valid directory */

        //pathname resolution
        const char *sep = "/";
        char *token =NULL;
        char return_name[NAME_LEN], path_cp[MAXPATHLEN];
        int i=0;
        int look_count=0, look_success=0;
        int path_len = (int)strlen(pathname);
        vnode_t *child_vnode, *par_vnode, *pre_par_vnode;

        strcpy(path_cp, pathname);
        token = strtok(path_cp, sep);
        while(NULL != token){
                dbg(DBG_PRINT, "(GRADING2A)\n");
                if(strlen(token) > NAME_LEN){
                        dbg(DBG_PRINT, "(GRADING2B)\n");
                        return -ENAMETOOLONG;
                }

                //Finding the target pathname position
                for(i=0; i<path_len; i++){
                        dbg(DBG_PRINT, "(GRADING2A)\n");
                        if(strcmp(token, &path_cp[i]) ==0){
                                dbg(DBG_PRINT, "(GRADING2A)\n");
                                strcpy(return_name, token);
                                break;
                        }
                }

                pre_par_vnode = par_vnode;
                par_vnode = dir;
                look_count++;
                if(0== lookup(dir, token, strlen(token), &child_vnode)){
                        dbg(DBG_PRINT, "(GRADING2A)\n");
                        look_success++;
                        if(look_count > 2){
                                dbg(DBG_PRINT, "(GRADING2B)\n");
                                vput(pre_par_vnode);
                        }
                        dir = child_vnode;
                
                }else{
                        dbg(DBG_PRINT, "(GRADING2A)\n");
                        if((token = strtok(NULL, sep)) ==NULL){
                                dbg(DBG_PRINT, "(GRADING2A)\n");
                                if(look_count > 2){
                                        dbg(DBG_PRINT, "(GRADING2B)\n");
                                        vput(pre_par_vnode);
                                }
                                break;
                        }else{
                                //parent directories are invalid
                                dbg(DBG_PRINT, "(GRADING2B)\n");
                                if(look_success == 1){
                                        dbg(DBG_PRINT, "(GRADING2B)\n");
                                        vput(dir);
                                }
                                else if(look_count > 1){
                                        dbg(DBG_PRINT, "(GRADING2B)\n");
                                        vput(pre_par_vnode);
                                        vput(dir);
                                }
                                return -ENOENT;
                        }
                }
                token = strtok(NULL, sep);

        }
        *name = &pathname[i];
        //*namelen = strlen(return_name);
        
        if(look_count == 0){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                vref(dir);
                par_vnode = dir;
                
        }else{
                dbg(DBG_PRINT, "(GRADING2A)\n");
                *namelen = strlen(return_name);
                if(look_count ==1){
                        dbg(DBG_PRINT, "(GRADING2A)\n");
                        vref(par_vnode);
                }
                if(look_count == look_success){
                        dbg(DBG_PRINT, "(GRADING2A)\n");
                        vput(dir);
                }

        }
        *res_vnode = par_vnode;
        
        dbg(DBG_PRINT, "(GRADING2A)\n");
        return 0;
}


/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fcntl.h>.  If the O_CREAT flag is specified and the file does
 * not exist, call create() in the parent directory vnode. However, if the
 * parent directory itself does not exist, this function should fail - in all
 * cases, no files or directories other than the one at the very end of the path
 * should be created.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
        vnode_t *par_vnode;
        const char *name = pathname;
        size_t namelen = strlen(name);
        if(namelen == 0){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EINVAL;
        }
        if(NULL != base){
                dbg(DBG_PRINT, "(GRADING2A)\n");
                par_vnode = base;
        }else{
                dbg(DBG_PRINT, "(GRADING2A)\n");
                par_vnode = curproc->p_cwd;
        }

        if(0!= dir_namev(pathname, &namelen, &name, NULL, &par_vnode)){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                // vput(par_vnode); //
                return -ENOENT;
        }
        

        vnode_t *dir_vnode;
        if(0!= lookup(par_vnode, name, namelen, &dir_vnode)){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                if(par_vnode->vn_mode != S_IFDIR){
                        dbg(DBG_PRINT, "(GRADING2B)\n");
                        vput(par_vnode);
                        // vput(dir_vnode); //
                        return -ENOTDIR;
                }else{
                        dbg(DBG_PRINT, "(GRADING2B)\n");
                        if((flag & O_CREAT) == O_CREAT){
                                KASSERT(NULL!= par_vnode->vn_ops->create); /* if file does not exist inside dir_vnode, need to make sure you can create the file */
                                dbg(DBG_PRINT, "(GRADING2A)\n");
                                dbg(DBG_PRINT, "(GRADING2B)\n");
                                int rv = par_vnode->vn_ops->create(par_vnode,name,namelen,&dir_vnode);
                                vput(par_vnode);
                                // vput(dir_vnode); //
                                *res_vnode = dir_vnode;
                                return rv;
                        }else{
                                dbg(DBG_PRINT, "(GRADING2B)\n");
                                vput(par_vnode);
                                // vput(dir_vnode); //
                                return -ENOENT;
                        }
                }
                
        }
        vput(par_vnode);
        *res_vnode = dir_vnode;

        dbg(DBG_PRINT, "(GRADING2A)\n");
        return 0;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
