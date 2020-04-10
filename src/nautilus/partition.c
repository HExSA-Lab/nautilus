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
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
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

#include <nautilus/nautilus.h>
#include <nautilus/blkdev.h>
#include <nautilus/partition.h>
#include <nautilus/shell.h>

#ifndef NAUT_CONFIG_DEBUG_PARTITION
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("partition: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("partition: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("partition: " fmt, ##args)

#define PART_PT_SIZE 4
#define PART_MBR_OFFSET 0
#define PART_GPTH_OFFSET 1
#define PART_HPC 255 
#define PART_GPT_MAGIC_NUM 0xEE
#define PART_EBR_MAGIC_NUM 0x0F

#define CEIL_DIV(x,y)  (((x)/(y)) + !!((x)%(y)))

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK(state) _state_lock_flags = spin_lock_irq_save(&state->lock)
#define STATE_UNLOCK(state) spin_unlock_irq_restore(&(state->lock), _state_lock_flags)

// Change later
struct partition_state {
    struct nk_block_dev *blkdev;
    spinlock_t lock;
    nk_pte_type ptype;
    nk_part_entry pte;
    //uint64_t len;
    uint64_t block_size;
    uint64_t num_blocks;
    // Add Initial Block
    uint32_t ILBA; // First LBA of the partition
    struct nk_block_dev *underlying_blkdev; // Blockdevice this partition was created from
};


static int get_characteristics(void *state, struct nk_block_dev_characteristics *c)
{
    STATE_LOCK_CONF;
    struct partition_state *s = (struct partition_state *)state;
    
    STATE_LOCK(s);
    c->block_size = s->block_size;
    c->num_blocks = s->num_blocks;
    STATE_UNLOCK(s);
    return 0;
}

// is block num and count within range of partition?
// translate (blocknum+starting point of part, then pass that info to underlying device)
static int read_blocks(void *state, uint64_t blocknum, uint64_t count, uint8_t *dest,void (*callback)(nk_block_dev_status_t, void *), void *context)
{
    STATE_LOCK_CONF;
    struct partition_state *s = (struct partition_state *)state;

    DEBUG("read_blocks on device %s starting at %lu for %lu blocks\n",
	  s->blkdev->dev.name, blocknum, count);

    STATE_LOCK(s);
    uint64_t real_offset = s->ILBA + blocknum + count;
    struct nk_block_dev_characteristics blk_dev_chars;
    nk_block_dev_get_characteristics(s->underlying_blkdev, &blk_dev_chars);
    if (blocknum+count >= s->num_blocks) { 
	    STATE_UNLOCK(s);
	    ERROR("Illegal access past end of partition\n");
	    return -1;
    } else if (real_offset >= blk_dev_chars.num_blocks) {
        STATE_UNLOCK(s);
        ERROR("Illegal access past end of block device\n");
        return -1;
    } else {
        nk_block_dev_read(s->underlying_blkdev, blocknum+s->ILBA, count, dest, NK_DEV_REQ_BLOCKING, callback, context);
	    STATE_UNLOCK(s);
        if (callback) {
	        callback(NK_BLOCK_DEV_STATUS_SUCCESS,context);
	    }
	    return 0;
    }
}

static int write_blocks(void *state, uint64_t blocknum, uint64_t count, uint8_t *src,void (*callback)(nk_block_dev_status_t, void *), void *context)
{
    STATE_LOCK_CONF;
    struct partition_state *s = (struct partition_state *)state;

    DEBUG("write_blocks on device %s starting at %lu for %lu blocks\n",
	  s->blkdev->dev.name, blocknum, count);

    STATE_LOCK(s);
    uint64_t real_offset = s->ILBA + blocknum + count;
    struct nk_block_dev_characteristics blk_dev_chars;
    nk_block_dev_get_characteristics(s->underlying_blkdev, &blk_dev_chars);
    if (blocknum+count >= s->num_blocks) { 
	    STATE_UNLOCK(s);
	    ERROR("Illegal access past end of partition\n");
	    return -1;
    } else if (real_offset >= blk_dev_chars.num_blocks) {
        STATE_UNLOCK(s);
	    ERROR("Illegal access past end of disk\n");
	    return -1;
    } else {
	    nk_block_dev_write(s->underlying_blkdev, blocknum+s->ILBA, count, src, NK_DEV_REQ_BLOCKING, callback, context);
	    STATE_UNLOCK(s);
        if (callback) { 
	        callback(NK_BLOCK_DEV_STATUS_SUCCESS,context);
	    }
	    return 0;
    }
}



static struct nk_block_dev_int inter = 
{
    .get_characteristics = get_characteristics,
    .read_blocks = read_blocks,
    .write_blocks = write_blocks,
};

static int nk_generate_partition_name(int partition_num, char *blk_name, char **new_name)
{
    // Create buffer char arr for int to str conversion
    char buffer[DEV_NAME_LEN];
    buffer[0] = 'p';

    int num_digits = 0;
    int temp_num = partition_num;
    do {
        temp_num /= 10;
        num_digits++;
    } while (temp_num > 0);
              
    // convert i (loop control var) to str
    itoa(partition_num, buffer+1, num_digits-1);
    const char *p_num_str = buffer;

    // new char* to hold new device name 
    char *part_str = malloc(sizeof(char)*DEV_NAME_LEN);
    if (!part_str) {
            ERROR("failed to allocate partition name\n");
            return -1;
    }
    memcpy(part_str, 0, sizeof(*part_str));

    // Copy blkdev to new part_str and concat i to it
    strncpy(part_str, blk_name, DEV_NAME_LEN);
    part_str = strncat(part_str, p_num_str, DEV_NAME_LEN);
    *new_name = part_str;
    return 0;
}

int nk_ebr_enumeration(struct nk_block_dev *blockdev, 
                        nk_part_modern_mbr_t *MBR,
                        struct nk_block_dev_characteristics blk_dev_chars,
                        int extended_entry)
{
    // Work through all the extended partitions until we hit the end
    uint64_t start_sec = MBR->partitions[extended_entry].first_lba;
    uint64_t ext_size = MBR->partitions[extended_entry].num_sectors;
    uint64_t next_ebr = MBR->partitions[extended_entry].first_lba;
    uint64_t mbr_blks = (sizeof(*MBR))/(blk_dev_chars.block_size);
    int i = 4;
    while (next_ebr) {
        nk_block_dev_read(blockdev, next_ebr, mbr_blks, MBR, NK_DEV_REQ_BLOCKING, 0, 0);
        struct partition_state *ps = malloc(sizeof(struct partition_state));
            
        if (!ps) {
            ERROR("Cannot allocate data structure for partition\n");
            return -1;
        }
         
        memset(ps,0,sizeof(*ps));
        
        // Initialize the partition state lock and pte    
        spinlock_init(&ps->lock);
        ps->pte.epte.entry1 = MBR->partitions[0];
        ps->pte.epte.entry2 = MBR->partitions[1];
        ps->ptype = PART_EBR_PTE;

        // Fill out rest of partition state
        ps->block_size = blk_dev_chars.block_size;
        ps->num_blocks = ps->pte.epte.entry1.num_sectors;
        ps->ILBA = ps->pte.epte.entry1.first_lba + next_ebr;
        ps->underlying_blkdev = blockdev;

        // Calculate where next block will be
        if (ps->pte.epte.entry2.num_sectors == 0) {
            // Last MBR in the table, don't have to worry about updating
            next_ebr = 0;
        } else {
            next_ebr = start_sec + ps->pte.epte.entry2.first_lba;
        }

        // Get new partition name
        char **new_name = 0;
        if (nk_generate_partition_name(i+1, blockdev->dev.name, new_name)) {
            free(ps);
            return -1; 
        }

        // Register new partition
        ps->blkdev = nk_block_dev_register(*new_name, 0, &inter, ps);
        free(*new_name);
        
        if (!ps->blkdev) {
            ERROR("Failed to register partition\n");
            free(ps);
            return -1;
        } 

        INFO("Added patition as %s, blocksize=%lu, ILBA=%lu, numblocks=%lu\n", ps->blkdev->dev.name, ps->block_size, ps->ILBA, ps->num_blocks);
        i++;
    }
    return 0;
} 

int nk_mbr_enumeration(struct nk_block_dev *blockdev, nk_part_modern_mbr_t *MBR, struct nk_block_dev_characteristics blk_dev_chars)
{
    int i;
    for (i = 0; i < PART_PT_SIZE; i++) {
        struct partition_state *ps = malloc(sizeof(struct partition_state));
            
        if (!ps) {
            ERROR("Cannot allocate data structure for partition\n");
            return -1;
        }
         
        memset(ps,0,sizeof(*ps));
        
        // Initialize the partition state lock and pte    
        spinlock_init(&ps->lock);
        ps->pte.mpte = MBR->partitions[i];
        ps->ptype = PART_MBR_PTE;

        // Check if curr partition table entry has at least 1 sector and less than max sectors
        if (ps->pte.mpte.num_sectors == 0  || ps->pte.mpte.num_sectors > blk_dev_chars.num_blocks) {
            free(ps);
            continue; 
        }

        // Fill out rest of partition state
        ps->block_size = blk_dev_chars.block_size;
        ps->num_blocks = ps->pte.mpte.num_sectors;
        ps->ILBA = ps->pte.mpte.first_lba;   
        ps->underlying_blkdev = blockdev;
        
        // Get new partition name
        char **new_name = 0;
        if (nk_generate_partition_name(i+1, blockdev->dev.name, new_name)) {
            free(ps);
            return -1; 
        }

        // Register new partition
        ps->blkdev = nk_block_dev_register(*new_name, 0, &inter, ps);
        free(*new_name);
        
        if (!ps->blkdev) {
            ERROR("Failed to register partition\n");
            free(ps);
            return -1;
        } 

        INFO("Added patition as %s, blocksize=%lu, ILBA=%lu, numblocks=%lu\n", ps->blkdev->dev.name, ps->block_size, ps->ILBA, ps->num_blocks);
    }
    return 0;
} 

int nk_gpt_enumeration(struct nk_block_dev *blockdev, nk_part_modern_mbr_t *MBR, struct nk_block_dev_characteristics blk_dev_chars)
{ 
    // Create GPT Header struct to hold GPT header info
    nk_part_gpt_header *GPTH = malloc(sizeof(*GPTH));
    if (!GPTH) {
        ERROR("Cannot allocate data structure for GPT Header\n");
        return -1;
    } 
    memset(GPTH, 0, sizeof(*GPTH));

    uint64_t gpth_blks = CEIL_DIV((sizeof(*GPTH)), (blk_dev_chars.block_size));

    // Read GPT Header
    nk_block_dev_read(blockdev, PART_GPTH_OFFSET, gpth_blks, GPTH, NK_DEV_REQ_BLOCKING, 0, 0);

    uint32_t entry_size = GPTH->pte_size;
    uint32_t num_entries = GPTH->num_ptes;
    // LBA of PT's first entry
    uint64_t SLBA = GPTH->slba;

    // Print out entry size, num entries, and the GPT starting LBA
    DEBUG("PTE Size: %lu, Num PTEs: %lu, Starting LBA: %lu\n", entry_size, num_entries, SLBA);
    DEBUG("GPTE Block Struct Size: %lu \n", sizeof(nk_part_gpte_block));

    uint64_t num_bytes = entry_size * num_entries;
    uint64_t blocks_to_read = CEIL_DIV(num_bytes,(blk_dev_chars.block_size));
    nk_part_gpte *gpte_arr = malloc(blocks_to_read*(blk_dev_chars.block_size));

    // Print number of blocks we're reading
    DEBUG("Reading %lu Bytes which = %lu 512-Byte blocks\n", num_bytes, blocks_to_read);
    DEBUG("Total size of gpte_arr = %lu\n", sizeof(gpte_arr));
    
    // Read enough blocks to cover all of GPT
    nk_block_dev_read(blockdev, SLBA, blocks_to_read, gpte_arr, NK_DEV_REQ_BLOCKING, 0, 0);

    // ptei = partition table entry index (within block)
    int ptei;
    for (ptei = 0; ptei < num_entries; ptei++)
    {
        nk_part_gpte temp_entry = gpte_arr[ptei];

        // Ignore the content of this partition! 
        if (temp_entry.attrs.EFI_ignore) {
            DEBUG("Partition entry %lu ignored\n", ptei);
            continue;
        }

        // Check if curr partition table entry has at least 1 sector and less than max sectors
        uint64_t num_sectors = temp_entry.llba - temp_entry.flba;
        if (num_sectors == 0  || num_sectors > blk_dev_chars.num_blocks) {
            // Generates LOTS of debug output
            //DEBUG("Partition entry %lu's size (%lu) is nonsense\n", ptei, num_sectors);
            continue; 
        }

        // allocate partition state
        struct partition_state *ps = malloc(sizeof(struct partition_state));
        if (!ps) {
            free(gpte_arr); 
            ERROR("Cannot allocate data structure for partition\n");
            return -1;
        }
        memset(ps,0,sizeof(*ps));

        // Initialize the partition state lock 
        spinlock_init(&ps->lock);

        // Fill out rest of partition state
        ps->pte.gpte = temp_entry;
        ps->ptype = PART_GPT_PTE;
        ps->block_size = blk_dev_chars.block_size;
        ps->num_blocks = num_sectors;
        ps->ILBA = ps->pte.gpte.flba;   
        ps->underlying_blkdev = blockdev;

        int partition_num = ptei;
        char **new_name = 0;
        if (nk_generate_partition_name(partition_num+1, blockdev->dev.name, new_name)) {
            free(ps);
            free(gpte_arr);
            return -1; 
        }

        // Register new partition
        ps->blkdev = nk_block_dev_register(*new_name, 0, &inter, ps);
        free(*new_name);

        if (!ps->blkdev) {
            ERROR("Failed to register partition\n");
            free(gpte_arr);
            free(ps);
            return -1;
        } 

        INFO("Added patition as %s, blocksize=%lu, ILBA=%lu, numblocks=%lu\n", ps->blkdev->dev.name, ps->block_size, ps->ILBA, ps->num_blocks);    
    }
    free(gpte_arr);
    return 0;
}

int nk_enumerate_partitions(struct nk_block_dev *blockdev)
{
    // Create MBR struct to hold MBR info
    nk_part_modern_mbr_t *MBR = malloc(sizeof(*MBR));
    if (!MBR) {
        ERROR("Cannot allocate data structure for MBR\n");
        return -1;
    } 
    memset(MBR, 0, sizeof(*MBR));

    // Collect blockdev characteristics (used to get MBR info)
    struct nk_block_dev_characteristics blk_dev_chars;
    nk_block_dev_get_characteristics(blockdev, &blk_dev_chars);
    uint64_t mbr_blks = (sizeof(*MBR))/(blk_dev_chars.block_size);

    // Read MBR
    nk_block_dev_read(blockdev, PART_MBR_OFFSET, mbr_blks, MBR, NK_DEV_REQ_BLOCKING, 0, 0);

    // Check to see if "magic" signature is present
    // If not, a master boot record isn't available.
    // That means we don't recognize this type of partitioning.
    if (MBR->boot_sig[0] != 0x55 && MBR->boot_sig[1] != 0xaa) {
        ERROR("Invalid MBR Boot Signature!\n");
        return -1;
    }

    int rc = 0;
    // First partition type of 0xEE signals a protective MBR and that a GPT is being used
    // First or second partition type of 0x0F signals that an EBR is being used (with LBA)
    // Else, treat MBR as a normal MBR and proceed with regular mbr enumeration
    if (MBR->partitions[0].p_type == PART_GPT_MAGIC_NUM) {
        DEBUG("The first partition type is: %lu\n", MBR->partitions[0].p_type);
        DEBUG("Starting GPT Enumeration\n");
        rc = nk_gpt_enumeration(blockdev, MBR, blk_dev_chars); 
    } else if (MBR->partitions[0].p_type == PART_EBR_MAGIC_NUM) {
        DEBUG("The first partition type is: %lu\n", MBR->partitions[0].p_type);
        DEBUG("Starting MBR Enumeration\n");
        rc = nk_mbr_enumeration(blockdev, MBR, blk_dev_chars);
        if (rc) {
            free(MBR);
            return rc;
        }
        DEBUG("MBR Enumeration finished. Starting EBR Enumeration\n");
        rc =  nk_ebr_enumeration(blockdev, MBR, blk_dev_chars, 0);
    } else if (MBR->partitions[1].p_type == PART_EBR_MAGIC_NUM) {
        DEBUG("The second partition type is: %lu\n", MBR->partitions[1].p_type);
        DEBUG("Starting MBR Enumeration\n");
        int rc = nk_mbr_enumeration(blockdev, MBR, blk_dev_chars);
        if (rc) {
            free(MBR);
            return rc;
        }
        DEBUG("MBR Enumeration finished. Starting EBR Enumeration\n");
        rc = nk_ebr_enumeration(blockdev, MBR, blk_dev_chars, 1);
    } else {
        /*
         * We only handle 3 types of partition tables. 1) GUID Partition Tables
         * 2) Extended Boot Records 3) Master boot records
         * This else case handles master boot records. If we don't recognize the
         * partitioning scheme, we treat it as a master boot record.
         */

        DEBUG("The first partition type is: %lu\n", MBR->partitions[0].p_type);
        DEBUG("Starting normal MBR Enumeration\n");
        rc = nk_mbr_enumeration(blockdev, MBR, blk_dev_chars);
    }
    free(MBR);
    return rc;
}


int nk_partition_init(struct naut_info *naut)
{
    INFO("init\n");
    return 1;
}

void nk_partition_deinit()
{
    INFO("deinit\n");
}


static int
handle_part (char * buf, void * priv)
{
    char blkdevName[DEV_NAME_LEN];

    memset(blkdevName, 0, DEV_NAME_LEN);

    if (sscanf(buf, "enumparts %s", &blkdevName) != 1) {
      nk_vc_printf("Don't understand %s\n",buf);
      return -1;
    }
 
    struct nk_block_dev *dev = nk_block_dev_find(blkdevName);
    if (!dev) { 
	  //ERROR("Cannot find block device %s\n", blkdevName);
	  nk_vc_printf("Cannot find block device %s\n", blkdevName);
	  return -1;
    }
   
    if (nk_enumerate_partitions(dev)) {
      //ERROR("Failed to enumerate partitions for block dev %s\n", blkdevName);
      nk_vc_printf("Failed to enumerate partitions for block dev %s\n", blkdevName);
      return -1;
    }
    nk_vc_printf("successfully enumerated partitions for block dev %s\n", blkdevName);
    return 0; 
}

static struct shell_cmd_impl part_impl = {
    .cmd      = "enumparts",
    .help_str = "enumparts [blkdevName]",
    .handler  = handle_part,
};
nk_register_shell_cmd(part_impl);
