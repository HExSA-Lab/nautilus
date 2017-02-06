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
#include <nautilus/nautilus.h>
#include <nautilus/dev.h>
#include <nautilus/blkdev.h>
#include <nautilus/fs.h>

#include <fs/ext2/ext2.h>
#include "ext2fs.h"

#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFDIR 0x4000
#define BLOCK_SIZE 1024
#define DENTRY_ALIGN 4
#define NUM_DIRECT_DATA_BLOCKS 12
#define ENDFILE 0xa0

#define INFO(fmt, args...)  INFO_PRINT("ext2: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ext2: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("ext2: " fmt, ##args)

#ifndef NAUT_CONFIG_DEBUG_EXT2_FILESYSTEM_DRIVER
#undef DEBUG
#define DEBUG(fmt, args...)
#endif


struct ext2_state {
    struct nk_block_dev_characteristics chars; 
    struct nk_block_dev *dev;
    struct nk_fs        *fs;
    struct ext2_super_block super;
};

#include "ext2_access.c"

static size_t get_file_size(struct ext2_state *fs, struct ext2_inode *inode) 
{
    size_t temp = inode->i_size_high;
    temp = temp << 32;
    temp = temp | (inode->i_size);
    return temp;
}

static void set_file_size(struct ext2_state *fs, struct ext2_inode *inode, size_t size) 
{
    size_t temp = inode->i_size_high;
    inode->i_size_high = size>>32;
    inode->i_size = size & 0xffffffff;
}


static void * ext2_open(void *state, char *path) 
{
    struct ext2_state *fs = (struct ext2_state *)state;

    uint32_t inode_num = get_inode_num_by_path(fs, path);

    DEBUG("open of %s returned inode number %u\n",path,inode_num);

    // ideally FS would track this here so that we can handle multiple
    // opens, locking, etc correctly, but that's outside of scope for now

    return (void*)(uint64_t)inode_num;
}


static void ext2_close(void *state, void *file) 
{
    struct ext2_state *fs = (struct ext2_state *)state;

    DEBUG("closing inode %u\n",(uint32_t)(uint64_t)file);

    // ideally FS would track this here so that we can handle multiple
    // opens, locking, etc correctly, but that's outside of scope for now

}

static int ext2_exists(void *state, char *path) 
{
    struct ext2_state *fs = (struct ext2_state *)state;

    DEBUG("exists(%s,%s)?\n",fs->fs->name,path);
    uint32_t inode_num = get_inode_num_by_path(fs, path);
    return inode_num != 0;
}


