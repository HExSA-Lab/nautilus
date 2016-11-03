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
#include <nautilus/fs.h>
#include <fs/ext2/ext2.h>

#define INFO(fmt, args...) printk("EXT2: " fmt "\n", ##args)
#define DEBUG(fmt, args...) printk("EXT2 (DEBUG): " fmt "\n", ##args)
#define ERROR(fmt, args...) printk("EXT2 (ERROR): " fmt "\n", ##args)

#ifndef NAUT_CONFIG_DEBUG_FILESYSTEM
#undef DEBUG
#define DEBUG(fmt, args...)
#endif

static uint16_t ext2_dentry_find_len(struct ext2_dir_entry_2 *dentry);
static int ext2_inode_has_mode(struct ext2_inode *inode, int mode);
static int dentry_add(uint8_t *fs, int dir_inum, int target_inum, char *name, uint8_t file_type);
static int dentry_remove(uint8_t *fs, int dir_inum, int target_inum);

uint32_t ext2_open(uint8_t *fs, char *path, int access) {
	uint32_t inode_num = (uint32_t) get_inode_by_path(fs, path);
	return inode_num;
}

ssize_t ext2_read(uint8_t *fs, int inode_number, char *buf, size_t num_bytes,size_t offset) {
	struct ext2_inode * inode_pointer = get_inode(fs, inode_number);
	int block_offset = offset/get_block_size(fs);
	int offset_remainder = offset-(get_block_size(fs)*block_offset);
	int bytes_from_first = get_block_size(fs)-offset_remainder;
	int blocks_to_read = 0;
	int remainder_to_read = 0;
	if (num_bytes > bytes_from_first) {
		int temp = num_bytes;
		temp -= bytes_from_first;
		blocks_to_read = temp/get_block_size(fs);
		remainder_to_read = temp-(get_block_size(fs)*blocks_to_read);
	}
	else {
		bytes_from_first = num_bytes;
	}

	unsigned char * blocks = (char *)(get_block(&RAMFS_START, inode_pointer->i_block[block_offset]));
	int total_bytes_read = 0;	
	uint8_t * cur = blocks + offset_remainder;
	int i = 0;
	if(ext2_get_size(fs, inode_number) == 0) {
		return 0;
	}
	
	//If trying to read past the first 12 blocks, end early
	if ((block_offset + blocks_to_read + (remainder_to_read != 0)) >= NUM_DATA_BLOCKS) {
		if (block_offset < NUM_DATA_BLOCKS) {
			blocks_to_read = NUM_DATA_BLOCKS - block_offset -1;
		}
		else {
			blocks_to_read = 0;
			bytes_from_first = 0;
		}
		remainder_to_read = 0;
	}
	printk("read info: %d %d %d %d %d\n", block_offset, offset_remainder, bytes_from_first, blocks_to_read, remainder_to_read);
	//Read to end of first partial block
	while (i<bytes_from_first && *cur != ENDFILE) {		
		//DEBUG("EXT2 READ: char %d %c", *cur, *cur);
		buf[total_bytes_read] = *cur;		
		i++;
		total_bytes_read++;
		cur++;
	}
	//Read as many full blocks as needed
	while (blocks_to_read>0) {
		block_offset++;
		cur = (char*)(get_block(&RAMFS_START,inode_pointer->i_block[block_offset]));
		i = 0;	
		while (i<get_block_size(fs) && *cur != ENDFILE) {		
			buf[total_bytes_read] = *cur;		
			i++;
			cur++;
			total_bytes_read++;
		}
		blocks_to_read--;
	}
	i = 0;
	block_offset++;
	cur = (char*)(get_block(&RAMFS_START,inode_pointer->i_block[block_offset]));
	//Read last partial block
	while (i < remainder_to_read && *cur != ENDFILE) {			
		//printk("Reading Block: %d\n",block_offset);
		buf[total_bytes_read] = *cur;		
		i++;
		total_bytes_read++;
		cur++;
	}
	buf[total_bytes_read] = 0x00; //null terminator
	return total_bytes_read;
}

