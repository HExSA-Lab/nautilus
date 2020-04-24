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
 * Copyright (c) 2020, Michael A. Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2020, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __PARTITION_H__
#define __PARTITION_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLER__

#include <nautilus/blkdev.h>

#define PART_MBR_PTE 0
#define PART_GPT_PTE 1
#define PART_EBR_PTE 2

typedef enum nk_part_partition_entry_type {
    MBR_PTE = 0,
    GPT_PTE = 1,
    EBR_PTE = 2
} nk_pte_type;


// MBR Structures
/*
 *
 * NOTE: - ALL PARTITION TABLE AND MBR ENTRIES ARE LITTLE ENDIAN
 *       - PARTITION TABLE ENTRIES ARE NOT ALLIGNED ON 32 BIT BOUNDARIES
 *       - THERE SHOULD AT MOST 1 ACTIVE PARTITION ENTRY
 *
 */

// Based on osdev description of partition table entry
typedef struct nk_part_mbr_partition_table_entry {
    union {
        uint16_t val;
        struct {
            uint8_t status         : 8; // Boot flag, 0x80 = bootable, 0x0 = no, 0x01-0x79 = inval
            uint8_t chs_start_h    : 8; // CHS absolute start addr, starting head location
            uint8_t chs_start_s    : 6; // starting sector = first 6 bits, cylinder = next 10 bits
            uint16_t chs_start_c   : 10; // starting sector = first 6 bits, cylinder = next 10 bits
            uint8_t p_type         : 8; // Partition type, magic value
            uint8_t chs_end_h      : 8; // CHS end addr, ending head location
            uint8_t chd_end_s      : 6; // CHS end sector = first 6 bits, cylinder = next 10 bits
            uint16_t chd_end_c     : 10; // CHS end sector = first 6 bits, cylinder = next 10 bits
            uint32_t first_lba     : 32; // LBA of first absolute sector in partition
            uint32_t num_sectors   : 32; // number of sectors in partition
        } __packed;
    } __packed;
} __packed nk_part_mpte; 


// Based on MBR (master boot record) osdev entry
typedef struct nk_part_classical_mbr {
    union {
        uint8_t val[512];
        struct {
            uint8_t boot_code1[446]; // Bootstrap code
            nk_part_mpte partitions[4]; // partition table entries, 16 bytes each
            uint8_t boot_sig[2]; // boot signature, first byte = 0x55, second byte = 0xaa
        } __packed;
    } __packed;
} __packed nk_part_classic_mbr_t;

// Based on MBR (master boot record) wiki entry
typedef struct nk_part_modern_standard_mbr {
    union {
        uint8_t val[512];
        struct {
            uint8_t boot_code1[218]; // Bootstrap code part 1
            uint8_t disk_ts_zero[6]; // Only used in Windows/DOS, Must be 0
            uint8_t boot_code2[216]; // Bootstrap code part 2
            uint8_t disk_sig[4]; // Disk signature, 32-bits
            uint8_t is_copyable[2]; // 0x0000 = not copy protected, 0x5a5a = copy protected
            nk_part_mpte partitions[4]; // partition table entries, 16 bytes each
            uint8_t boot_sig[2]; // boot signature, first byte = 0x55, second byte = 0xaa
        } __packed;
    } __packed;
} __packed nk_part_modern_mbr_t;


// GUID Partition Table Structures
/*
 *
 * NOTE: - Some GUID header's and PTE field's endian-ness depends on machine
 *       - However, some fields are little endian ONLY
 *       - It varies by field, so please read the comments!
 *
 */

typedef struct nk_part_guid_pt_header {
    union {
        uint8_t val[512];
        struct {
            uint64_t sig; // GUID header signature (set value, differs between endian type)
            uint32_t rev; // GUID header revision (0x00000100 for GPT v1.0-v2.7)
            uint32_t size; // header size (little endian, typically 92 bytes)
            uint32_t crc_h; // CRC32 (cycle redundancy check) of header (little endian)
            uint32_t res1; // reserved, must be 0
            uint64_t clba; // current LBA (curr location of header copy)
            uint64_t blba; // backup LBA (location of other header copy)
            uint64_t flba; // first usable LBA (primary part-table's last LBA + 1)
            uint64_t llba; // last usable LBA (secondary PT's first LBA - 1)
            uint64_t dguid[2]; // Disk GUID in mixed endian
            uint64_t slba; // starting LBA of arr of partition entries (2 in primary copy)
            uint32_t num_ptes; // number of partition table entries in array
            uint32_t pte_size; // size of single pte
            uint32_t crc_pet; // CRC32 of partition entries array (little endian)
            uint8_t res2[420]; // Rest of block is reserved, 420 bytes for 512 Byte sectors
        } __packed;
    } __packed;
} __packed nk_part_gpt_header; 

typedef struct nk_part_gpt_entry_attributes {
    union {
        uint64_t val;
        struct {
            uint64_t plat_req    :1; // required by computer to function properly
            uint64_t EFI_ignore  :1; // Tells EFI firmware to ignore the content of partition
            uint64_t legacy_flag :1; // active bit in MBR entries
            uint64_t reserved    :45; // reserved for future use
            uint64_t part_flags  :16; // each partition type can have unique attrs
        } __packed;
    } __packed;
} __packed nk_part_gpte_attrs;

typedef struct nk_part_gpt_entry {
    union {
        uint8_t val[128];
        struct {
            uint8_t guidpt[16]; // GUID partition type (mixed endian)
            uint8_t upguid[16]; // Unique Partition GUID (mixed endian)
            uint64_t flba; // First LBA (little endian)
            uint64_t llba; // Last LBA (inclusive, usually odd)
            nk_part_gpte_attrs attrs; // Attribute flags, last 16 bits depend on partition type
            uint8_t pname[72]; // partition name (36 UTF-16LE code units)
        } __packed;
    } __packed;
} __packed nk_part_gpte;
 
typedef struct nk_part_gpt_entry_block {
    union {
        uint8_t val[4][128];
        nk_part_gpte block[4];
    } __packed; 
} __packed nk_part_gpte_block;


// Extended Partition Table Structures
/*
 *
 * NOTE: - Extended partition tables use the same structures as MBR partition tables
 *       - However, we should differentiate between them because their entries
 *          will differ slightly.
 *              - All EBR's only have 2 table entries filled out (instead of all 4)
 *       - Extended partition's offsets are also calculated differently and therefore
 *          should be contained in different structures
 *
 */

typedef struct nk_part_extended_mpte {
    union {
        uint32_t val;
        struct {
            nk_part_mpte entry1;
            nk_part_mpte entry2;
        } __packed;
    } __packed; 
} __packed nk_part_epte;


// Generic Partition Entry
/*
 *
 * NOTE: - Each partition state needs a single partition table entry associated with it
 *       - Instead of adding 3 fields to the partition state struct, we add a generic partition entry
 *       - We also have an enum that signals the partition entry type
 *
 */

typedef union nk_generic_partition_entry {
        nk_part_mpte mpte;
        nk_part_gpte gpte;
        nk_part_epte epte;
} __packed nk_part_entry;


// Partition Functions

int  nk_partition_init(struct naut_info *naut);
// add additional arg that hands back details of partition
int nk_enumerate_partitions(struct nk_block_dev *blockdev); 
void nk_partition_deinit();

#endif /* !__ASSEMBLER */

#ifdef __cplusplus
}
#endif

#endif /* !__PARTITION_H__ */
