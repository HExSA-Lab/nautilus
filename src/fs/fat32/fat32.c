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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2017, Yingyi Luo, Guixing Lin, and Jinghang Wang
 * Copyright (c) 2017, Peter Dinda
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Yingyi Luo <YingyiLuo2015@u.northwestern.edu>
 *           Guixing Lin <GuixingLin2018@u.northwestern.edu>
 *           Jinghang Wang <JinghangWang2018@u.northwestern.edu>
 *           Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/dev.h>
#include <nautilus/blkdev.h>
#include <nautilus/fs.h>

#include <fs/fat32/fat32.h>

#include "fat32_types.h"
#include "fat32_access.c"
#include "fat32fs.h"

static int fat32_exists(void *state, char *path);

static ssize_t fat32_read_write(void *state, void *file, void *srcdest, off_t offset, size_t num_bytes, int write)
{
    char *rw[2] = {"read","write"};
    
    if (srcdest == NULL) {
	return -1; // if buffer is NULL
    }

    write &= 0x1;

    struct fat32_state *fs = (struct fat32_state *) state;

    uint32_t dir_cluster_num;
    dir_entry dir_ent;

    DEBUG("%s from fs %s file %s offset %lu %lu bytes\n",rw[write], fs->fs->name, (char*) file, offset, num_bytes);
    
    int dir_num = path_lookup(fs, (char*) file, &dir_cluster_num, &dir_ent, 0);
    if (dir_num == -1) {
	DEBUG("Directory entry does not exist\n");
	return -1;
    }

/*
    dir_entry full_dirs[FLOOR_DIV(fs->bootrecord.sector_size, sizeof(dir_entry))];
    nk_block_dev_read(fs->dev, get_sector_num(dir_cluster_num, fs), 1, full_dirs, NK_DEV_REQ_BLOCKING);
    DEBUG("dir_num is %d\n", dir_num);
    dir_entry *dir = &(full_dirs[dir_num]);
    */
/*
    if (!dir) { // if dir doesn't exist
        return -1;
    }*/


    off_t file_size = (off_t)dir_ent.size;

    DEBUG("offset = %lu file_size = %u\n", offset, file_size);

    if (offset > file_size) {
	DEBUG("Offset past end of file\n");
        return -1; // invalid offset
    }
    
    if (offset == file_size && write == 0){
	DEBUG("Read at end of file\n");
        return 0; 
    }

    off_t remainder;
    if (write && dir_ent.attri.each_att.readonly) {
	DEBUG("Attempt to write read-only file\n");
	return -1;
    }  

    remainder = offset;

    //off_t remainder = offset;
    uint32_t cluster_size = get_cluster_size(fs); // in bytes
    uint32_t cluster_num = DECODE_CLUSTER(dir_ent.high_cluster, dir_ent.low_cluster);  //first cluster number of the file
    DEBUG("CLUSTER NUM is %u\n", cluster_num);
    uint32_t cluster_min = fs->bootrecord.rootdir_cluster; // min valid cluster number
    uint32_t cluster_max = fs->table_chars.data_end - fs->table_chars.data_start; // max valid cluster number

    // scan the FAT chain until we find the relevant cluster
    while (remainder > cluster_size) {
        uint32_t next = fs->table_chars.FAT32_begin[cluster_num];
        //check if next is valid
        if ( next >= EOC_MIN && next <= EOC_MAX ) {
	    DEBUG("Bogus next cluster value (%x)\n",next);
            return -1;
        }
        if ( next < cluster_min || next > cluster_max ) {
	    DEBUG("Bogus next cluster value (%x)\n",next);
            return -1;
        }
        cluster_num = next; 
        remainder -= cluster_size;
    }
    
    DEBUG("remainder = %d\n", remainder);

    char buf[cluster_size];
    if (write) { // write
        //suppose we have the file
        int rc;
        long src_off = 0;
        if (offset + num_bytes < file_size ) {  //don't need to allocate new block 
            //update file content
            do {
                if (nk_block_dev_read(fs->dev, get_sector_num(cluster_num, fs), fs->bootrecord.cluster_size, buf, NK_DEV_REQ_BLOCKING,0,0)) {
                    ERROR("Failed to read block.\n");
		    // should really unwind here
		    return -1;
                }
                memcpy(buf+remainder, srcdest + src_off, MIN(cluster_size - remainder, num_bytes-src_off));
                DEBUG("Num Bytes to be written: %d\n", MIN(cluster_size - remainder, num_bytes-src_off));
                if (nk_block_dev_write(fs->dev, get_sector_num(cluster_num, fs), fs->bootrecord.cluster_size, buf, NK_DEV_REQ_BLOCKING,0,0)) {
                    ERROR("Failed to write block.\n");
		    // should really unwind here
		    return -1;
                }
                src_off += MIN(cluster_size - remainder, num_bytes-src_off); 
                remainder = 0; //?
                uint32_t next = fs->table_chars.FAT32_begin[cluster_num];
                //if( next >= EOC_MIN && next <= EOC_MAX ) break;
                //if( next < cluster_min || next > cluster_max ) break;
                cluster_num = next;
            } while(src_off < num_bytes);
        } else {
	    //need to allocate block
            //1. fill up current block
            uint32_t next = cluster_num;
            while( ! (next >= EOC_MIN && next <= EOC_MAX) ) {
                cluster_num = next;
                if (nk_block_dev_read(fs->dev, get_sector_num(cluster_num, fs), fs->bootrecord.cluster_size, buf, NK_DEV_REQ_BLOCKING,0,0)) {
                    ERROR("Failed to read on block.\n");
		    // should really unwind here
		    return -1;
                }
                memcpy(buf+remainder, srcdest + src_off, MIN(cluster_size - remainder, num_bytes-src_off));
                DEBUG("Num Bytes to be written: %d\n", MIN(cluster_size - remainder, num_bytes-src_off));
                if (nk_block_dev_write(fs->dev, get_sector_num(cluster_num, fs), fs->bootrecord.cluster_size, buf, NK_DEV_REQ_BLOCKING,0,0)) {
                    ERROR("Failed to write on block.\n");
		    // should really unwind here
		    return -1;
                }
                src_off += MIN(cluster_size - remainder, num_bytes-src_off); 
                remainder = 0;
                next = fs->table_chars.FAT32_begin[cluster_num];
                if (next < cluster_min || ( next > cluster_max && next < EOC_MIN ) ) {
		    ERROR("No block available\n");
		    // should really unwind here
		    return -1;
                }
            }
            //2. if it still has sth to write, allocate new block
            uint32_t num_allocate = CEIL_DIV(num_bytes - src_off, cluster_size);
            DEBUG("num_allocate is %u\n", num_allocate);
            if (num_allocate > 0){
                if (grow_shrink_chain(fs, cluster_num, num_allocate) == -1 ) {
		    ERROR("Cannot allocate blocks\n");
		    // unwind... 
                    return -1;
                }
                cluster_num = fs->table_chars.FAT32_begin[cluster_num];
                DEBUG("cluster number after allocation is %u\n", cluster_num);
                do {
                    memset(buf, '\0', cluster_size);
                    memcpy(buf, srcdest + src_off, MIN(cluster_size, num_bytes-src_off));
                    DEBUG("Num Bytes to be written: %d\n", MIN(cluster_size, num_bytes-src_off));
                    if (nk_block_dev_write(fs->dev, get_sector_num(cluster_num, fs), fs->bootrecord.cluster_size, buf, NK_DEV_REQ_BLOCKING,0,0)) {
                        //TODO: RESTORE THE FILE
                        ERROR("Failed to write block.\n");
			// unwind... 
			return -1;
                    }
                    src_off += MIN(cluster_size, num_bytes-src_off); 
                    cluster_num = fs->table_chars.FAT32_begin[cluster_num];
                } while (num_bytes > src_off);
            }

            //Update directory entry
            dir_entry dir_buf[fs->bootrecord.directory_entry_num];
            if (nk_block_dev_read(fs->dev, get_sector_num(dir_cluster_num, fs), fs->bootrecord.cluster_size, dir_buf, NK_DEV_REQ_BLOCKING,0,0)) {
                ERROR("Failed to read block.\n");
		// unwind... 
		return -1;
            }

            uint32_t new_file_size = offset + num_bytes; 
            dir_buf[dir_num].size = new_file_size; 

            if (nk_block_dev_write(fs->dev, get_sector_num(dir_cluster_num, fs), fs->bootrecord.cluster_size, dir_buf, NK_DEV_REQ_BLOCKING,0,0)) {
                ERROR("Failed to write block.\n");
		// unwind... 
		return -1;
            }
        }
        return src_off;
    } else { // read
        long to_be_read = MIN(num_bytes, file_size - offset);
        DEBUG("to_be_read = %d\n", to_be_read);
        long dest_off = 0;
        
        do {
            if (nk_block_dev_read(fs->dev, get_sector_num(cluster_num, fs), fs->bootrecord.cluster_size, buf, NK_DEV_REQ_BLOCKING,0,0)) {
		ERROR("Failed to read block\n");
		return -1;
	    }

            if (remainder > 0) {
                memcpy(srcdest + dest_off, (char *)buf + remainder, MIN(cluster_size - remainder, to_be_read));
                to_be_read = to_be_read - cluster_size + remainder;
                
                dest_off += MIN(cluster_size - remainder, to_be_read);
                DEBUG("dest_off is %ld\n", dest_off);
                remainder = 0;
            } else {
                memcpy(srcdest + dest_off, buf, MIN(to_be_read, cluster_size));
                DEBUG("read buf is %s\n", buf);
                dest_off += MIN(to_be_read, cluster_size);
                DEBUG("dest_off is %ld\n", dest_off);
                to_be_read -= cluster_size;
            }
            //update cluster number
            uint32_t next = fs->table_chars.FAT32_begin[cluster_num];

            if( next >= EOC_MIN && next <= EOC_MAX ) {
		break; // end of file
	    }
           // if( next < cluster_min || next > cluster_max ) break;
            cluster_num = next;

        } while (to_be_read > 0);

        return MIN(num_bytes, file_size - offset);
    }

}

