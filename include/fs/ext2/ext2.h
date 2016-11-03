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

#ifndef __FS_EXT2_H__
#define __FS_EXT2_H__

#include <fs/ext2/ext2_access.h>

#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFDIR 0x4000
#define BLOCK_SIZE 1024
#define DENTRY_ALIGN 4
#define NUM_DATA_BLOCKS 12
#define ENDFILE 0xa0

//static uint8_t EOF = 255;

uint32_t ext2_open(uint8_t *device, char *path, int access);
ssize_t ext2_read(uint8_t *device, int inode_number, char *buf, size_t num_bytes, size_t offset);
ssize_t ext2_write(uint8_t *device, int inode_number, char *buf, size_t num_bytes, size_t offset);
size_t ext2_get_size(uint8_t *device, int inode_number);

uint32_t ext2_create_file(uint8_t *device, char *path);
int ext2_remove_file(uint8_t *device, char *path);
int ext2_exists(uint8_t *device, char *path);

#endif