ssize_t ext2_write(uint8_t *fs, int inode_number, char *buf, size_t num_bytes, size_t offset) {
	struct ext2_inode * inode_pointer = get_inode(fs, inode_number);

	int block_offset = offset/get_block_size(fs);
	int offset_remainder = offset-(get_block_size(fs)*block_offset);
	int bytes_from_first = get_block_size(fs)-offset_remainder;
	int blocks_to_write = 0;
	int remainder_to_write = 0;
	if (num_bytes > bytes_from_first) {
		int temp = num_bytes;
		temp -= bytes_from_first;
		blocks_to_write = temp/get_block_size(fs);
		remainder_to_write = temp-(get_block_size(fs)*blocks_to_write);
	}
	else {
		bytes_from_first = num_bytes;
	}
	unsigned char * blocks = (char *)(get_block(&RAMFS_START, inode_pointer->i_block[block_offset]));
	int i = 0;	
	int total_bytes_written = 0;
	int end_flag = 0;
	int end_bytes = 0;
	uint8_t * cur = blocks + offset_remainder;
	printk("file size: %d\n", ext2_get_size(fs, inode_number));
	if(ext2_get_size(fs, inode_number) == 0) {
		end_flag = 1;
		end_bytes = num_bytes;
	}

	//If trying to write past the first 12 blocks, end early
	if ((block_offset + blocks_to_write + (remainder_to_write != 0)) >=12) {
		if (block_offset < NUM_DATA_BLOCKS) {
			blocks_to_write = NUM_DATA_BLOCKS - block_offset - 1;
		}
		else {
			blocks_to_write = 0;
			bytes_from_first = 0;
		}
		remainder_to_write = 0;
	}
	printk("write info: %d %d %d %d %d\n", block_offset, offset_remainder, bytes_from_first, blocks_to_write, remainder_to_write);

	//Write to end of first partial block
	while (total_bytes_written<bytes_from_first) {		
		if(*cur == ENDFILE && !end_flag) {
			end_flag = 1;
			end_bytes = num_bytes-total_bytes_written;
		}
		*cur = buf[total_bytes_written];		
		total_bytes_written++;
		cur++;
	}
	//Write as many full blocks as needed
	while (blocks_to_write>0) {
		block_offset++;
		cur = (char*)(get_block(&RAMFS_START,inode_pointer->i_block[block_offset]));
		i = 0;	
		while (i<get_block_size(fs)) {		
			if(*cur == ENDFILE && !end_flag) {
				end_flag = 1;
				end_bytes = num_bytes-total_bytes_written;
			}
			*cur = buf[total_bytes_written];		
			i++;
			cur++;
			total_bytes_written++;
		}
		blocks_to_write--;
	}
	i = 0;
	block_offset++;
	if(i<remainder_to_write) {
	cur = (char*)(get_block(&RAMFS_START,inode_pointer->i_block[block_offset]));
	}
	//Write last partial block
	while (i<remainder_to_write) {			
		//printk("Writing Block: %d\n",block_offset);
		if(*cur == ENDFILE && !end_flag) {
			end_flag = 1;
			end_bytes = num_bytes-total_bytes_written;
		}
		*cur = buf[total_bytes_written];	
		i++;
		total_bytes_written++;
		cur++;
	}
	DEBUG("Writing reached end %d", end_flag);
	if(end_flag) {
		*cur = ENDFILE;
		inode_pointer->i_size += end_bytes;
	}
	return total_bytes_written;
}

/*
size_t ext2_get_size(uint8_t *fs, int inum) {
	struct ext2_inode *inode = get_inode(fs, inum);	
	if (ext2_inode_has_mode(inode, EXT2_S_IFREG))
		return ext2_get_size(fs, inum);
	if (ext2_inode_has_mode(inode, EXT2_S_IFDIR))
		return ext2_get_dir_size(fs, inum);
	return 0;
}
*/

size_t ext2_get_size(uint8_t *fs, int inum) {
	struct ext2_inode *inode = get_inode(fs, inum);	
	uint64_t temp = inode->i_size_high;
	temp = temp << 32;
	temp = temp | (inode->i_size);
	return temp;
}