static ssize_t fat32_read(void *state, void *file, void *srcdest, off_t offset, size_t num_bytes)
{
    return fat32_read_write(state,file,srcdest,offset,num_bytes,0);
}

static ssize_t fat32_write(void *state, void *file, void *srcdest, off_t offset, size_t num_bytes)
{
    return fat32_read_write(state,file,srcdest,offset,num_bytes,1);
}

static int fat32_stat_path(void *state, char *path, struct nk_fs_stat *st)
{
    struct fat32_state *fs = (struct fat32_state *)state;
    dir_entry dir_ent;
    int num;
    int dir_num = path_lookup(fs, (char*) path, &num, &dir_ent, 0);
    if(dir_num == -1) return -1;

    st->st_size = dir_ent.size;

    return 0;
}


static void *fat32_create(void *state, char *path, int isdir)
{
    char *fd[2] = {"file","dir"};
    isdir &= 0x1;
    struct fat32_state *fs = (struct fat32_state *)state;
    DEBUG("create %s %s on fs %s\n", fd[isdir], path, fs->fs->name);

    if (fat32_exists(state, path)) {
        return NULL; // file already exists
    }
    
    int num_parts = 0;
    char **parts = split_path(path, &num_parts);
    if (!num_parts) {
        ERROR("Impossible path %s\n", path);
        free_split_path(parts,num_parts);
        return NULL;
    }

    char *name = parts[num_parts - 1]; // get name of file
    char path_without_name[strlen(path)];
    strncpy(path_without_name, path, strlen(path));
    for (int i = strlen(path_without_name); i >= 0; --i) {
        if (path_without_name[i] == '/') {
            path_without_name[i] = 0; // get path (excluding name) of file
            break;
        }
    }

    uint32_t dir_cluster_num;
    dir_entry dir_ent;
    uint32_t cluster_num;
    uint32_t num_dir_entry_per_file = FLOOR_DIV(fs->bootrecord.sector_size, sizeof(dir_entry));
    dir_entry full_dirs[num_dir_entry_per_file]; // buffer for b
    DEBUG("path_without_name is %s\n", path_without_name);
    int dir_num = path_lookup(fs, path_without_name, &dir_cluster_num, &dir_ent, 1);
    DEBUG("dir_num is %d\n", dir_num);
    DEBUG("dir_cluster_num (b) is %d\n", dir_cluster_num);

    if (path_without_name[0] == 0) { // for path "/name" 
        cluster_num = dir_cluster_num;
    }  else { // for path "/a/b/c/name", get dir_entry of c
        if (dir_num == -1) {
            DEBUG("directory does not exist: %s \n", path_without_name);
	    free_split_path(parts,num_parts);
            return NULL;
        }
        if (nk_block_dev_read(fs->dev, get_sector_num(dir_cluster_num, fs), 1, full_dirs, NK_DEV_REQ_BLOCKING,0,0)) { 
	    ERROR("block read failed\n");
	    free_split_path(parts,num_parts);
	    return NULL;
	}
        cluster_num = DECODE_CLUSTER(dir_ent.high_cluster, dir_ent.low_cluster);
    }

    DEBUG("begin of dir_cluster_num (c) is %d\n", cluster_num);

    uint32_t cluster_min = fs->bootrecord.rootdir_cluster; // min valid cluster number
    uint32_t cluster_max = fs->table_chars.data_end - fs->table_chars.data_start; // max valid cluster number
    uint32_t * fat = fs->table_chars.FAT32_begin;
    while (fat[cluster_num] >= cluster_min && fat[cluster_num] <= cluster_min) { // find the last cluster of c
        cluster_num = fat[cluster_num];
    }

    dir_entry full_dirs2[num_dir_entry_per_file]; // last cluster of c
    DEBUG("end of dir_cluster_num (c) is %d\n", cluster_num);
    if (nk_block_dev_read(fs->dev, get_sector_num(cluster_num, fs), 1, full_dirs2, NK_DEV_REQ_BLOCKING,0,0)) {
	ERROR("block read failed\n");
	free_split_path(parts,num_parts);
	return NULL;
    }

    int i = 0;
    while (full_dirs2[i].name[0] != 0) { // dir_entry already used
        ++i;
    }

    if (i == num_dir_entry_per_file) { // cluster is full of dir_entry ==> extend current file
        if (grow_shrink_chain(fs, cluster_num, 1) == -1) { // out of memory
	    ERROR("Failed to allocate block\n");
	    free_split_path(parts,num_parts);
            return NULL;
        }
        cluster_num = fat[cluster_num]; // advance to the allocated cluster
        i = 0; // start of cluster
        if (nk_block_dev_read(fs->dev, get_sector_num(cluster_num, fs), 1, full_dirs2, NK_DEV_REQ_BLOCKING,0,0)) { // read the new cluster of c
	    ERROR("Failed to read block\n");
	    free_split_path(parts,num_parts);
            return NULL;
	}	    
    }

    // fill info in dir_entry full_dirs2[i]??
    
    int new_file_cluster_num = grow_shrink_chain(fs, -1, 1); // allocate one cluster for the new file
    if (new_file_cluster_num == -1){ // out of memory
	ERROR("No room for file/dir\n");
	free_split_path(parts,num_parts);
        return NULL;
    }

    DEBUG("updating dir_entry, new file cluster_num = %d\n", new_file_cluster_num);

    if (isdir) {
        strncpy(full_dirs2[i].name, name, 8);
        full_dirs2[i].attri.attris = 0;
        full_dirs2[i].attri.each_att.dir = 1;
    } else {
        int ext_len, name_len;
        char file_name[8];
        char file_ext[3];
        filename_parser(name, file_name, file_ext, &name_len, &ext_len);
        strncpy(full_dirs2[i].name, file_name, name_len);
        strncpy(full_dirs2[i].ext, file_ext, ext_len);
        full_dirs2[i].attri.attris = 0;
    }

    full_dirs2[i].size = 0;
    full_dirs2[i].high_cluster = EXTRACT_HIGH_CLUSTER(new_file_cluster_num);
    full_dirs2[i].low_cluster = EXTRACT_LOW_CLUSTER(new_file_cluster_num);

    int rc;
    if (path_without_name[0] != 0) { // make sure path not like "/name"
        dir_ent.size += sizeof(dir_entry); // increment size of c in (dir entry of c in b)
        if (nk_block_dev_write(fs->dev, get_sector_num(dir_cluster_num, fs), fs->bootrecord.cluster_size, full_dirs, NK_DEV_REQ_BLOCKING,0,0)) {
            ERROR("Failed to write block for full_dirs.\n");
	    free_split_path(parts,num_parts);
	    return NULL;
        }
    }
    
    if (nk_block_dev_write(fs->dev, get_sector_num(cluster_num, fs), fs->bootrecord.cluster_size, full_dirs2, NK_DEV_REQ_BLOCKING,0,0)) {
        ERROR("Failed to write on block for full_dirs2.\n");
	free_split_path(parts,num_parts);
	return NULL;
    }

    free_split_path(parts,num_parts);
    //return ptr to the dir_entry of the newly created file
    // huh?
    return (void*) (1);
}

