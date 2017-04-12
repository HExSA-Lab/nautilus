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

/* ext2_access.c
 *
 * Authors: Brady Lee and David Williams
 * Based on the ext2 specification found at http://www.nongnu.org/ext2-doc/ext2.html
 *
 * Contains the lowest level methods for interacting with an ext2
 * filesystem image.
 *
 * Built for adding ext2 functionality to the Nautilus OS, Northwestern University
 */

#define FLOOR_DIV(x,y) ((x)/(y))
#define CEIL_DIV(x,y)  (((x)/(y)) + !!((x)%(y)))
#define DIVIDES(x,y) (((x)%(y))==0)
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define MIN(x,y) ((x)<(y) ? (x) : (y))


static char *rw[2] = { "read", "write" };


static int read_write_superblock(struct ext2_state *fs, int write)
{
    write &= 0x1;

    if (sizeof(fs->super)!=SUPERBLOCK_SIZE) { 
	ERROR("Cannot %s superblock as the code is not internally consistent\n",rw[write]);
	return -1;
    }
    
    if (!DIVIDES(SUPERBLOCK_SIZE,fs->chars.block_size) || 
	SUPERBLOCK_SIZE<fs->chars.block_size) { 
	ERROR("Cannot %s superblock as it is too small or not a multiple of device blocks\n",rw[write]);
	return -1;
    }

    if (!DIVIDES(SUPERBLOCK_OFFSET,fs->chars.block_size)) {
	ERROR("Cannot %s superblock as its offset is not a multiple of device block size\n",rw[write]);
	return -1;
    }
    
    // superblock is a positive integer multiple of device block size

    uint64_t dev_offset = FLOOR_DIV(SUPERBLOCK_OFFSET,fs->chars.block_size);
    uint64_t dev_num    = FLOOR_DIV(SUPERBLOCK_SIZE,fs->chars.block_size);
    int rc;

    DEBUG("%sing superblock (offset=%u, size=%u) on fs %s, bs=%u, dev_off=%lu, dev_num=%lu\n",
	  rw[write], SUPERBLOCK_OFFSET, SUPERBLOCK_SIZE, fs->fs->name, fs->chars.block_size, dev_offset, dev_num);

    if (write) { 
	rc = nk_block_dev_write(fs->dev,dev_offset,dev_num,&fs->super,NK_DEV_REQ_BLOCKING,0,0); 
	// TODO: write shadow copies
    } else {
	rc = nk_block_dev_read(fs->dev,dev_offset,dev_num,&fs->super,NK_DEV_REQ_BLOCKING,0,0);
    }
    
    if (rc) { 
	ERROR("Failed to %s superblock due to device error\n",rw[write]);
	return -1;
    }

    return 0;
}

#define read_superblock(fs)  read_write_superblock(fs,0)
#define write_superblock(fs) read_write_superblock(fs,1)



static uint32_t get_block_size(struct ext2_state *fs) 
{
    uint32_t shift;

    //s_log_block_size tells how much to shift 1K by to determine block size
    shift = fs->super.s_log_block_size;
    return (1024 << shift);
}

static int read_write_block(struct ext2_state * fs, uint32_t block_num, void *srcdest, int write) 
{
    uint32_t block_size = get_block_size(fs);
    uint64_t dev_offset = FLOOR_DIV(block_num*block_size,fs->chars.block_size);
    uint64_t dev_num    = FLOOR_DIV(block_size,fs->chars.block_size);
    int rc;

    write &= 0x1;

    DEBUG("%sing block %u on fs %s / dev %s, bs=%u, dev_off=%lu, dev_num=%lu\n",
	  rw[write], block_num, fs->fs->name, fs->dev->dev.name, block_size, dev_offset, dev_num);

    if (write) { 
	rc = nk_block_dev_write(fs->dev,dev_offset,dev_num,srcdest,NK_DEV_REQ_BLOCKING,0,0); 
    } else {
	rc = nk_block_dev_read(fs->dev,dev_offset,dev_num,srcdest,NK_DEV_REQ_BLOCKING,0,0);
    }
    
    if (rc) { 
	ERROR("Failed to %s block %lu due to device error\n",rw[write],block_num);
	return -1;
    }

    return 0;

}

#define read_block(fs,block_num,dest)  read_write_block(fs,block_num,dest,0)
#define write_block(fs,block_num,src)  read_write_block(fs,block_num,src,1)


#define blocks_per_group(sb) ((sb)->s_blocks_per_group)
#define inodes_per_group(sb) ((sb)->s_inodes_per_group)
#define num_block_groups(sb) ((sb)->s_blocks_count/(sb)->s_blocks_per_group)
/*
 * reads/writes the requested block_group descriptor
 * only the first block group descriptor table is read/written
 */