/*
size_t ext2_get_dir_size(uint8_t *fs, int inum) {
	struct ext2_inode *dir = get_inode(fs, inum);
	void *block = get_block(fs, dir->i_block[0]);
	ssize_t blocksize = get_block_size(fs);
	ssize_t new_blocksize = 0;

	// find target dentry
	struct ext2_dir_entry_2 *dentry = (struct ext2_dir_entry_2 *)block;
	struct ext2_dir_entry_2 *target = NULL;
	struct ext2_dir_entry_2 *prev = NULL;
	int count = 0;
	blocksize -= dentry-> rec_len;
	while (blocksize > 0) { 
		dentry = (struct ext2_dir_entry_2*)((uint8_t*)dentry + dentry->rec_len);
		new_blocksize = blocksize - dentry->rec_len;
		if (new_blocksize <= 0) {
			return blocksize + ext2_dentry_find_len(dentry);
		}
		blocksize = new_blocksize;
	}
	return 0;
}
*/

int ext2_exists(uint8_t *fs, char *path) {
	uint32_t inode_num = (uint32_t)get_inode_by_path(fs, path);
	return inode_num != 0;
}

// return new inode number; 0 if failed
uint32_t ext2_create_file(uint8_t *fs, char *path) {
	uint32_t free_inode = alloc_inode(fs);
	DEBUG("EXT2 CREATE: Allocated node %d", free_inode);

	if (free_inode) {
		struct ext2_inode *inode_pointer = get_inode(fs, free_inode);

		// get directory path
		int num_parts = 0;
		char **parts = split_path(path, &num_parts);
		if (!num_parts) {
			return 0;
		}
		int newstring_size = 0;
		for (int i=0; i < num_parts-1; i++) {
			newstring_size += strlen(parts[i]) + 1;
		}
		char *newstring = malloc(newstring_size);
		strcpy(newstring, "/");
		for (int i=0; i < num_parts-1; i++) {
			strcat(newstring,parts[i]);
			if (i != num_parts-2) {
				strcat(newstring, "/");
			}
		}

		// get inode of directory
		char *name = parts[num_parts-1];
		int dir_num = 0;
		if(strcmp(newstring, "/")) {
			dir_num = get_inode_by_path(fs, newstring);
		}
		else {
			dir_num = EXT2_ROOT_INO;
		}

		// create dentry
		if (dir_num) {
			dentry_add(fs, dir_num, free_inode, name, 1);
			//fill in inode with stuff
			inode_pointer->i_mode = EXT2_S_IFREG;
			inode_pointer->i_size = 0;
			//set bitmap to taken
		}
		else {
			return 0;
		}
	}
	return free_inode;
}

static int ext2_inode_has_mode(struct ext2_inode *inode, int mode) {
	return ((inode->i_mode & mode) == mode);
}

int ext2_remove_file(uint8_t *fs, char *path) {
	uint32_t inum = get_inode_by_path(fs, path);
	if (!inum) {
		DEBUG("Bad path");
		return 0;
	}
	struct ext2_inode *inode = get_inode(fs, inum);
	// check if file
	if (!ext2_inode_has_mode(inode, EXT2_S_IFREG)) {
		DEBUG("FILE DELETE: Not a file %x", inode->i_mode);
		return 0;
	}
	// get directory path
	int num_parts = 0;
	char **parts = split_path(path, &num_parts);
	if (!num_parts) {
		DEBUG("No parts");
		return 0;
	}
	int newstring_size = 0;
	for (int i=0; i < num_parts-1; i++) {
		newstring_size += strlen(parts[i]) + 1;
	}
	
	char* newstring = malloc(newstring_size);
	strcpy(newstring, "/");
	for (int i=0; i < num_parts-1; i++) {
	        strcat(newstring,parts[i]);
	        if (i != num_parts-2) {
		        strcat(newstring, "/");
	        }
	}

	// get inode of directory
	int dir_num = 0;
	if(strcmp(newstring, "/")) {
		dir_num = get_inode_by_path(fs, newstring);
	}
	else {
		dir_num = EXT2_ROOT_INO;
	}

	// create dentry
	if (dir_num) {
		dentry_remove(fs, dir_num, inum);
	}
	else {
		DEBUG("Bad dir path");
		return 0;
	}

	return 1;
}

static uint16_t ext2_dentry_find_len(struct ext2_dir_entry_2 *dentry) {
	uint16_t len = (uint16_t)(sizeof(dentry->inode) + sizeof(dentry->name_len) + sizeof(dentry->file_type) + dentry->name_len + sizeof(uint16_t));
	//DEBUG("Dentry Find Len: %s", dentry->name);
	return (len += (DENTRY_ALIGN - len % DENTRY_ALIGN));
}