static void *fat32_create_file(void *state, char *path)
{
    return fat32_create(state, path, 0);
}

static int fat32_create_dir(void *state, char *path)
{
    void *f = fat32_create(state,path,1);

    if (!f) {
        return -1;
    } else {
        return 0;
    }
}

static int fat32_exists(void *state, char *path)
{
    dir_entry dir_ent;
    struct fat32_state *fs = (struct fat32_state *)state;
    return path_lookup(fs, path, NULL, &dir_ent, 0) != -1;
}

int fat32_remove(void *state, char *path)
{
    struct fat32_state *fs = (struct fat32_state *) state;
    uint32_t * fat = fs->table_chars.FAT32_begin;
    uint32_t fat_size = fs->table_chars.FAT32_size;
    uint32_t cluster_min = fs->bootrecord.rootdir_cluster; // min valid cluster number
    uint32_t cluster_max = fs->table_chars.data_end - fs->table_chars.data_start; // max valid cluster number
    uint32_t dir_cluster_num;
    dir_entry dir_ent;

    DEBUG("remove %s from fs %s\n",path,fs->fs->name);

    int dir_num = path_lookup(fs, (char*) path, &dir_cluster_num, &dir_ent, 0);
    if (dir_num == -1) {
	DEBUG("Path does not exist\n");
        return -1;
    }

    //clear FAT table entries for the file 
    uint32_t cluster_num = DECODE_CLUSTER(dir_ent.high_cluster, dir_ent.low_cluster);
    do {
        uint32_t next = fs->table_chars.FAT32_begin[cluster_num];
        if( next < cluster_min || ( next > cluster_max && next < EOC_MIN ) ) {
	    ERROR("Cluster chain has invalid entry\n");
            return -1;
        }
        fs->table_chars.FAT32_begin[cluster_num] = FREE_CLUSTER; 
        cluster_num = next;
    } while (! (cluster_num >= EOC_MIN && cluster_num <= EOC_MAX) );

    fs->table_chars.FAT32_begin[cluster_num] = FREE_CLUSTER; 
    if (nk_block_dev_write(fs->dev, fs->bootrecord.reservedblock_size, fat_size, fat, NK_DEV_REQ_BLOCKING,0,0)) {
	ERROR("Failed to write block\n");
	return -1;
    }

    if (nk_block_dev_write(fs->dev, fs->bootrecord.reservedblock_size + fat_size, fat_size, fat, NK_DEV_REQ_BLOCKING,0,0)) {
	ERROR("Failed to write block\n");
	return -1;
    }

    //remove the directory entry
    dir_entry full_dirs[FLOOR_DIV(fs->bootrecord.sector_size, sizeof(dir_entry))];
    if (nk_block_dev_read(fs->dev, get_sector_num(dir_cluster_num, fs), 1, full_dirs, NK_DEV_REQ_BLOCKING,0,0)) { 
	ERROR("Failed to read block\n");
	return -1;
    }
    DEBUG("dir_num is %d\n", dir_num);
    memset(full_dirs + dir_num, 0, sizeof(dir_entry));
    if (nk_block_dev_write(fs->dev, get_sector_num(dir_cluster_num, fs), 1, full_dirs, NK_DEV_REQ_BLOCKING,0,0)) {
	ERROR("Failed to write block\n");
	return -1;
    }
    return 0;
}