static int read_write_block_group(struct ext2_state* fs, uint32_t block_group_num, struct ext2_group_desc *srcdest, int write) 
{
    uint64_t block_num;
    uint64_t offset;
    uint32_t block_size;
    uint64_t desc_per_block;

    write &= 0x1;

    DEBUG("%s block group fs %s bgn=%u\n", rw[write], fs->fs->name, block_group_num);

    block_size = get_block_size(fs);
    block_num = FLOOR_DIV(SUPERBLOCK_OFFSET+SUPERBLOCK_SIZE,block_size);
    desc_per_block = block_size/sizeof(struct ext2_group_desc);
    block_num += block_group_num/desc_per_block;
    offset = block_group_num%desc_per_block;

    DEBUG("%sing block group descriptor %u (block_num=%u) on fs %s\n",
	  rw[write], block_group_num, block_num, fs->fs->name);
    
    uint8_t buf[block_size];
    struct ext2_group_desc *d = (struct ext2_group_desc *)buf;

    if (read_block(fs,block_num,buf)) { 
	ERROR("Cannot read block group\n");
	return -1;
    } 

    if (write) { 
	d[offset] = *srcdest;
	if (write_block(fs,block_num,buf)) { 
	    ERROR("Cannot write block group\n");
	    return -1;
	} else {
	    return 0;
	}
	// TODO: update shadow copies
    } else {
	*srcdest = d[offset];
	return 0;
    }
}

#define read_block_group(fs,block_group_num,dest)  read_write_block_group(fs,block_group_num,dest,0)
#define write_block_group(fs,block_group_num,src)  read_write_block_group(fs,block_group_num,src,1)

static int read_write_inode(struct ext2_state *fs, uint32_t inode_num, struct ext2_inode *srcdest, int write) 
{
    struct ext2_group_desc bg;
    uint32_t inode_block;
    uint64_t inode_offset;
    uint32_t block_size = get_block_size(fs);
    uint64_t inodes_per_block = FLOOR_DIV(block_size,sizeof(struct ext2_inode));
    uint8_t buf[block_size];
    struct ext2_inode* inode_table = (struct ext2_inode *)buf;

    write &= 0x1;

    if (read_block_group(fs,inode_num/inodes_per_group(&fs->super),&bg)) { 
	ERROR("Cannot read block group\n");
	return -1;
    }

    DEBUG("block group:  bbitmap=%u ibitmap=%u, itable=%u\n", 
	  bg.bg_block_bitmap, bg.bg_inode_bitmap, bg.bg_inode_table);

    //get index into inode table
    inode_block  = bg.bg_inode_table + FLOOR_DIV(inode_num - 1, inodes_per_block);
    inode_offset = (inode_num - 1) % inodes_per_block;

    DEBUG("%sing inode %u (block %u, offset %u) inode_size=%u  on fs %s\n", 
	  rw[write], inode_num, inode_block, inode_offset, sizeof(struct ext2_inode), fs->fs->name);

    //gets pointer to block where inodes are located 
    if (read_block(fs,inode_block,buf)) { 
	ERROR("Cannot read inode block\n");
	return -1;
    }

    if (write) { 
	inode_table[inode_offset] = *srcdest;
	if (write_block(fs,inode_block,buf)) { 
	    ERROR("Cannot write inode block\n");
	    return -1;
	} else {
	    return 0;
	}
    } else {
	*srcdest = inode_table[inode_offset];
	return 0;
    }
}

#define read_inode(fs,inode_num,dest)  read_write_inode(fs,inode_num,dest,0)
#define write_inode(fs,inode_num,src)  read_write_inode(fs,inode_num,src,1)

/* split_path
 *
 * returns an array of each part of a filepath
 * i.e. split_path("/a/b/c") = {"a", "b", "c"}
 * also puts the number of parts into the specified location for convenience
 */
