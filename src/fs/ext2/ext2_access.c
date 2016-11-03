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

/* ext_access.c
 *
 * Authors: Brady Lee and David Williams
 * Based on the ext2 specification found at http://www.nongnu.org/ext2-doc/ext2.html
 *
 * Contains the lowest level methods for interacting with an ext2
 * filesystem image.
 *
 * Built for adding ext2 functionality to the Nautilus OS, Northwestern University
 */



#include "fs/ext2/ext2_access.h"
#include "fs/ext2/ext2fs.h"

#define INFO(fmt, args...) printk("EXT2_ACCESS: " fmt "\n", ##args)
#define DEBUG(fmt, args...) printk("EXT2_ACCESS (DEBUG): " fmt "\n", ##args)
#define ERROR(fmt, args...) printk("EXT2_ACCESS (ERROR): " fmt "\n", ##args)

#ifndef NAUT_CONFIG_DEBUG_FILESYSTEM
#undef DEBUG
#define DEBUG(fmt, args...)
#endif

/* get_super_block
 *
 * returns a pointer to the super block at start of fs
 */
struct ext2_super_block *get_super_block(void * fs) {

	//Super block is always a constant offset away from fs
	return (struct ext2_super_block*)(fs+SUPERBLOCK_OFFSET);
}

/* get_block_size
 *
 * returns the block size for a filesystem.
 */
uint32_t get_block_size(void *fs) {
	uint32_t shift;
	struct ext2_super_block* sb;
	sb = get_super_block(fs);

	//s_log_block_size tells how much to shift 1K by to determine block size
	shift = sb->s_log_block_size;
	return (1024 << shift);
}

/* get_block
 *
 * returns a pointer to a block given its block number.
 */

void * get_block(void * fs, uint32_t block_num) {
	uint32_t block_size;
	block_size = get_block_size(fs);

	//gets block size, adds block offset to beginning of fs
	return (void*)(block_size*block_num + fs);
}

/* get_block_group
 *
 * returns a pointer to the first block group descriptor.
 * Ext2 allows by definition for multiple groups. Currently, only supports one group
 */
struct ext2_group_desc *get_block_group(void * fs, uint32_t block_group_num) {
	struct ext2_super_block* sb;
	sb = get_super_block(fs);

	//returns location directly after superblock, which is where first block group descriptor resides 
	return (struct ext2_group_desc *)((void*)sb+SUPERBLOCK_SIZE);
}

/* get_inode
 *
 * returns a pointer to an inode, given its inode number
 */
struct ext2_inode* get_inode(void *fs, uint32_t inode_num) {
	struct ext2_group_desc* bg;
	uint32_t inode_table_num;
	void* inode_table;

	//get first (only) block group pointer
	bg = get_block_group(fs,1);

	//get index into inode table
	inode_table_num = bg->bg_inode_table;

	//gets pointer to block where inodes are located 
	inode_table = get_block(fs, inode_table_num);

	//Adds offset of inode within table to the beginning of the inode table pointer 
	return (struct ext2_inode*)((void*)inode_table + (inode_num -1)*sizeof(struct ext2_inode));
}

/* split_path
 *
 * returns an array of each part of a filepath
 * i.e. split_path("/a/b/c") = {"a", "b", "c"}
 * also puts the number of parts into the specified location for convenience
 */
char** split_path(char *path, int *num_parts) {
	int num_slashes = 0;
	for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
		num_slashes++;
	}
	*num_parts = num_slashes;
	// Copy out each piece by advancing two pointers (piece_start and slash).
	char **parts = (char **)malloc(num_slashes*sizeof(char *));
	char *piece_start = path + 1;
	int i = 0;
	for (char *slash = strchr(path + 1, '/'); slash != NULL; slash = strchr(slash + 1, '/')) {
		int part_len = slash - piece_start;
		parts[i] = (char *) malloc((part_len + 1)*sizeof(char));
		strncpy(parts[i], piece_start, part_len);
		piece_start = slash + 1;
		i++;
	}
	// Get the last piece.
	parts[i] = (char *)malloc((strlen(piece_start) + 1)*sizeof(char));
	strcpy(parts[i], " ");
	strcpy(parts[i], piece_start);
	return parts;
}

/* get_root_dir
 *
 * Convenience function to get pointer to inode of the root directory.
 */
struct ext2_inode* get_root_dir(void * fs) {
	return get_inode(fs, EXT2_ROOT_INO);
}

/* get_inode_from_dir
 *
 * searches a directory for a file by name
 * returns the inode number of the file if found, or 0
 */
