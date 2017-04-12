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
/* fat32_access.c
 *
 * Contains the lowest level methods for interacting with an fat322
 * filesystem image.
 *
 * Built for adding fat32 functionality to the Nautilus OS, Northwestern University
 */

#include "fat32fs.h"
#include "fat32_types.h"
#include "fat32fs.h"

#define FLOOR_DIV(x,y) ((x)/(y))
#define CEIL_DIV(x,y)  (((x)/(y)) + !!((x)%(y)))
#define DIVIDES(x,y) (((x)%(y))==0)
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define DECODE_CLUSTER(h,l) ((((uint32_t)h)<<16) | ((uint32_t)l))
#define EXTRACT_HIGH_CLUSTER(x)	((x >> 16) & 0xFFFF)
#define EXTRACT_LOW_CLUSTER(x)	(x & 0xFFFF)

static char *rw[2] = { "read", "write" };

static int read_write_bootrecord(struct fat32_state *fs, int write)
{
    write &= 0x1;
    
    DEBUG("size of boot record %d\n",sizeof(fs->bootrecord));
    
    if (sizeof(fs->bootrecord)!=BOOTRECORD_SIZE) { 
	ERROR("Cannot %s bootrecord as the code is not internally consistent\n",rw[write]);
	return -1;
    }
    
    if (!DIVIDES(BOOTRECORD_SIZE,fs->chars.block_size) || 
	BOOTRECORD_SIZE < fs->chars.block_size) { 
	ERROR("Cannot %s bootrecord as it is too small or not a multiple of device blocks\n",rw[write]);
	return -1;
    }
    
    int rc = 0;
    if (write) { 
	rc = nk_block_dev_write(fs->dev,0,1,&fs->bootrecord,NK_DEV_REQ_BLOCKING,0,0); 
    } else {
	rc = nk_block_dev_read(fs->dev,0,1,&fs->bootrecord,NK_DEV_REQ_BLOCKING,0,0);
    }
    
    if (rc) { 
	ERROR("Failed to %s boot record due to device error\n",rw[write]);
	return -1;
    }

    return 0;

}
#define read_bootrecord(fs)  read_write_bootrecord(fs,0)
#define write_bootrecord(fs) read_write_bootrecord(fs,1)

static int read_FAT(struct fat32_state *fs)
{
    int rc = 0;
    uint32_t cluster_size = fs->bootrecord.cluster_size;
    uint32_t FAT32_size = fs->bootrecord.FAT32_size;
    
    fs->table_chars.cluster_size = cluster_size;
    fs->table_chars.FAT32_size = FAT32_size;

    if (!fs->table_chars.FAT32_begin) { 
	fs->table_chars.FAT32_begin = malloc(FAT32_size * fs->chars.block_size);
	if (!fs->table_chars.FAT32_begin) {
	    ERROR("Failed to allocate space to read the FAT into memory\n");
	    return -1;
	}
    }

    fs->table_chars.data_start = fs->bootrecord.FAT_num * FAT32_size + fs->bootrecord.reservedblock_size;
    fs->table_chars.data_end = fs->bootrecord.total_sector_num - 1; 
    
    rc = nk_block_dev_read(fs->dev, fs->bootrecord.reservedblock_size, FAT32_size, fs->table_chars.FAT32_begin, NK_DEV_REQ_BLOCKING,0,0);

    if (rc) {
	ERROR("Failed to read FAT from disk");
	free(fs->table_chars.FAT32_begin);
	fs->table_chars.FAT32_begin=0;
	return -1;
    }
    return 0;
}

static uint32_t get_cluster_size(struct fat32_state *fs)
{
    return (uint32_t)fs->bootrecord.sector_size * (uint32_t)fs->bootrecord.cluster_size;
}

