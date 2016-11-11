/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Brady Lee and David Williams
 * Copyright (c) 2016, Peter Dinda
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Brady Lee <BradyLee2016@u.northwestern.edu>
 *           David Williams <davidwilliams2016@u.northwestern.edu>
 *           Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __FS_H__
#define __FS_H__

#include <nautilus/naut_types.h>
#include <nautilus/printk.h>
#include <nautilus/list.h>
#include <nautilus/spinlock.h>

#include <fs/ext2/ext2.h>


#define FS_NAME_LEN     32
#define MOUNT_PATH_LEN  1024

struct nk_fs_stat {
    // to be expanded as we go
    // trying to keep things somewhat compatible with user-level stat
    uint64_t st_size;
};

// Abstract base class for a filesystem interface
struct nk_fs_int {
    int   (*stat_path)(void *state, char *path, struct nk_fs_stat *st);
    void *(*create_file)(void *state, char *path);
    int   (*create_dir)(void *state, char *path);
    int   (*exists)(void *state, char *path);
    int   (*remove)(void *state, char *path);
    void *(*open_file)(void *state, char *path);
    int   (*stat)(void *state, void *file, struct nk_fs_stat *st);
    int   (*trunc_file)(void *state, void *file, off_t len);
    ssize_t  (*read_file)(void *state, void *file, void *dest, off_t offset, size_t n);
    ssize_t  (*write_file)(void *state, void *file, void *src, off_t offset, size_t n);
    void  (*close_file)(void *state, void *file);
};

// This is the class for a filesystem.  It should be the first
// member of any specific type of filesystem
struct nk_fs {
    char              name[FS_NAME_LEN];           // colon-terminated name
    char              mount_path[MOUNT_PATH_LEN];  // for future mount point model, maybe... 
    struct list_head  fs_list_node;

    uint64_t          flags;
#define NK_FS_READONLY   1

    void             *state;  // internal FS state
    struct nk_fs_int *interface;
    
};

int nk_fs_init();
int nk_fs_deinit();

struct nk_fs *nk_fs_register(char *name, uint64_t flags, struct nk_fs_int *inter, void *state);
int           nk_fs_unregister(struct nk_fs *fs);

struct nk_fs *nk_fs_find(char *name);

void nk_fs_dump_filesystems();
void nk_fs_dump_files();

//
// The interface emulates the Unix interface as closely as possible
// 
// Currently, a path here includes the fs name:
//
// C:/foo/bar/baz => fs named "C", path on fs is /foo/bar/baz
// /foo/bar/haz => fs named "rootfs", path on fs is /foo/bar/baz
//
// Yes, this is DOS style...
// Support for mount points does not exist yet
//

typedef struct nk_fs_open_file_state *nk_fs_fd_t;
#define FS_BAD_FD ((nk_fs_fd_t)-1UL)
#define FS_FD_ERR(fd) ((fd)==FS_BAD_FD)

int        nk_fs_stat(char *path, struct nk_fs_stat *st);
int        nk_fs_truncate(char *path, off_t len);
#define O_RDONLY 1
#define O_WRONLY 2
#define O_RDWR   3 // OR of RD and WR ONLY
#define O_APPEND 4
#define O_CREAT  8
#define O_TRUNC  16 // guess
nk_fs_fd_t nk_fs_creat(char *path, int mode);
nk_fs_fd_t nk_fs_open(char *path, int flags, int mode);
int        nk_fs_fstat(nk_fs_fd_t fd, struct nk_fs_stat *st);
int        nk_fs_ftruncate(nk_fs_fd_t fd, off_t len);
off_t      nk_fs_seek(nk_fs_fd_t fd, off_t position, int whence);
ssize_t    nk_fs_tell(nk_fs_fd_t fd);
ssize_t    nk_fs_read(nk_fs_fd_t fd, void *buf, size_t len);
ssize_t    nk_fs_write(nk_fs_fd_t fd, void *buf, size_t len);
int        nk_fs_close(nk_fs_fd_t fd);


void test_fs(void);
void init_fs(void);
void deinit_fs(void);


// Temporary hackery to get this to compile with
// the reorg
extern uint8_t __RAMDISK_START, __RAMDISK_END;
#define RAMFS_START __RAMDISK_START
#define RAMFS_END __RAMDISK_END

#endif