uint32_t get_inode_from_dir(void *fs, struct ext2_inode *dir, char *name) {
	char *tempname;

	//get pointer to block where directory entries reside using inode of directory
	struct ext2_dir_entry_2* dentry = (struct ext2_dir_entry_2*)get_block(fs,dir->i_block[0]);

	//scan only valid files in dentry
	//while(dentry->inode){
	ssize_t blocksize = get_block_size(fs) - dentry->rec_len;
	int count = 0;
	while(blocksize > 0) {
	//while(++count < 20) {
		int i;
		//DEBUG("malloc %d", dentry->name_len);
		tempname = (char*)malloc(dentry->name_len*sizeof(char));

		//copy file name to temp name, then null terminate it
		for(i = 0; i<dentry->name_len;i++){
			tempname[i] = dentry->name[i];
		}
		tempname[i] = '\0';

		//if there is a match, return the inode number
		//DEBUG("INODE DIR: name %s, tempname %s", name, tempname);
		//DEBUG("INODE DIR: blocksize %d, reclen %d", blocksize, dentry->rec_len);
		if(strcmp(name,tempname) ==0){
			free((void*)tempname);
			return dentry->inode;
		}
		//else, go to next dentry entry 
		dentry = (struct ext2_dir_entry_2*)((void*)dentry+dentry->rec_len);
		blocksize -= dentry->rec_len;

		free((void*)tempname);
	}
	return 0;
}

/* get_inode_by_path
 *
 * given a path to a file, tries to find the inode number of the file
 * returns inode number of file if found, or 0
 */
uint32_t get_inode_by_path(void *fs, char *path) {
	int num_parts = 0;
	if (!strcmp(path, "/")) {
		return EXT2_ROOT_INO;
	}
	char **parts = split_path(path, &num_parts);
	char *cur_part = *parts;
	int i;
	struct ext2_inode* cur_inode = get_root_dir(fs);
	uint32_t new_inode_num = 0;

	DEBUG("GET INODE: %s has %d parts", path, num_parts);

	for(i = 1; i <= num_parts; i++){
		//treat current inode as directory, and search for inode of next part
		new_inode_num = get_inode_from_dir(fs, cur_inode, cur_part);
		if (!new_inode_num) {
			ERROR("GET INODE: inode == 0");
			return 0;
		}
		DEBUG("GET INODE found: %d, %s", new_inode_num, cur_part);
		cur_inode = get_inode(fs, new_inode_num);
		cur_part = *(parts+i);
	}

	//final inode is the requested file. return its number
	DEBUG("GET INODE returning: %d", new_inode_num);
	return new_inode_num;
}

/* alloc_inode
 *
 * Searches the inode bitmap in the front of the block group for a free inode
 * also sets the inode to taken in the bitmap when found.
 * returns the inode number of the first free inode, or 0
 */
uint32_t alloc_inode(void *fs) {
	struct ext2_group_desc *bg = get_block_group(fs, 1);	//get block group descriptor
	uint8_t *bitmap_byte = get_block(fs, bg->bg_inode_bitmap);	//get inode bitmap first byte
	int bytes_checked = 0;
	int bit;
	uint8_t cur_byte;

	while(bytes_checked < 1024) {		//cycle through bitmap
		cur_byte = *bitmap_byte;
		for(bit=0; bit < 8; bit++) {
			if(!(cur_byte & 0x01)) {
				*bitmap_byte = *bitmap_byte | (0x01 << bit); //set the inode to taken
				return (uint32_t)(bytes_checked*8 + bit + 1); //first inode number is 1, not 0
			}
			cur_byte = cur_byte >> 1;
		}
		bitmap_byte++;
		bytes_checked++;
	}
	return 0;
}

/* free_inode
 *
 * sets an inode to free in the inode bitmap at front of block group
 * returns 1
 */
int free_inode(void *fs, uint32_t inum) {
	struct ext2_group_desc *bg = get_block_group(fs, 1);	//get block group descriptor
	uint8_t *bitmap_byte = get_block(fs, bg->bg_inode_bitmap);	//get inode bitmap first byte
	int test = inum-1;
	DEBUG("%d",test/8);
	bitmap_byte += (test/ 8);
	int bit = (test % 8);
	DEBUG("* %d %x",bit, *bitmap_byte);
	uint8_t mask = ~(1 << (bit));
	*bitmap_byte &= mask;
	DEBUG("* %x %x",mask, *bitmap_byte);
	return 1;
}



