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
#ifndef __FAT32FS_H__
#define __FAT32FS_H__


#define BOOTRECORD_SIZE	512
#define EOC_MIN         0xFFFFFF8
#define EOC_MAX         0xFFFFFFF
#define FREE_CLUSTER    0

struct fat32_bootrecord {
    // BIOS boot parameters
    uint8_t na[3];                      // jmp code for entry
    uint8_t OEM_identifier[8];
    uint16_t sector_size;		//number of bytes per sector
    uint8_t cluster_size; 		//number of sectors per cluster
    uint16_t reservedblock_size;	//number of reserved sectors, boot record is included
    uint8_t FAT_num;		        //number of FATs
    uint16_t directory_entry_num;	//number of directory entries in root directory
    uint16_t logical_volume_size;	//total number of sectors in the logical volume (for <64K sector volumes)
    uint8_t media_type;
    uint16_t FAT16_size;		//number of sectors per FAT12/16
    uint16_t track_size;		//number of sectors per track
    uint16_t head_num;			//number of heads/sides on the media
    uint32_t hidden_sector_num;	        //number of hidden sectors;
    uint32_t total_sector_num;	        //large number of sectors on media (>64K sector volumes)
    //end of BIOS boot parameters

    //Extended Boot Record
    uint32_t FAT32_size;		//number of sectors per FAT
    uint16_t flags;			//flags
    uint16_t version;			//FAT version number
    uint32_t rootdir_cluster;		//root directory cluster number, usually 2
    uint16_t FSInfo;			//sector number of FSInof Structure
    uint16_t backup_boot;		//sector number of backup boot sector
    uint8_t  reserved[12];		//should be 0
    uint8_t  drive_number;
    uint8_t  flag_Windowns_NT;		//reserved
    uint8_t  signature;			//must be 0x28 or 0x29
    uint32_t serial_num;		//Volume serial number 
    char     volument_lbl_string[11];   //padded with spaces	
    char     system_id[8];		//system identifier "FAT32"
    uint8_t  boot_code[420];
    uint16_t partition_signature;	//bootable partition signature 0xAA55
    //end of Extended Boot Record
}__attribute__((packed));


struct fat32_char {
    uint32_t cluster_size;
    uint32_t FAT32_size;
    uint32_t *FAT32_begin;
    uint32_t data_start;  //data region starting sector number
    uint32_t data_end;    //end of data sectors
};

union attributes {
    struct {
	char readonly : 1;
	char hidden : 1;
	char system : 1;
	char volumeid : 1;
	char dir : 1;
	char archive : 1; 
    } each_att;
    char attris;
};

typedef struct __attribute__((packed)) dir_entry  {
    char name[8];
    char ext[3];
    union attributes attri;  //attributes 
    char reserved0[8];
    uint16_t high_cluster;
    char reserved1[4];
    uint16_t low_cluster;
    uint32_t size;
} dir_entry; 
#endif