static uint32_t get_sector_num(uint32_t cluster_num, struct fat32_state* fs)
{
    uint32_t num = fs->table_chars.data_start + (cluster_num - fs->bootrecord.rootdir_cluster) * fs->table_chars.cluster_size;
    /*	if(num < fs->table_chars.data_end)
	return num;
	return -1;*/
    return num;
}


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
    if (!parts) { 
	goto out_bad;
    }
    memset(parts,0,num_slashes*sizeof(char *));
    char *piece_start = path + 1;
    int i = 0;
    for (char *slash = strchr(path + 1, '/'); slash != NULL; slash = strchr(slash + 1, '/')) {
	int part_len = slash - piece_start;
	parts[i] = (char *) malloc((part_len + 1)*sizeof(char));
	if (!parts[i]) { 
	    goto out_bad;
	}
	strncpy(parts[i], piece_start, part_len);
	piece_start = slash + 1;
	i++;
    }
    // Get the last piece.
    parts[i] = (char *)malloc((strlen(piece_start) + 1)*sizeof(char));
    strcpy(parts[i], " ");
    strcpy(parts[i], piece_start);
    DEBUG("num_parts is %d\n", *num_parts);
    for(int n = 0; n <= i; n++){
    	DEBUG("parts[%d] is %s\n", n, parts[n]);
    }
   
    return parts;

 out_bad:
    ERROR("Allocation failure in path split\n");
    if (parts) { 
	for (i=0;i<*num_parts;i++){ 
	    if (parts[i]) { 
		free(parts[i]);
	    }
	}
	free(parts);
    }
    return 0;
}

static void free_split_path(char **list, int n)
{
    int i;
    for (i=0;i<n;i++) { 
	free(list[i]);
    }
    free(list);
}

static void filename_parser(char* path, char* name, char* ext, int* name_s, int* ext_s)
{
	
    int i = 0; 
    int ext_i = 0;
    //needs to handle directory(use split_path function)
    //validate file name(eg: length)
    //further optimization: handle long file name; handle relative path 
    DEBUG("PARSER  path = %s\n", path);
    while(path[i] != '.' && path[i]!= 0) {
	name[i] = path[i];
	i++;
    }
		
    *name_s = i;
    
    while (*name_s < 8) {
	name[(*name_s)++] = ' '; // append a space if name < 8 chars
    }
    
    i++;

    while(path[i] != '\0') {
	ext[ext_i] = path[i];
	i++;
	ext_i++;
    }

    *ext_s = ext_i; 

    while (*ext_s >0 && *ext_s < 3) {
	ext[(*ext_s)++] = ' '; // append a space if ext < 3 chars
    }
}

static void debug_print_file(struct fat32_state* state, uint32_t cluster_num, uint32_t size)
{
    char file[512];
    if (nk_block_dev_read(state->dev, get_sector_num(cluster_num, state), 1, file, NK_DEV_REQ_BLOCKING,0,0)) {
	ERROR("Failed to read block\n");
	return;
    }
    char output[size+1];
    
    strncpy(output, file, size);
    output[size] = 0;
    DEBUG("file contents: %s\n", output);
}

static int isLowerCase(char c) 
{
    return (c >= 'a' && c <= 'z');
}

static char* toUpperCase(char* s) 
{
    for (int i = 0; s[i] != '\0'; ++i) {
	if (isLowerCase(s[i])) {
	    s[i] += ('A' - 'a');
	}
    }
    return s;
}