static void * fat32_open(void *state, char *path)
{
    struct fat32_state *fs = (struct fat32_state *)state;

    DEBUG("Open %s on fs %s\n", path, fs->fs->name);

    uint32_t dir_cluster_num;
    dir_entry dir_ent;
    int dir_num = path_lookup(fs, (char*) path, &dir_cluster_num, &dir_ent, 0);
    if (dir_num == -1) {
	DEBUG("Failed to look up path\n");
	return NULL;
    }
    /*
    dir_entry full_dirs[FLOOR_DIV(fs->bootrecord.sector_size, sizeof(dir_entry))];
    nk_block_dev_read(fs->dev, get_sector_num(dir_cluster_num, fs), 1, full_dirs, NK_DEV_REQ_BLOCKING);
    dir_entry *dir = &full_dirs[dir_num];
    */
    //uint32_t cluster_num = DECODE_CLUSTER(dir_ent.high_cluster, dir_ent.low_cluster);
    //DEBUG("open of %s returned cluster number %u\n", path, cluster_num);

    return (void*)path;
}

static int fat32_stat(void *state, void *file, struct nk_fs_stat *st)
{
    return fat32_stat_path(state, file, st);
}

static int fat32_truncate(void *state, void *file, off_t len)
{
    struct fat32_state *fs = (struct fat32_state *)state;
    uint32_t dir_cluster_num;
    dir_entry dir_ent;

    DEBUG("truncate file %s on fs %s to length %lu\n", (char*)file, fs->fs->name, len);

    int dir_num = path_lookup(fs, (char*) file, &dir_cluster_num, &dir_ent, 0);

    if (dir_num == -1) {
	DEBUG("Failed to look up path\n");
	return -1;
    }

    uint32_t cluster_size = get_cluster_size(fs);
    off_t file_size = (off_t)dir_ent.size;
    size_t file_size_clusters = CEIL_DIV(file_size,(off_t)cluster_size);
    off_t new_file_size = len; 
    size_t new_file_size_clusters = CEIL_DIV(new_file_size,(off_t)cluster_size);
    long size_clusters_diff = new_file_size_clusters-file_size_clusters; 
    uint32_t cluster_num = DECODE_CLUSTER(dir_ent.high_cluster, dir_ent.low_cluster);
    uint32_t cluster_min = fs->bootrecord.rootdir_cluster; // min valid cluster number
    uint32_t cluster_max = fs->table_chars.data_end - fs->table_chars.data_start; // max valid cluster number

    if (new_file_size_clusters < file_size_clusters) { 

	// shrink the file/dir

        off_t size = len;
        for(uint32_t n = 0; n < (new_file_size_clusters -1); n++) {
            uint32_t next = fs->table_chars.FAT32_begin[cluster_num];
            if(next < cluster_min || next > cluster_max ) return -1;
            if(next >= EOC_MIN && next <= EOC_MAX) return -1;
            cluster_num = next;
            size -= cluster_size;
        }
        char file_content[cluster_size];
        if (nk_block_dev_read(fs->dev, get_sector_num(cluster_num, fs), 1, file_content, NK_DEV_REQ_BLOCKING,0,0)) {
	    ERROR("Failed to read block\n");
	    return -1;
	}

        memset(file_content + size - 1, '\0', cluster_size - size + 1);
        if (nk_block_dev_write(fs->dev, get_sector_num(cluster_num, fs), 1, file_content, NK_DEV_REQ_BLOCKING,0,0)) { 
	    ERROR("Failed to write block\n");
	    return -1;
	}
	       
        //grow_shrink_chain(fs, cluster_num, size_clusters_diff); 
        /*
        uint32_t end_cluster_num = cluster_num;
        for(n = new_file_size_clusters; n < file_size_clusters; n++) {
            uint32_t next = fs->table_chars.FAT32_begin[cluster_num];
            if(next < cluster_min || next > cluster_max ) return -1;
            if(next >= EOC_MIN && next <= EOC_MAX) return -1;
            fs->table_chars.FAT32_begin[next] = FREE_CLUSTER; 
            cluster_num = next; 
        }
        fs->table_chars.FAT32_begin[end_cluster_num] = EOC_MIN;
        */

    } else if (new_file_size_clusters > file_size_clusters) { 
        // grow the file/dir
        for(uint32_t n = 0; n < (file_size_clusters -1); n++) {
            uint32_t next = fs->table_chars.FAT32_begin[cluster_num];
            if (next < cluster_min || next > cluster_max ) {
		ERROR("out-of-range cluster\n"); 
		return -1;
	    }
            if (next >= EOC_MIN && next <= EOC_MAX) {
		ERROR("End of cluster chain encountered\n");
		return -1;
	    }
            cluster_num = next;
        }
        //grow_shrink_chain(fs, cluster_num, size_clusters_diff);
    }
    if (grow_shrink_chain(fs, cluster_num, size_clusters_diff)==-1) {
	ERROR("Failed to allocate block\n");
	return -1;
    }
    //set new file size and write directory entry back 
    dir_entry full_dirs[FLOOR_DIV(fs->bootrecord.sector_size, sizeof(dir_entry))];
    
    if (nk_block_dev_read(fs->dev, get_sector_num(dir_cluster_num, fs), 1, full_dirs, NK_DEV_REQ_BLOCKING,0,0)) { 
	ERROR("FAiled to read block\n");
	return -1;
    }
    full_dirs[dir_num].size = (uint32_t) new_file_size; 

    if (nk_block_dev_write(fs->dev, get_sector_num(dir_cluster_num, fs), 1, full_dirs, NK_DEV_REQ_BLOCKING,0,0)) { 
	ERROR("Failed to write block\n");
	return -1;
    }

    return 0;
}