static int map_logical_to_physical_get_put(struct ext2_state *fs, uint32_t inode_num, struct ext2_inode *their_inode, uint32_t logical_block, uint32_t *physical_block, int put)
{
    uint64_t block_size = get_block_size(fs);
    uint64_t ptrs_per_block = block_size/4;
    uint64_t num_direct = NUM_DIRECT_DATA_BLOCKS;
    uint64_t num_single = ptrs_per_block;
    uint64_t num_double = ptrs_per_block*ptrs_per_block;
    uint64_t num_triple = ptrs_per_block*ptrs_per_block*ptrs_per_block;
    struct ext2_inode our_inode;
    struct ext2_inode *inode=0;
    uint64_t left = logical_block;
    uint32_t  next, cur;
    uint8_t   buf[block_size];
    uint32_t  *ptrs = (uint32_t*)buf;
    uint8_t   temp[block_size];
	
    if (!their_inode) { 
	if (read_inode(fs,inode_num,&our_inode)) {
	    ERROR("Failed to read inode\n");
	    return -1;
	}
	inode = &our_inode;
    } else {
	inode = their_inode;
    }

    if (left < num_direct) { 
	//DEBUG("Direct map (%lu)\n", logical_block);
	if (put) {
	    inode->i_block[logical_block] = *physical_block;
	    if (write_inode(fs,inode_num,inode)) { 
		ERROR("Failed to write inode\n");
		return -1;
	    }
	    return 0;
	} else {
	    *physical_block = inode->i_block[logical_block];
	    return 0;
	} 
    } else {
	//DEBUG("Indirect map (%lu)\n", logical_block);
	left -= num_direct;
	if (left<num_single) {
	    // 1 indirect
	    next = inode->i_block[num_direct];
	    if (!next) { 
		if (put) { 
		    if (alloc_block(fs,&next)) {  
			ERROR("Failed to allocate new indirect block\n");
			return -1;
		    }
		    memset(temp,0,sizeof(temp));
		    if (write_block(fs,next,temp)) { 
			ERROR("Failed to write new indirect block\n");
			return -1;
		    }
		    inode->i_block[num_direct] = next;
		    if (write_inode(fs,inode_num,inode)) { 
			ERROR("Failed to write inode with new indirect block\n");
			return -1;
		    }
		} else {
		    ERROR("required indirect block does not exist\n");
		    return -1;
		}
	    }
	    if (read_block(fs,next,buf)) { 
		ERROR("Cannot read indirect block\n");
		return -1;
	    }
	    if (put) { 
		ptrs[left%num_single] = *physical_block;
		if (write_block(fs,next,buf)) { 
		    ERROR("Failed to write indirect block\n");
		    return -1;
		}
		return 0;
	    } else {
		*physical_block = ptrs[left%num_single];
		return 0;
	    }

	} else {
	    // >1 indirect
	    left -= num_single;
	    if (left<num_double) { 
		//DEBUG("Double indirect map (%lu)\n", logical_block);
		// 2 indirect
		next = inode->i_block[num_direct+1];
		if (!next) { 
		    if (put) { 
			if (alloc_block(fs,&next)) {  
			    ERROR("Failed to allocate new 2-indirect block (1st)\n");
			    return -1;
			}
			memset(temp,0,sizeof(temp));
			if (write_block(fs,next,temp)) { 
			    ERROR("Failed to write new 2-indirect block (1st)\n");
			    return -1;
			}
			inode->i_block[num_direct+1] = next;
			if (write_inode(fs,inode_num,inode)) { 
			    ERROR("Failed to write inode with new 2-indirect block (1st)\n");
			    return -1;
			}
		    } else {
			ERROR("required 2-indirect block (1st) does not exist\n");
			return -1;
		    }
		}
		if (read_block(fs,next,buf)) { 
		    ERROR("Cannot read double indirect block (1st)\n");
		    return -1;
		} 
		cur = next;
		next = ptrs[left/num_single];
		if (!next) { 
		    if (put) {
			if (alloc_block(fs,&next)) {  
			    ERROR("Failed to allocate new 2-indirect block (2nd)\n");
			    return -1;
			}
			memset(temp,0,sizeof(temp));
			if (write_block(fs,next,temp)) { 
			    ERROR("Failed to write new 2-indirect block (2nd)\n");
			    return -1;
			}
			ptrs[left/num_single] = next;
			if (write_block(fs,cur,buf)) { 
			    ERROR("Failed to update for new 2-indirect block (2nd)\n");
			    return -1;
			}
		    } else {
			ERROR("required 2-indirect block (2nd step) does not exist\n");
			return -1;
		    }
		}
		if (read_block(fs,next,buf)) { 
		    ERROR("Cannot read double indirect block (2nd)\n");
		    return -1;
		}
		if (put) { 
		    ptrs[left%num_single] = *physical_block;
		    if (write_block(fs,next,buf)) {
			ERROR("Failed to write 2-indirect mapping\n");
			return -1;
		    }
		    return 0;
		} else {
		    *physical_block = ptrs[left%num_single];
		    return 0;
		}
	    } else {
		// >2 indirect
		left -= num_double;
		if (left < num_triple) {
		    //DEBUG("Triple indirect map (%lu)\n", logical_block);
		    // 3 indirect
		    next = inode->i_block[num_direct+2];
		    if (!next) { 
			if (put) { 
			    if (alloc_block(fs,&next)) {  
				ERROR("Failed to allocate new 3-indirect block (1st)\n");
				return -1;
			    }
			    memset(temp,0,sizeof(temp));
			    if (write_block(fs,next,temp)) { 
				ERROR("Failed to write new 3-indirect block (1st)\n");
				return -1;
			    }
			    inode->i_block[num_direct+2] = next;
			    if (write_inode(fs,inode_num,inode)) { 
				ERROR("Failed to write inode with new 3-indirect block (1st)\n");
				return -1;
			    }
			} else {
			    ERROR("required 3-indirect block (1st) does not exist\n");
			    return -1;
			}
		    }
		    if (read_block(fs,next,buf)) { 
			ERROR("Cannot read triple indirect block (1st)\n");
			return -1;
		    } 
		    cur = next;
		    next = ptrs[left/num_double];
		    if (!next) { 
			if (put) {
			    if (alloc_block(fs,&next)) {  
				ERROR("Failed to allocate new 3-indirect block (2nd)\n");
				return -1;
			    }
			    memset(temp,0,sizeof(temp));
			    if (write_block(fs,next,temp)) { 
				ERROR("Failed to write new 3-indirect block (2nd)\n");
				return -1;
			    }
			    ptrs[left/num_double] = next;
			    if (write_block(fs,cur,buf)) { 
				ERROR("Failed to update for new 3-indirect block (2nd)\n");
				return -1;
			    }
			} else {
			    ERROR("required 3-indirect block (2nd step) does not exist\n");
			    return -1;
			}
		    }
		    if (read_block(fs,next,buf)) { 
			ERROR("Cannot read triple indirect block (2nd)\n");
			return -1;
		    }
		    cur = next;
		    next = ptrs[(left%num_double)/num_single];
		    if (!next) { 
			if (put) {
			    if (alloc_block(fs,&next)) {  
				ERROR("Failed to allocate new 3-indirect block (3rd)\n");
				return -1;
			    }
			    memset(temp,0,sizeof(temp));
			    if (write_block(fs,next,temp)) { 
				ERROR("Failed to write new 3-indirect block (3rd)\n");
				return -1;
			    }
			    ptrs[(left%num_double)/num_single] = next;
			    if (write_block(fs,cur,buf)) { 
				ERROR("Failed to update for new 3-indirect block (3rd)\n");
				return -1;
			    }
			} else {
			    ERROR("required 3-indirect block (3rd step) does not exist\n");
			    return -1;
			}
		    }
		    if (read_block(fs,next,buf)) { 
			ERROR("Cannot read triple indirect block (3rd)\n");
			return -1;
		    }
		    if (put) { 
			ptrs[left%num_single] = *physical_block;
			if (write_block(fs,next,buf)) {
			    ERROR("Failed to write 3-indirect mapping\n");
			    return -1;
			}
			return 0;
		    } else {
			*physical_block = ptrs[left%num_single];
			return 0;
		    }
		} else {
		    // >3 indirect - impossible
		    ERROR("ext2 only goes up to 3-indirect.... WTF\n");
		    return -1;
		}
	    }
	}
    }
}


