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
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Brady Lee <BradyLee2016@u.northwestern.edu>
 *           David Williams <davidwilliams2016@u.northwestern.edu>
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

#define O_RDONLY 1
#define O_WRONLY 2
#define O_RDWR 3 // OR of RD and WR ONLY
#define O_APPEND 4
#define O_CREAT 8
// there are more ...

enum Filesystem {ext2, fat32};

struct filesystem {
	char *path;
	struct block_dev *device;
	enum Filesystem fs_type;
};

struct file_int {
	uint32_t (*open)(uint8_t*, char*, int);
	ssize_t (*read)(uint8_t*, int, char*, size_t, size_t);
	ssize_t (*write)(uint8_t*, int, char*, size_t, size_t);
	size_t (*get_size)(uint8_t*, int);
};

struct file {
	struct list_head file_node;
	struct file_int interface;
	size_t position;
	uint32_t filenum;
	uint32_t fileid;
	int access;
	spinlock_t lock;
};

void mount(char *source, char *target);
void umount(char *target);

void test_fs(void);
void init_fs(void);
void deinit_fs(void);


// Temporary hackery to get this to compile with
// the reorg
extern uint8_t __RAMDISK_START, __RAMDISK_END;
#define RAMFS_START __RAMDISK_START
#define RAMFS_END __RAMDISK_END

#endif