static void fat32_close(void *state, void *file)
{
    dir_entry dir_ent;
    struct fat32_state *fs = (struct fat32_state *)state;
    
    DEBUG("Close %s on fs %s\n",(char*)file,fs->fs->name);

    if (path_lookup(fs, file, NULL, &dir_ent, 0) == -1 ) {
        ERROR("cannot find the file to be closed");
    } else   {
        DEBUG("closed file %s\n", file);
    }
}

static struct nk_fs_int fat32_inter = {
    .stat_path = fat32_stat_path,
    .create_file = fat32_create_file,
    .create_dir = fat32_create_dir,
    .exists = fat32_exists,
    .remove = fat32_remove,
    .open_file = fat32_open,
    .stat = fat32_stat,
    .trunc_file = fat32_truncate,
    .close_file = fat32_close,
    .read_file = fat32_read,
    .write_file = fat32_write,
};

static void fat32_demo(struct fat32_state *s)
{
    char src[600];

    // create a new dir and a new file under it
    fat32_create_dir(s, "/live");
    fat32_create_file(s, "/live/foo.txt");

    // write into file just created
    fat32_write(s, "/live/foo.txt", "Hello world!\n", 0, 13);
    for(int i = 0; i < 600; i++) { 
	src[i] = 'o'; 
    }
    src[598] = '!';
    src[599] = '\n';
    fat32_write(s, "/bar.txt", src, 2, 600);

    // truncate and remove
    // fat32_truncate(s, "/bar.txt", 3);
    // fat32_remove(s, "/live/foo.txt");
}