#define map_logical_to_physical_get(fs,inode_num,inode,logical_block,physical_block) \
    map_logical_to_physical_get_put(fs,inode_num,inode,logical_block,physical_block,0)
#define map_logical_to_physical_put(fs,inode_num,inode,logical_block,physical_block) \
    map_logical_to_physical_get_put(fs,inode_num,inode,logical_block,&physical_block,1)


static int ext2_truncate(void *state, void *file, off_t len)
{ 
    struct ext2_state *fs = (struct ext2_state *)state;
    uint64_t block_size = get_block_size(fs);
    uint32_t inode_num = (uint32_t)(uint64_t)file;
    uint32_t phys;

    struct ext2_inode inode;   

    size_t file_size_bytes, file_size_blocks;
    size_t new_file_size_bytes, new_file_size_blocks;
    
    DEBUG("truncating inode %u to %lu bytes\n", inode_num, len);
     
    if (read_inode(fs,inode_num,&inode)) { 
	ERROR("Failed to read inode %u\n",inode_num);
	return -1;
    }

    file_size_bytes = get_file_size(fs,&inode);
    file_size_blocks = CEIL_DIV(file_size_bytes,block_size);
    new_file_size_bytes = len;
    new_file_size_blocks = CEIL_DIV(new_file_size_bytes,block_size);

    if (new_file_size_blocks < file_size_blocks) { 
	// shrink
	uint64_t block;
	for (block=new_file_size_blocks;block<file_size_blocks;block++) { 
	    if (map_logical_to_physical_get(fs,inode_num,&inode,block,&phys)) { 
		ERROR("Unable to map logical block %lu to physical block in truncation\n");
		return -1;
	    } 
	    if (free_block(fs,phys)) { 
		ERROR("Unable to free block in truncation\n");
		return -1;
	    }
	}
    } else if (new_file_size_blocks > file_size_blocks) {
	// grow
	uint64_t block;
	uint8_t buf[block_size];
	memset(buf,0,sizeof(buf));
	for (block=file_size_blocks;block<new_file_size_blocks;block++) { 
	    if (alloc_block(fs,&phys)) { 
		ERROR("Unable to allocate block in truncation\n");
		// should unwind previous allocations here...
		return -1;
	    }
	    if (map_logical_to_physical_put(fs,inode_num,&inode,block,phys)) { 
		ERROR("Unable to create mapping of logical block %lu to physical block %lu in truncation\n", block, phys);
		// should unwind here
		return -1;
	    } 
	    // zero block
	    if (write_block(fs,phys,buf)) { 
		ERROR("Unable to zero block on expansion\n");
		return -1;
	    }
	}
    } else {
	// same size in terms of blocks
    }

    set_file_size(fs, &inode, new_file_size_bytes);
    inode.i_blocks = new_file_size_blocks;

    if (write_inode(fs,inode_num,&inode)) { 
	ERROR("Failed to update inode with new sizes\n");
	return -1;
    }
    
    return 0;

}
 
