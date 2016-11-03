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
#ifndef __EXT2_ACCESS_H__
#define __EXT2_ACCESS_H__

#include "nautilus/naut_types.h"
#include "nautilus/printk.h"
#include "fs/ext2/ext2fs.h"

struct ext2_super_block *get_super_block(void *fs);
uint32_t get_block_size(void *fs);
void* get_block(void *fs, uint32_t block_num);
struct ext2_group_desc *get_block_group(void *fs, uint32_t block_group_num);
struct ext2_inode *get_inode(void *fs, uint32_t inode_num);
char** split_path(char *path, int *num_parts); 
struct ext2_inode *get_root_dir(void *fs);
uint32_t get_inode_from_dir(void *fs, struct ext2_inode *dir, char *name);
uint32_t get_inode_by_path(void *fs, char *path);
uint32_t alloc_inode(void *fs);
int free_inode(void *fs, uint32_t inum);

#endif