int nk_fs_fat32_attach(char *devname, char *fsname, int readonly)
{
    struct nk_block_dev *dev = nk_block_dev_find(devname);
    uint64_t flags = readonly ? NK_FS_READONLY : 0;
    
    if (!dev) { 
        ERROR("Cannot find device %s\n",devname);
        return -1;
    }
    
    struct fat32_state *s = malloc(sizeof(*s));
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
    
    //read reserved block(boot record)
    if (read_bootrecord(s)) {
        ERROR("Cannot read bootrecord for fs FAT32 %s on device %s\n", fsname, devname);
        free(s);
        return -1;
    }
    
    if (read_FAT(s)){
        ERROR("Cannot load FAT into memory");
        free(s);
        return -1;
    }
    
    //DEBUG("System ID \"%s\"\n", s->bootrecord.system_id);
    DEBUG("Media byte %x\n", s->bootrecord.media_type);
    DEBUG("%lu bytes per logical sector\n",s->bootrecord.sector_size);
    DEBUG("%lu bytes per cluster\n",s->bootrecord.cluster_size * BLOCK_SIZE);
    DEBUG("%lu reserved sectors\n",s->bootrecord.reservedblock_size);
    DEBUG("First FAT starts at sector %lu\n",s->bootrecord.reservedblock_size);
    DEBUG("%lu FATs\n",s->bootrecord.FAT_num);
    DEBUG("%lu sectors per FAT\n",s->bootrecord.FAT32_size);
    DEBUG("Root directory start at cluster %lu (arbitrary size)\n",s->bootrecord.rootdir_cluster);
    //DEBUG("Data area starts at byte 1049600 (sector 2050)\n",);
    //DEBUG("129022 data clusters (66059264 bytes)\n",);
    DEBUG("%lu sectors/track, %lu heads\n",s->bootrecord.track_size, s->bootrecord.head_num);
    DEBUG("%lu hidden sectors\n",s->bootrecord.hidden_sector_num);
    DEBUG("%lu sectors total\n",s->bootrecord.total_sector_num);

    s->fs = nk_fs_register(fsname, flags, &fat32_inter, s);

    if (!s->fs) { 
	ERROR("Unable to register filesystem %s\n", fsname);
	free(s);
	return -1;
    }

    INFO("filesystem %s on device %s is attached (%s)\n", fsname, devname, readonly ?  "readonly" : "read/write");
    
    //fat32_demo(s);

    return 0;
}

int nk_fs_fat32_detach(char *fsname)
{
    struct nk_fs *fs = nk_fs_find(fsname);
    if (!fs) {
        return -1;
    } else {
        return nk_fs_unregister(fs);
    }
}