static ssize_t ext2_read_write(void *state, void *file, void *srcdest, off_t offset, size_t num_bytes, int write)
{
    struct ext2_state *fs = (struct ext2_state *)state;
    uint64_t block_size = get_block_size(fs);
    uint32_t inode_num = (uint32_t)(uint64_t)file;
    struct ext2_inode inode;   
    size_t file_size_bytes, file_size_blocks;

    DEBUG("%sing inode %u %lu bytes at offset %lu\n",rw[write], inode_num, num_bytes, offset);
  
    if (read_inode(fs,inode_num,&inode)) { 
	ERROR("Failed to read inode %u\n",inode_num);
	return -1;
    }

    file_size_bytes = get_file_size(fs,&inode);
    file_size_blocks = CEIL_DIV(file_size_bytes,block_size);

    //num_bytes = MIN(block_size*NUM_DATA_BLOCKS)-offset, num_bytes);

    if (offset>=file_size_bytes) {
	if (!write) {
	    // handle read past end of file	
	    DEBUG("Reading starts past end of file\n");
	    return 0;
	} else {
	    // expand and zero fill if needed
	    DEBUG("Writing starts past end of file - expanding and retrying\n");
	    if (ext2_truncate(fs,file,offset+num_bytes-1)) { 
		ERROR("file expansion failed\n");
		return -1;
	    } else {
		return ext2_read_write(state, file, srcdest, offset, num_bytes, write);
	    }
	}
    }

    if (write) { 
	if (offset+num_bytes > file_size_bytes) { 
	    DEBUG("Writing continues past end of file - expanding and retrying\n");
	    if (ext2_truncate(fs,file,offset+num_bytes-1)) { 
		ERROR("file expansion failed\n");
		return -1;
	    } else {
		return ext2_read_write(state, file, srcdest, offset, num_bytes, write);
	    }
	}
    } else {
	num_bytes = MIN(file_size_bytes-offset,num_bytes);
    }

    DEBUG("Updated request: %sing inode %u %lu bytes at offset %lu\n",rw[write], inode_num, num_bytes, offset);
    
    uint64_t offset_into_first_block = offset % block_size;
    uint64_t bytes_from_first_block = MIN(num_bytes,block_size - offset_into_first_block);
    uint64_t bytes_from_middle_blocks = block_size*FLOOR_DIV((num_bytes - bytes_from_first_block),block_size);
    uint64_t bytes_from_last_block = num_bytes-bytes_from_first_block-bytes_from_middle_blocks;
    uint64_t have_first_block = bytes_from_first_block!=0;
    uint64_t num_middle_blocks = bytes_from_middle_blocks/block_size;
    uint64_t have_last_block = bytes_from_last_block!=0;
    uint64_t num_blocks = have_first_block + num_middle_blocks + have_last_block;

    uint64_t logical_block_start = FLOOR_DIV(offset,block_size);
	
    uint8_t buf[block_size];
    uint32_t cur_logical_block;
    uint32_t cur_physical_block;

    DEBUG("logical blocks [%lu,%lu), first_offset=%lu first=%lu middle=%lu, last=%lu\n",
	  logical_block_start, logical_block_start+num_blocks,
	  offset_into_first_block, bytes_from_last_block);

    uint64_t bytes=0;

    for (cur_logical_block = logical_block_start;
	 cur_logical_block < logical_block_start + num_blocks;
	 cur_logical_block++) {
	
	if (map_logical_to_physical_get(fs,inode_num,&inode,cur_logical_block,&cur_physical_block)) { 
	    ERROR("Unable to map logical block %lu\n", cur_logical_block);
	    return -1;
	}
	
	DEBUG("mapped logical block %lu to physical block %lu\n", cur_logical_block, cur_physical_block);
	
	if (have_first_block && cur_logical_block==logical_block_start) {
	    // first block (partial)
	    if (read_block(fs,cur_physical_block,buf)) {
		ERROR("Failed to read first partial physical block %lu\n",cur_physical_block);
		return -1;
	    } 
	    if (!write) { 
		// read - copy-out
		memcpy(srcdest+bytes,buf+offset_into_first_block,bytes_from_first_block);
	    } else {
		// write - copy-in and flush
		memcpy(buf+offset_into_first_block,srcdest+bytes,bytes_from_first_block);
		if (write_block(fs,cur_physical_block,buf)) { 
		    ERROR("Failed to write first partial physical block %lu\n",cur_physical_block);
		    return -1;
		}
	    }
	    bytes += bytes_from_first_block;
	    continue;
	}

	if (have_last_block && cur_logical_block==(logical_block_start+num_blocks-1)) {
	    // last block (partial)
	    if (read_block(fs,cur_physical_block,buf)) {
		ERROR("Failed to read last partial physical block %lu\n",cur_physical_block);
		return -1;
	    } 
	    if (!write) { 
		// read - copy-out
		memcpy(srcdest+bytes,buf,bytes_from_last_block);
	    } else {
		// write - copy-in and flush
		memcpy(buf,srcdest+bytes,bytes_from_last_block);
		if (write_block(fs,cur_physical_block,buf)) { 
		    ERROR("Failed to write first partial physical block %lu\n",cur_physical_block);
		    return -1;
		}
	    }
	    bytes += bytes_from_last_block;
	    continue;
	}
	
	// common case - r/w complete blocks
	if (!write) {
	    if (read_block(fs,cur_physical_block,srcdest+bytes)) { 
		ERROR("Failed to read middle block %lu\n",cur_physical_block);
		return -1;
	    } else {
		bytes += block_size;
		continue;
	    }
	} else {
	    if (write_block(fs,cur_physical_block,srcdest+bytes)) { 
		ERROR("Failed to write middle block %lu\n",cur_physical_block);
		return -1;
	    } else {
		bytes += block_size;
		continue;
	    }
	}
    }

    if (bytes != num_bytes) { 
	ERROR("Strange... request for %lu bytes, but access loop handled %lu bytes\n",
	      num_bytes, bytes);
    }

    DEBUG("%s request done\n", rw[write]);

    return bytes;
}


static ssize_t ext2_read(void *state, void *file, void *srcdest, off_t offset, size_t num_bytes)
{
    return ext2_read_write(state,file,srcdest,offset,num_bytes,0);
}

static ssize_t ext2_write(void *state, void *file, void *srcdest, off_t offset, size_t num_bytes)
{
    return ext2_read_write(state,file,srcdest,offset,num_bytes,1);
}


/*
static uint16_t dentry_find_len(struct ext2_dir_entry_2 *dentry) 
{
    uint16_t len = (uint16_t)(sizeof(dentry->inode) + sizeof(dentry->name_len) + sizeof(dentry->file_type) + dentry->name_len + sizeof(uint16_t));
    return (len += (DENTRY_ALIGN - len % DENTRY_ALIGN));
}
*/