static int path_lookup( struct fat32_state* state, char* path, uint32_t* dir_cluster_num, dir_entry* file_entry, int is_dir)
{
    // if look for root
    if (path[0] == 0) {
	DEBUG("path_lookup: path is emtpy, trying to loop up root directory");
	*dir_cluster_num = state->bootrecord.rootdir_cluster;;
	return 0;
    }
    char found = 0;
    int num_parts;
    char** parts = split_path(toUpperCase(path), &num_parts);
    uint32_t clu_per_sec = state->bootrecord.cluster_size;
    uint32_t dir_entry_num = FLOOR_DIV(state->bootrecord.sector_size, sizeof(dir_entry));
    uint32_t cluster_min = state->bootrecord.rootdir_cluster; // min valid cluster number
    uint32_t cluster_max = state->table_chars.data_end - state->table_chars.data_start;
    *dir_cluster_num = state->bootrecord.rootdir_cluster;
    uint32_t dir_sector = get_sector_num(*dir_cluster_num, state);
    
    //DEBUG("table entry[2] = %x\n", state->table_chars.FAT32_begin[2]);
    DEBUG("root sector num is %d\n", dir_sector);
    DEBUG("directory entry size %d\n", sizeof(dir_entry));
    //dir_entry* root_data = (dir_entry*)malloc(state->bootrecord.sector_size);
    //nk_block_dev_read(state->dev, root_sector, 1, root_data, NK_DEV_REQ_BLOCKING);
    dir_entry dir_data[dir_entry_num];

    for(int n = 0; n < (num_parts-1); n++){
	char* dir_name = parts[n];
	int dir_len = strlen(dir_name);
	DEBUG("dir_len is %d\n", dir_len);
	DEBUG("dir_name is %s\n", dir_name);
	while(! (*dir_cluster_num >= EOC_MIN && *dir_cluster_num <= EOC_MAX) ){
	    if (nk_block_dev_read(state->dev, dir_sector, clu_per_sec, dir_data, NK_DEV_REQ_BLOCKING,0,0)) { 
		ERROR("Failed to read block\n");
		free_split_path(parts, num_parts);
		return -1;
	    }
		
	    for(int k = 0; k < dir_entry_num; k++){
		dir_entry data = dir_data[k];
		DEBUG("k is %d, dir_entry name is %s, ext is %s, cluster number is %d\n", k, data.name, data.ext, 
		      DECODE_CLUSTER(data.high_cluster,data.low_cluster));
		DEBUG("is directory: %d\n", data.attri.each_att.dir);
		
		if (data.attri.each_att.dir != 0 && strncmp(data.name, dir_name, dir_len) == 0) {
		    //if(strncmp(data.name, dir_name, dir_len) == 0){
		    *dir_cluster_num = DECODE_CLUSTER(data.high_cluster,data.low_cluster);
		    dir_sector = get_sector_num(*dir_cluster_num, state);
		    found = 1;
		    break;
		}
	    }
	    
	    if (found == 1){
		break; 
	    } else {
		
		*dir_cluster_num = state->table_chars.FAT32_begin[*dir_cluster_num];
		if (*dir_cluster_num < cluster_min || ( *dir_cluster_num > cluster_max && *dir_cluster_num < EOC_MIN )){
		    free_split_path(parts, num_parts);
		    return -1;
		}
		dir_sector = get_sector_num(*dir_cluster_num, state);
	    }
	}
	
	if (found == 0) {
	    DEBUG("could not find directory %s\n", dir_name);
	    free_split_path(parts, num_parts);
	    return -1;
	} else {
	    found = 0;
	}
    }
    /*	
	while(){
	//if not found and more cluster
	call helper
	}*/
    char file_name[8];
    char file_ext[3];
    int i, name_size, ext_size;
    if (is_dir == 0){
	filename_parser(parts[num_parts-1], file_name, file_ext, &name_size, &ext_size);
    }else{
	name_size = strlen(parts[num_parts-1]);
	strncpy(file_name, parts[num_parts-1], name_size);
	ext_size = 0;
	
    }
    
    DEBUG("read file name is %s, ext is %s, ext_size is %d\n", file_name, file_ext, ext_size);
    while(! (*dir_cluster_num >= EOC_MIN && *dir_cluster_num <= EOC_MAX) ){
	if (nk_block_dev_read(state->dev, dir_sector, clu_per_sec, dir_data, NK_DEV_REQ_BLOCKING,0,0) ) {
	    ERROR("Failed to read block\n");
	    free_split_path(parts, num_parts);
	    return -1;
	}
	
	for(i = 0; i < dir_entry_num; i++){
	    dir_entry data = dir_data[i];
	    DEBUG("i is %d, dir_entry name is %s, ext is %s, cluster_num is %d\n", i, data.name, data.ext,
		  DECODE_CLUSTER(data.high_cluster,data.low_cluster));	
	    if ( strncmp(data.name, file_name, name_size) == 0 ){
		DEBUG("enter if statement.\n");
		if (ext_size == 0 ){
		    uint32_t cluster_num = DECODE_CLUSTER(data.high_cluster,data.low_cluster);
		    DEBUG("cluster num is %d\n", cluster_num);	
		    //debug_print_file(state, cluster_num, root_data[i].size);
		    *file_entry = data;
		    free_split_path(parts, num_parts);
		    return i; //return the position of file in the directory
		} else {
		    if (strncmp(data.ext, file_ext, ext_size) == 0) {
			*file_entry = data;
			uint32_t cluster_num = DECODE_CLUSTER(data.high_cluster, data.low_cluster);
			DEBUG("cluster num is %d\n", cluster_num);	
			//debug_print_file(state, cluster_num, root_data[i].size);
			free_split_path(parts, num_parts);
			return i; //return the position of file in the directory
		    }
		}
	    }
	}

	*dir_cluster_num = state->table_chars.FAT32_begin[*dir_cluster_num];
	if (*dir_cluster_num < cluster_min || 
	    ( *dir_cluster_num > cluster_max && *dir_cluster_num < EOC_MIN )) {
	    free_split_path(parts, num_parts);
	    return -1;
	}
	dir_sector = get_sector_num(*dir_cluster_num, state);
    }
    
    free_split_path(parts, num_parts);
    return -1; //cannot find file
}