static char** split_path(char *path, int *num_parts) 
{
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

static void free_split_path(char **list, int n)
{
    int i;
    for (i=0;i<n;i++) { 
	free(list[i]);
    }
    free(list);
}

/* get_root_dir_inode
 *
 * Convenience function to get pointer to inode of the root directory.
 */
static int get_root_dir_inode(void * fs, struct ext2_inode *inode) 
{
    return read_inode(fs, EXT2_ROOT_INO, inode);
}



enum dentry_op {GET,PUT,DEL_BY_NAME,DEL_BY_INODE};

static int dentry_get_put_del(struct ext2_state      *fs,
			      uint32_t                inode_num,
			      struct ext2_inode       *their_inode,
			      struct ext2_dir_entry_2 *dentry,
			      enum dentry_op           op);

/* get_inode_num_from_dir
 *
 * searches a directory for a file by name
 * returns the inode number of the file if found, or 0
 */
static uint32_t get_inode_num_from_dir(struct ext2_state *fs, 
				       uint32_t           inode_num,
				       struct ext2_inode *dir, 
				       char *name) 
{
    struct ext2_dir_entry_2 dentry;
    uint32_t inum;

    DEBUG("get_inode_num_from_dir on %s, inode_num=%u, dir=%p, search=%s\n",
	  fs->fs->name,inode_num,dir,name);

    strcpy(dentry.name,name);
    dentry.name_len=strlen(name);
    dentry.rec_len=EXT2_DIR_REC_LEN(dentry.name_len);
    
    if (dentry_get_put_del(fs,inode_num,dir,&dentry,GET)) { 
	ERROR("Failed to get directory entry for %s\n",name);
	return 0;
    } else {
	return dentry.inode;
    }

    /*
    uint32_t block_size = get_block_size(fs);
    uint8_t buf[block_size];

    DEBUG("get_inode_num_from_dir(%s, %p, %s)\n", fs->fs->name, dir, name);

    DEBUG("directory inode has i_block[0]=%lu\n",dir->i_block[0]);

    if (!dir->i_block[0]) {
	return 0;
    }

    //get pointer to block where directory entries reside using inode of directory
    if (read_block(fs,dir->i_block[0],buf)) { 
	ERROR("Cannot read block for directory inode\n");
	return 0;
    }

    DEBUG("Block read\n");

    struct ext2_dir_entry_2* dentry = (struct ext2_dir_entry_2*)buf;

    //scan only valid files in dentry
    //while(dentry->inode){
    //ssize_t left = block_size - dentry->rec_len;  // huh?  PAD
    ssize_t left = block_size;  // huh?  PAD
    int count = 0;
    while (left > 0 && dentry->rec_len!=0 ) {
	//while(++count < 20) {
	//DEBUG("malloc %d", dentry->name_len);
	char tempname[dentry->name_len+1];

	DEBUG("Entry has length %lu\n",dentry->name_len);

	//copy file name to temp name, then null terminate it
	memcpy(tempname,dentry->name,dentry->name_len);
	tempname[dentry->name_len] = 0;

	//if there is a match, return the inode number
	//DEBUG("INODE DIR: name %s, tempname %s", name, tempname);
	//DEBUG("INODE DIR: blocksize %d, reclen %d", blocksize, dentry->rec_len);
	
	if (!strcmp(name,tempname)) {
	    return dentry->inode;
	}

	dentry = (struct ext2_dir_entry_2*)((void*)dentry+dentry->rec_len);
	left -= dentry->rec_len;
    }
    return 0;
    */
}

/* get_inode_by_path
 *
 * given a path to a file, tries to find the inode number of the file
 * returns inode number of file if found, or 0
 */
static uint32_t get_inode_num_by_path(struct ext2_state *fs, char *path) 
{
    int num_parts = 0;

    DEBUG("get_inode_num_by_path(%s,%s)\n",fs->fs->name, path);

    if (!strcmp(path, "/")) {
	DEBUG("root dir\n");
	return EXT2_ROOT_INO;
    }

    char **parts = split_path(path, &num_parts);
    char *cur_part = *parts;

    int i;
    struct ext2_inode cur_inode;
    uint32_t new_inode_num = 0;
    uint32_t cur_inode_num;

    if (get_root_dir_inode(fs,&cur_inode)) { 
	ERROR("Cannot get root directory inode\n");
	free_split_path(parts,num_parts);
	return 0;
    }
    cur_inode_num=EXT2_ROOT_INO;

    DEBUG("Root inode fetched has mode %u, uid %u, size %u, blocks %u\n",
	  cur_inode.i_mode, cur_inode.i_uid, cur_inode.i_size, cur_inode.i_blocks);

    DEBUG("%s has %d parts\n", path, num_parts);
    
    for(i = 1; i <= num_parts; i++) {   // HUH?  PAD (<=)
	//treat current inode as directory, and search for inode of next part
	DEBUG("Considering part %s\n",cur_part);
	new_inode_num = get_inode_num_from_dir(fs, cur_inode_num, &cur_inode, cur_part);
	if (!new_inode_num) {
	    ERROR("Finished search and did not find element %s\n",cur_part);
	    free_split_path(parts,num_parts);
	    return 0;
	}
	DEBUG("GET INODE found: %d, %s\n", new_inode_num, cur_part);
	if (read_inode(fs, new_inode_num, &cur_inode)) { 
	    ERROR("Failed to read inode...\n");
	    free_split_path(parts,num_parts);
	    return 0;
	}
	cur_inode_num = new_inode_num;
	cur_part = *(parts+i);
    }

    //final inode is the requested file. return its number
    DEBUG("Found inode: %u\n", new_inode_num);
    free_split_path(parts,num_parts);

    return new_inode_num;
}

static int alloc_free_inode(struct ext2_state *fs, uint32_t *num, int free)
{
    char *af[2] = { "alloc", "free" };

    uint32_t bg_start, bg_end, bgi;
    uint32_t block_size=get_block_size(fs);
    uint8_t buf[block_size];
    struct ext2_group_desc bg;
    
    free &= 0x1;

    if (free) { 
	bg_start = bg_end = (*num-1)/inodes_per_group(&fs->super);
    } else {
	bg_start = 0;
	bg_end = num_block_groups(&fs->super);
    }

    for (bgi=bg_start;bgi<bg_end;bgi++) {
	//get block group descriptor
	if (read_block_group(fs, bgi, &bg)) { 
	    ERROR("Failed to read block group %u in inode %s\n", bgi, af[free]);
	    return -1;
	}
	
	if (read_block(fs,bg.bg_inode_bitmap,buf)) { 
	    ERROR("Failed to read inode bitmap in inode %s\n", af[free]);
	    return -1;
	}
	
	int byte;
	int bit;
	uint8_t cur_byte;
    
	if (free) { 
	    uint32_t actual;
	    actual = *num-1;  // inode numbers start with 1.... 
	    actual %= inodes_per_group(&fs->super); // we want index in this group
	    byte = actual/8;
	    bit = actual%8;
	    buf[byte] &= ~(0x1<<bit);
	} else {
	    for (byte=0; byte<block_size; byte++) {
		cur_byte = buf[byte];
		for (bit=0; bit < 8; bit++, cur_byte>>=1) {
		    if (!(cur_byte & 0x01)) {
			buf[byte] |= (0x1 << bit);
			*num = byte*8 + bit + 1; //first inode number is 1, not 0
			*num += bgi*inodes_per_group(&fs->super); // we want inode index on whole volume
			goto out_good;
		    }
		}
	    }
	    // if we got here, we could not find anything...
	    *num=0;
	    return -1;
	}
    }
    
 out_good:

    if (write_block(fs,bg.bg_inode_bitmap,buf)) { 
	ERROR("Failed to write inode bitmap in inode %s\n", af[free]);
	return -1;
    }
    
    return 0;
}

#define alloc_inode(fs,num) alloc_free_inode(fs,num,0)
#define free_inode(fs,num) alloc_free_inode(fs,&(num),1)


static int alloc_free_block(struct ext2_state *fs, uint32_t *num, int free)
{
    char *af[2] = { "alloc", "free" };

    uint32_t bg_start, bg_end, bgi;
    uint32_t block_size=get_block_size(fs);
    uint8_t buf[block_size];
    struct ext2_group_desc bg;
    
    free &= 0x1;

    if (free) { 
	bg_start = bg_end = *num/blocks_per_group(&fs->super);
    } else {
	bg_start = 0;
	bg_end = num_block_groups(&fs->super);
    }

    for (bgi=bg_start;bgi<bg_end;bgi++) {
	//get block group descriptor
	if (read_block_group(fs, bgi, &bg)) { 
	    ERROR("Failed to read block group %u in block %s\n", bgi, af[free]);
	    return -1;
	}
	
	if (read_block(fs,bg.bg_block_bitmap,buf)) { 
	    ERROR("Failed to read block bitmap in block %s\n", af[free]);
	    return -1;
	}
	
	int byte;
	int bit;
	uint8_t cur_byte;
    
	if (free) { 
	    uint32_t actual;
	    actual = *num;  // inode numbers start with 0 
	    actual %= blocks_per_group(&fs->super); // we want index in this group
	    byte = actual/8;
	    bit = actual%8;
	    buf[byte] &= ~(0x1<<bit);
	} else {
	    for (byte=0; byte<block_size; byte++) {
		cur_byte = buf[byte];
		for (bit=0; bit < 8; bit++, cur_byte>>=1) {
		    if (!(cur_byte & 0x01)) {
			buf[byte] |= (0x1 << bit);
			*num = byte*8 + bit ; //first block number is 0
			*num += bgi*blocks_per_group(&fs->super); // we want block index on whole volume
			goto out_good;
		    }
		}
	    }
	    // if we got here, we could not find anything...
	    *num=0;
	    return -1;
	}
    }
    
 out_good:

    if (write_block(fs,bg.bg_block_bitmap,buf)) { 
	ERROR("Failed to write block bitmap in block %s\n", af[free]);
	return -1;
    }
    
    return 0;
}

#define alloc_block(fs,num) alloc_free_block(fs,num,0)
#define free_block(fs,num) alloc_free_block(fs,&(num),1)