static int dentry_get_put_del(struct ext2_state      *fs,
			      uint32_t                inode_num,
			      struct ext2_inode       *their_inode,
			      struct ext2_dir_entry_2 *dentry,
			      enum dentry_op           op)
{
    struct ext2_inode our_inode;
    struct ext2_inode *inode;
    uint64_t block_size = get_block_size(fs);
    uint32_t num_sects;
    uint32_t num_blocks;
    uint32_t logical_block;
    uint32_t physical_block;
    uint8_t buf[block_size];
    struct ext2_dir_entry_2 *de, *pde;
    uint32_t dl;

    DEBUG("dentry_get_put_del(%s, fs=%s, inode=%u, name=%s)\n",
	  op==GET ? "GET" : op==PUT ? "PUT" : "DEL",
	  fs->fs->name, inode_num, dentry->name);

    if (!their_inode) {
	if (read_inode(fs,inode_num,&our_inode)) { 
	    ERROR("Failed to read inode\n");
	    return -1;
	}
	inode = &our_inode;
    } else {
	inode = their_inode;
    }
    
    // i_blocks counts in sectors, not fs blocks....
    num_sects = inode->i_blocks;
    num_blocks = CEIL_DIV(num_sects,block_size);

    DEBUG("Directory has %u sectors / %u blocks\n",num_sects, num_blocks);

    for (logical_block=0;logical_block<num_blocks;logical_block++) {
	DEBUG("Scanning directory logical block %lu\n",logical_block);
	if (map_logical_to_physical_get(fs,inode_num,inode,logical_block,&physical_block)) { 
	    ERROR("Unable to map directory block %u\n",logical_block);
	    return -1;
	}
	DEBUG("Scanning directory physical block %lu\n",physical_block);
	if (read_block(fs,physical_block,buf)) {
	    ERROR("Unable to read directory block %u (%u)\n",logical_block,physical_block);
	    return -1;
	}
	// now scan block for name or for space
	uint32_t left=block_size;
	uint32_t offset;

	for (offset=0, pde=0; offset<block_size; offset+=de->rec_len, pde=de) { 
	    de = (struct ext2_dir_entry_2 *)&buf[offset];
	    if (!de->inode) { 
		//blank entry - perhaps its big enough for us
		if (op==PUT) { 
		    if (dentry->rec_len<=de->rec_len) {
			uint16_t old_rec_len = de->rec_len;
			// will fit here
			*de = *dentry;
			if ((old_rec_len - de->rec_len) >= 12) {
			    // split
			    struct ext2_dir_entry_2 *nde = (struct ext2_dir_entry_2 *)&buf[offset+de->rec_len];
			    nde->inode = 0;
			    nde->rec_len = old_rec_len - de->rec_len;
			} else {
			    de->rec_len = old_rec_len;
			}
			if (write_block(fs,physical_block,buf)) {
			    ERROR("Cannot write updated directory block\n");
			    return -1;
			} else {
			    return 0;
			}
		    }
		} 
	    } else {
		if (op==GET || op==DEL_BY_NAME || op==DEL_BY_INODE) { 
		    if (((op==GET || op==DEL_BY_NAME) 
			 && de->name_len==dentry->name_len && !strncmp(de->name,dentry->name,de->name_len))
			|| ((op==DEL_BY_INODE) && de->inode==dentry->inode)) {

			// found it
			if (op==GET) { 
			    *dentry = *de;
			    return 0;
			} else {
			    // DEL by either
			    de->inode=0;
			    if ((offset+de->rec_len)<block_size) { 
				// possible to merge with next
				struct ext2_dir_entry_2 *nde = (struct ext2_dir_entry_2 *)&buf[offset+de->rec_len];
				if (!nde->inode) { 
				    // merge next
				    de->rec_len+=nde->rec_len;
				}
			    }
			    if (pde && !pde->inode) { 
				// merge prev
				pde->rec_len+=de->rec_len;
			    }
			    if (write_block(fs,physical_block,buf)) { 
				ERROR("Cannot write updated directory block after del\n");
				return -1;
			    } else {
				// we should free the block here and update the inode 
				// if there are now no non-empty entries on it.
				return 0;
			    }
			}
		    }
		}
	    }
	}
    }
	
    // reached end of line...
    if (op!=PUT) { 
	return -1;
    }
    
    // We are now in an add, so we need allocate new block and put
    // the record into it, followed by an empty record
    //
    if (alloc_block(fs,&physical_block)) { 
	ERROR("Unable to allocate block in put\n");
	return -1;
    }
    
    memset(buf,0,sizeof(buf));
    de = (struct ext2_dir_entry_2 *)&buf[0];
    *de = *dentry;
    de = (struct ext2_dir_entry_2 *)&buf[dentry->rec_len];
    de->rec_len = block_size-dentry->rec_len;
    
    if (write_block(fs,physical_block,buf)) { 
	ERROR("Unable to write new block in put\n");
	return -1;
    }
    
    if (map_logical_to_physical_put(fs,inode_num,inode,logical_block,physical_block)) { 
	ERROR("Unable to map new directory block %u\n",logical_block);
	return -1;
    }
    
    return 0;
}
    