static int dentry_add(uint8_t *fs, int dir_inum, int target_inum, char *name, uint8_t file_type) {
	struct ext2_inode *dir = get_inode(fs,dir_inum);

	//create directory entry
	struct ext2_dir_entry_2 new_entry;
	new_entry.inode = target_inum;
	new_entry.name_len = (uint8_t) strlen(name);
	new_entry.file_type = file_type;
	strcpy(new_entry.name, name);
	new_entry.rec_len = ext2_dentry_find_len(&new_entry);
	dir->i_size+= new_entry.rec_len;
	//new_entry.rec_len = (uint16_t)(sizeof(new_entry.inode) + sizeof(new_entry.name_len) + sizeof(new_entry.file_type) + strlen(name) + sizeof(uint16_t));
	DEBUG("Dir Add: %s, reclen %d", name, new_entry.rec_len);

	void *block = get_block(fs, dir->i_block[0]);
	ssize_t blocksize = get_block_size(fs);

	// iterate to last dentry
	struct ext2_dir_entry_2 *dentry = (struct ext2_dir_entry_2 *)block;
	blocksize -= dentry-> rec_len;
	while (blocksize > 0) { 
		dentry = (struct ext2_dir_entry_2*)((uint8_t*)dentry + dentry->rec_len);
		blocksize -= dentry->rec_len;
	}

	// adjust rec_lens
	uint16_t last_len = ext2_dentry_find_len(dentry);
	new_entry.rec_len = dentry->rec_len - last_len; 
	dentry->rec_len = last_len;

	// place entry
	// TODO: check new entry fits
	void *dst = (void*)((uint8_t*)dentry + last_len);
	DEBUG("DIR ADD: Block at %p, place at %p", block, dst);
	*(struct ext2_dir_entry_2 *)dst = new_entry;
	return 1;
}

static int dentry_remove(uint8_t *fs, int dir_inum, int target_inum) {
	struct ext2_inode *dir = get_inode(fs, dir_inum);
	void *block = get_block(fs, dir->i_block[0]);
	ssize_t blocksize = get_block_size(fs);

	// find target dentry
	struct ext2_dir_entry_2 *dentry = (struct ext2_dir_entry_2 *)block;
	struct ext2_dir_entry_2 *target = NULL;
	struct ext2_dir_entry_2 *prev = NULL;
	int count = 0;
	blocksize -= dentry-> rec_len;
	while (blocksize > 0) { 
		prev = dentry;
		dentry = (struct ext2_dir_entry_2*)((uint8_t*)dentry + dentry->rec_len);
		blocksize -= dentry->rec_len;
		//DEBUG("Dir Remove: dentry %p, inode %d, length %d, lenex %d, cursize %d", dentry, dentry->inode, dentry->rec_len, ext2_dentry_find_len(dentry), blocksize);
		if (dentry->inode == target_inum) {
			target = dentry;
			//DEBUG("Dir Remove: Found dentry");
		}
	}
	if (blocksize) {
		//ERROR("DIR REMOVE FILE: rec_len values not aligned %d", blocksize);
	}
	int is_target_end = 0;
	if (target == dentry) {
		is_target_end = 1;
		//DEBUG("DIR REMOVE: End target");
	}
	//DEBUG("Dir Remove: size %d", blocksize);
	if (target == NULL) {
		ERROR("Dir Remove: Did not find dentry!");
		return 0;
	}
	size_t dir_size = (uint64_t)dentry - (uint64_t)block + ext2_dentry_find_len(dentry); 
	//DEBUG("Dir Remove: last reclen %d", ext2_dentry_find_len(dentry)); 
	//DEBUG("Dir Remove: dir_size %d, %x", dir_size, dir_size); 

	if (is_target_end) {
		prev->rec_len += target->rec_len;
	}
	else {
		// move remaining dentries back
		dentry->rec_len += target->rec_len;
		void *move_dst = (void*)((uint8_t*)target);
		void *move_src = (void*)((uint8_t*)target + target->rec_len);
		size_t move_len = (uint64_t)block + dir_size - (uint64_t)move_src;
		//DEBUG("Dir Remove: move %p %p", move_dst, move_src);
		//DEBUG("Dir Remove: move len %d", move_len);
		memmove(move_dst, move_src, move_len);
	}

	// update terminator
	dir_size -= target->rec_len;
	dir->i_size -= dir_size;
	free_inode(fs, target_inum);
	return 1;
}