// misnamed function - this expands or shrinks a cluster chain
static int grow_shrink_chain(struct fat32_state* state, uint32_t cluster_entry, long num) 
{
    uint32_t * fat = state->table_chars.FAT32_begin;
    uint32_t size = state->table_chars.FAT32_size; 
    uint32_t start = state->bootrecord.rootdir_cluster;
    uint32_t cluster_entry_cpy = cluster_entry;
    uint32_t count = 0;

    if (num > 0) { 
	//grow chain
	for(uint32_t i = start; i < size; i++) {
	    uint32_t tmp = fat[i];
	    if( (tmp << 1) == FREE_CLUSTER ){
		//update FAT table
		DEBUG("ALLOC BLOCK: i is %u\n", i);
		if (cluster_entry_cpy == -1) { // alloc block for new file
		    fat[i] = EOC_MIN;
		    return i;
		} else { // extend current file
		    fat[cluster_entry] = i;
		    count++;
		    cluster_entry = i;
		}
	    }
	    if (count == num) {
		break;
	    }
	}
	
	if (cluster_entry_cpy == -1) { 
	    // alloc blcok for new file & out of memory
	    return -1;
	}
	
	fat[cluster_entry] = EOC_MIN;
	
	if (count < num){ 
	    // extend current file & ran out of space ==> undo allocation
	    uint32_t next = fat[cluster_entry_cpy];
	    fat[cluster_entry_cpy] = EOC_MIN;
	    cluster_entry_cpy = next;
	    while (cluster_entry_cpy != EOC_MIN) {
		next = fat[cluster_entry_cpy];
		fat[cluster_entry_cpy] = FREE_CLUSTER;
		cluster_entry_cpy = next;
	    }
	    fat[cluster_entry_cpy] = FREE_CLUSTER;
	    // 
	    // nk_block_dev_read(state->dev, state->bootrecord.reservedblock_size, size, state->table_chars.FAT32_begin, NK_DEV_REQ_BLOCKING);
	    return -1;
	}
    } else if(num < 0) { 
	// shrink cluster chain
	long n = -(num); 
	uint32_t cluster_min = state->bootrecord.rootdir_cluster; // min valid cluster number
    	uint32_t cluster_max = state->table_chars.data_end - state->table_chars.data_start; // max valid cluster number
        for(uint32_t i = 0; i < n; i++) {
            uint32_t next = fat[cluster_entry];
            if (next < cluster_min || next > cluster_max ) {
		return -1;
	    }
            if (next >= EOC_MIN && next <= EOC_MAX) {
		return -1;
	    }
            fat[next] = FREE_CLUSTER; 
            cluster_entry = next; 
        }
        fat[cluster_entry_cpy] = EOC_MIN;
    
    }

    // flush fat back to disk

    // copy 1
    if (nk_block_dev_write(state->dev, state->bootrecord.reservedblock_size, size, fat, NK_DEV_REQ_BLOCKING,0,0)) {
	ERROR("Failed to write blocks\n");
	return -1;
    }

    // copy 2
    if (nk_block_dev_write(state->dev, state->bootrecord.reservedblock_size+size, size, fat, NK_DEV_REQ_BLOCKING,0,0)) {
	ERROR("Failed to write blocks\n");
	return -1;
    }

    return 0; 
}


#define BYTES_PER_LINE 16
static void mem_print(char *addr, int len)
{
    int i, j, k;
    int size = 1;
    for (i=0;i<len;i+=BYTES_PER_LINE) {
	nk_vc_printf("%016lx :",addr+i);
	nk_vc_printf(" ");
	for (j=0;j<BYTES_PER_LINE && (i+j)<len; j+=size) {
	    for (k=0;k<size;k++) { 
		nk_vc_printf("%c", isalnum(*(uint8_t*)(addr+i+j+k)) ? 
			     *(uint8_t*)(addr+i+j+k) : '.');
	    }
	}
	nk_vc_printf("\n");
    }
}