#define dentry_get(fs,inode_num,inode,dentry) dentry_get_put_del(fs,inode_num,inode,dentry,GET)
#define dentry_put(fs,inode_num,inode,dentry) dentry_get_put_del(fs,inode_num,inode,dentry,PUT)
#define dentry_del_by_name(fs,inode_num,inode,dentry) dentry_get_put_del(fs,inode_num,inode,dentry,DEL_BY_NAME)
#define dentry_del_by_inode(fs,inode_num,inode,dentry) dentry_get_put_del(fs,inode_num,inode,dentry,DEL_BY_INODE)

	
	       

static int dentry_add(struct ext2_state *fs, 
		      uint32_t dir_inum, 
		      uint32_t target_inum, 
		      char *name, 
		      uint8_t file_type) 
{

    struct ext2_dir_entry_2 de;

    de.inode = target_inum;
    strcpy(de.name,name);
    de.name_len=strlen(name);
    de.file_type = file_type;
    de.rec_len = EXT2_DIR_REC_LEN(de.name_len);
    
    return dentry_put(fs,dir_inum,0,&de);
	    
    /*    

    ssize_t blocksize = get_block_size(fs);

    struct ext2_inode dir;

    if (read_inode(fs,dir_inum,&dir)) { 
	ERROR("Failed to read directory inode %u\n",dir_inum);
	return -1;
    }

    //create directory entry
    struct ext2_dir_entry_2 new_entry;

    new_entry.inode = target_inum;
    new_entry.name_len = (uint8_t) strlen(name);
    new_entry.file_type = file_type;
    strcpy(new_entry.name, name);
    new_entry.rec_len = dentry_find_len(&new_entry);
    dir.i_size += new_entry.rec_len;

    DEBUG("Dir Add: %s, reclen %d, name %s, inode %u", name, new_entry.rec_len, name, target_inum);

    uint8_t buf[blocksize];

    if (read_block(fs,dir.i_block[0],buf)) { 
	ERROR("Failed to read directory block\n");
	return -1;
    }

    // iterate to last dentry looking for an open entry
    struct ext2_dir_entry_2 *dentry = (struct ext2_dir_entry_2 *)buf;

    blocksize -= dentry-> rec_len;
    while (blocksize > 0) { 
	dentry = (struct ext2_dir_entry_2*)((uint8_t*)dentry + dentry->rec_len);
	blocksize -= dentry->rec_len;
    }
    
    // adjust rec_lens
    uint16_t last_len = dentry_find_len(dentry);
    new_entry.rec_len = dentry->rec_len - last_len; 
    dentry->rec_len = last_len;

    // place entry
    // TODO: check new entry fits
    void *dst = (void*)((uint8_t*)dentry + last_len);
    DEBUG("DIR ADD: Block at %p, place at %p", buf, dst);
    *(struct ext2_dir_entry_2 *)dst = new_entry;

    if (write_block(fs,dir.i_block[0],buf)) { 
	ERROR("Failed to write directory block\n");
	return -1;
    }

    if (write_inode(fs, dir_inum, &dir)) {
	ERROR("Cannot write directory inode\n");
	return -1;
    }

    return 0;

    */
}

static int dentry_remove(struct ext2_state *fs, int dir_inum, int target_inum) 
{
    struct ext2_dir_entry_2 de;

    de.inode = target_inum;
    strcpy(de.name,"");
    de.name_len=strlen("");
    de.file_type = 0;
    de.rec_len = EXT2_DIR_REC_LEN(de.name_len);
    
    return dentry_del_by_inode(fs,dir_inum,0,&de);

    /*
    ssize_t blocksize = get_block_size(fs);
    uint8_t buf[blocksize];
    struct ext2_inode dir;

    if (read_inode(fs, dir_inum, &dir)) {
	ERROR("Cannot read directory inode\n");
	return -1;
    }

    if (read_block(fs,dir.i_block[0],buf)) { 
	ERROR("Cannot read directory block\n");
	return -1;
    }

    // find target dentry
    struct ext2_dir_entry_2 *dentry = (struct ext2_dir_entry_2 *)buf;
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
	ERROR("DIR REMOVE FILE: rec_len values not aligned %d", blocksize);
    }
    
    int is_target_end = 0;
    if (target == dentry) {
	is_target_end = 1;
	DEBUG("DIR REMOVE: End target");
    }

    DEBUG("Dir Remove: size %d", blocksize);
    if (target == NULL) {
	ERROR("Dir Remove: Did not find dentry!");
	return -1;
    }
    
    size_t dir_size = (uint64_t)dentry - (uint64_t)buf + dentry_find_len(dentry); 

    //DEBUG("Dir Remove: last reclen %d", ext2_dentry_find_len(dentry)); 
    //DEBUG("Dir Remove: dir_size %d, %x", dir_size, dir_size); 

    if (is_target_end) {
	prev->rec_len += target->rec_len;
    } else {
	// move remaining dentries back
	dentry->rec_len += target->rec_len;
	void *move_dst = (void*)((uint8_t*)target);
	void *move_src = (void*)((uint8_t*)target + target->rec_len);
	size_t move_len = (uint64_t)buf + dir_size - (uint64_t)move_src;
	//DEBUG("Dir Remove: move %p %p", move_dst, move_src);
	//DEBUG("Dir Remove: move len %d", move_len);
	memmove(move_dst, move_src, move_len);
    }
    
    // update terminator
    dir_size -= target->rec_len;
    dir.i_size -= dir_size;

    if (write_block(fs,dir.i_block[0],buf)) { 
	ERROR("Cannot write directory block\n");
	return -1;
    }

    if (write_inode(fs, dir_inum, &dir)) {
	ERROR("Cannot write directory inode\n");
	return -1;
    }
    
    return 0;
    */
}

/*
static int dentry_get_put(struct ext2_state *fs, int dir_inum, int target_inum) 
{
    ssize_t blocksize = get_block_size(fs);
    uint8_t buf[blocksize];
    struct ext2_inode dir;

    if (read_inode(fs, dir_inum, &dir)) {
	ERROR("Cannot read directory inode\n");
	return -1;
    }

    if (read_block(fs,dir.i_block[0],buf)) { 
	ERROR("Cannot read directory block\n");
	return -1;
    }

    // find target dentry
    struct ext2_dir_entry_2 *dentry = (struct ext2_dir_entry_2 *)buf;
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
	ERROR("DIR REMOVE FILE: rec_len values not aligned %d", blocksize);
    }
    
    int is_target_end = 0;
    if (target == dentry) {
	is_target_end = 1;
	DEBUG("DIR REMOVE: End target");
    }

    DEBUG("Dir Remove: size %d", blocksize);
    if (target == NULL) {
	ERROR("Dir Remove: Did not find dentry!");
	return -1;
    }
    
    size_t dir_size = (uint64_t)dentry - (uint64_t)buf + dentry_find_len(dentry); 

    //DEBUG("Dir Remove: last reclen %d", ext2_dentry_find_len(dentry)); 
    //DEBUG("Dir Remove: dir_size %d, %x", dir_size, dir_size); 

    if (is_target_end) {
	prev->rec_len += target->rec_len;
    } else {
	// move remaining dentries back
	dentry->rec_len += target->rec_len;
	void *move_dst = (void*)((uint8_t*)target);
	void *move_src = (void*)((uint8_t*)target + target->rec_len);
	size_t move_len = (uint64_t)buf + dir_size - (uint64_t)move_src;
	//DEBUG("Dir Remove: move %p %p", move_dst, move_src);
	//DEBUG("Dir Remove: move len %d", move_len);
	memmove(move_dst, move_src, move_len);
    }
    
    // update terminator
    dir_size -= target->rec_len;
    dir.i_size -= dir_size;

    if (write_block(fs,dir.i_block[0],buf)) { 
	ERROR("Cannot write directory block\n");
	return -1;
    }

    if (write_inode(fs, dir_inum, &dir)) {
	ERROR("Cannot write directory inode\n");
	return -1;
    }
    
    return 0;
}
*/



// return new inode number; 0 if failed
static void *ext2_create(void *state, char *path, int dir)
{
    char *fd[2] = {"file","dir"};
    struct ext2_state *fs = (struct ext2_state *) state;
    uint32_t inode_num;
    struct ext2_inode inode;

    dir &= 0x1;

    DEBUG("create %s %s on fs %s\n", fd[dir], path,fs->fs->name);

    // HERE HERE

    // get directory path
    int num_parts = 0;
    char **parts = split_path(path, &num_parts);
    char *name;

    if (!num_parts) {
	ERROR("Impossible path %s\n",path);
	free_split_path(parts,num_parts);
	return 0;
    }

    name = parts[num_parts-1];

    char dirname[strlen(path)+1];

    strcpy(dirname, path);

    for (int i=strlen(dirname); i>=0; i--) {
	if (dirname[i]=='/') {
	    dirname[i] = 0;
	}
    }

    int dir_num = 0;
    
    if (dirname[0]==0) {
	dir_num = EXT2_ROOT_INO;
    } else {
	dir_num = get_inode_num_by_path(fs, dirname);
    }

    if (!dir_num) { 
	ERROR("Parent directory not found (dirname=%s)\n",dirname);
	free_split_path(parts,num_parts);
	return 0;
    }

    if (alloc_inode(fs,&inode_num)) {
	ERROR("Unable to allocate inode\n");
	free_split_path(parts,num_parts);
	return 0;
    }

    struct ext2_inode newinode;

    memset(&newinode,0,sizeof(newinode));

    if (dentry_add(fs, dir_num, inode_num, name, dir ? EXT2_FT_DIR : EXT2_FT_REG_FILE)) { 
	ERROR("Failed to add directory entry\n");
	free_inode(fs,inode_num);
	free_split_path(parts,num_parts);
	return 0;
    }

    //fill in inode with stuff
    newinode.i_mode = dir ? EXT2_S_IFDIR : EXT2_S_IFREG;
    newinode.i_size = 0;
    //set bitmap to taken  // of what?

    if (write_inode(fs, inode_num, &newinode)) {
	ERROR("Cannot write new inode\n");
	free_split_path(parts,num_parts);
	return 0;
    }
    
    free_split_path(parts,num_parts);

    return (void*)(uint64_t)inode_num;

}

static void *ext2_create_file(void *state, char *path)
{
    return ext2_create(state,path,0);
}

static int ext2_create_dir(void *state, char *path)
{
    void *f = ext2_create(state,path,1);

    if (!f) { 
	return -1;
    } else {
	return 0;
    }
}


static int inode_has_mode(struct ext2_inode *inode, int mode) 
{
    return ((inode->i_mode & mode) == mode);
}

int ext2_remove(void *state, char *path) 
{
    struct ext2_state *fs = (struct ext2_state *)state;
    char *name;

    uint32_t inum = get_inode_num_by_path(fs, path);

    if (!inum) {
	ERROR("Bad path for remove (%s)\n", path);
	return -1;
    }

    struct ext2_inode inode;

    if (read_inode(fs,inum,&inode)) { 
	ERROR("Failed to read inode\n");
	return -1;
    }

    // check if file
    if (!inode_has_mode(&inode, EXT2_S_IFREG)) {
	ERROR("Cannot currently remove non-files (mode %x)", inode.i_mode);
	return -1;
    }

    int num_parts = 0;

    char **parts = split_path(path, &num_parts);

    if (!num_parts) {
	ERROR("No parts");
	free_split_path(parts,num_parts);
	return -1;
    }

    name = parts[num_parts-1];

    char dirname[strlen(path)+1];

    strcpy(dirname, path);

    for (int i=strlen(dirname); i>=0; i--) {
	if (dirname[i]=='/') {
	    dirname[i] = 0;
	}
    }

    int dir_num = 0;
    
    if (dirname[0]==0) {
	dir_num = EXT2_ROOT_INO;
    } else {
	dir_num = get_inode_num_by_path(fs, dirname);
    }

    if (!dir_num) { 
	ERROR("Parent directory not found (dirname=%s)\n",dirname);
	free_split_path(parts,num_parts);
	return -1;
    }

    // remove dentry 
    if (dentry_remove(fs, dir_num, inum)) { 
	ERROR("Failed to remove directory entry\n");
	return -1;
    }

    // truncate file
    if (ext2_truncate(fs, (void*)(uint64_t)inum, 0)) {
	ERROR("Failed to truncate file during removal\n");
	return -1;
    }

    if (free_inode(fs, inum)) { 
	ERROR("Failed to free inode during removal\n");
	return -1;
    }

    return 0;
}

static int ext2_stat(void *state, void *file, struct nk_fs_stat *st)
{
    struct ext2_state *fs = (struct ext2_state *)state;
    uint32_t inum = (uint32_t)(uint64_t)file;
    struct ext2_inode inode;

    if (read_inode(fs,inum,&inode)) { 
	ERROR("Failed to read inode during stat\n");
	return -1;
    }
    
    st->st_size = inode.i_size;

    return 0;
}

static int ext2_stat_path(void *state, char *path, struct nk_fs_stat *st)
{
    struct ext2_state *fs = (struct ext2_state *)state;
    uint32_t inum = get_inode_num_by_path(fs,path);
    
    if (!inum) { 
	ERROR("Nonexistent path %s during stat\n",path);
	return -1;
    }

    return ext2_stat(state,(void*)(uint64_t)inum,st);
}


static struct nk_fs_int ext2_inter = {
    .stat_path = ext2_stat_path,
    .create_file = ext2_create_file,
    .create_dir = ext2_create_dir,
    .exists = ext2_exists,
    .remove = ext2_remove,
    .open_file = ext2_open,
    .stat = ext2_stat,
    .trunc_file = ext2_truncate,
    .close_file = ext2_close,
    .read_file = ext2_read,
    .write_file = ext2_write,
};


int nk_fs_ext2_attach(char *devname, char *fsname, int readonly)
{
    struct nk_block_dev *dev = nk_block_dev_find(devname);
    uint64_t flags = readonly ? NK_FS_READONLY : 0;

    if (!dev) { 
	ERROR("Cannot find device %s\n",devname);
	return -1;
    }
    
    struct ext2_state *s = malloc(sizeof(*s));
    if (!s) { 
	ERROR("Cannot allocate space for fs %s\n", fsname);
	return -1;
    }

    memset(s,0,sizeof(*s));

    s->dev = dev;
    
    if (nk_block_dev_get_characteristics(dev,&s->chars)) { 
	ERROR("Cannot get characteristics of device %s\n", devname);
	free(s);
	return -1;
    }

    DEBUG("Device %s has block size %lu and numblocks %lu\n",dev->dev.name, s->chars.block_size, s->chars.num_blocks);

    // stash away superblock for later use
    // any modifier is responsible for writing it as well
    if (read_superblock(s)) {
	ERROR("Cannot read superblock for fs %s on device %s\n", fsname, devname);
	free(s);
	return -1;
    }
    
    s->fs = nk_fs_register(fsname, flags, &ext2_inter, s);

    if (!s->fs) { 
	ERROR("Unable to register filesystem %s\n", fsname);
	free(s);
	return -1;
    }

    INFO("filesystem %s on device %s is attached (%s)\n", fsname, devname, readonly ?  "readonly" : "read/write");
    
    return 0;
}

int nk_fs_ext2_detach(char *fsname)
{
    struct nk_fs *fs = nk_fs_find(fsname);
    if (!fs) { 
	return -1;
    } else {
	return nk_fs_unregister(fs);
    }
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

