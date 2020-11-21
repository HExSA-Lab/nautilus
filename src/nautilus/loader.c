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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/nautilus_exe.h>
#include <nautilus/loader.h>
#include <nautilus/fs.h>
#include <nautilus/shell.h>

#ifndef NAUT_CONFIG_DEBUG_LOADER
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("loader: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("loader: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("loader: " fmt, ##args)

struct nk_exec {
    void      *blob;          // where we loaded it
    uint64_t   blob_size;     // extent in memory
    uint64_t   entry_offset;  // where to start executing in it
};


/******************************************************************
     Data contained in the ELF file we will attempt to load
******************************************************************/

#define ELF_MAGIC    0x464c457f
#define MB2_MAGIC    0xe85250d6

// EXE has one of these headers for sure
typedef struct mb_header {
    uint32_t magic;
    uint32_t arch; 
#define ARCH_X86 0
    uint32_t headerlen;
    uint32_t checksum;
} __attribute__((packed)) mb_header_t;

// generic tagged type
typedef struct mb_tag {
    uint16_t type;
    uint16_t flags;
    uint32_t size;
} __attribute__((packed)) mb_tag_t;

#define MB_TAG_INFO    1
typedef struct mb_info_req {
    mb_tag_t tag;
    uint32_t types[0];
} __attribute__((packed)) mb_info_t;


typedef uint32_t u_virt, u_phys;

#define MB_TAG_ADDRESS 2
typedef struct mb_addr {
    mb_tag_t tag;
    u_virt   header_addr;
    u_virt   load_addr;
    u_virt   load_end_addr;
    u_virt   bss_end_addr;
} __attribute__((packed)) mb_addr_t;

#define MB_TAG_ENTRY 3
typedef struct mb_entry {
    mb_tag_t tag;
    u_virt   entry_addr;
} __attribute__((packed)) mb_entry_t;

#define MB_TAG_FLAGS 4
typedef struct mb_flags {
    mb_tag_t tag;
    uint32_t console_flags;
} __attribute__((packed)) mb_flags_t;

#define MB_TAG_FRAMEBUF 5
typedef struct mb_framebuf {
    mb_tag_t tag;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} __attribute__((packed)) mb_framebuf_t;

#define MB_TAG_MODALIGN 6
typedef struct mb_modalign {
    mb_tag_t tag;
    uint32_t size;
} __attribute__((packed)) mb_modalign_t;


// For HVM, which can use a pure 64 bit variant
// version of multiboot.  The existence of
// this tag indicates that this special mode is
// requested
#define MB_TAG_MB64_HRT           0xf00d
typedef struct mb_mb64_hrt {
    mb_tag_t       tag;
    uint64_t       hrt_flags;
    // whether this kernel is relocable
#define MB_TAG_MB64_HRT_FLAG_RELOC      0x1
    // whether this is an executable
#define MB_TAG_MB64_HRT_FLAG_EXE        0x2
    // How to map the memory in the initial PTs
    // highest set bit wins
#define MB_TAG_MB64_HRT_FLAG_MAP_4KB    0x100
#define MB_TAG_MB64_HRT_FLAG_MAP_2MB    0x200
#define MB_TAG_MB64_HRT_FLAG_MAP_1GB    0x400
#define MB_TAG_MB64_HRT_FLAG_MAP_512GB  0x800

    // How much physical address space to map in the
    // initial page tables (bytes)
    // 
    uint64_t       max_mem_to_map;
    // offset of the GVA->GPA mappings (GVA of GPA 0)
    uint64_t       gva_offset;
    // 64 bit entry address (=0 to use entry tag (which will be offset by gva_offset))
    uint64_t       gva_entry;
    // desired address of the page the VMM, HRT, and ROS share
    // for communication.  "page" here a 4 KB quantity
    uint64_t       comm_page_gpa;
    // desired interrupt vector that should be used for upcalls
    // the default for this is 255
    uint8_t        hrt_int_vector;
    uint8_t        reserved[7];
    
} __attribute__((packed)) mb_mb64_hrt_t;


typedef struct mb_data {
    mb_header_t   *header;
    mb_info_t     *info;
    mb_addr_t     *addr;
    mb_entry_t    *entry;
    mb_flags_t    *flags;
    mb_framebuf_t *framebuf;
    mb_modalign_t *modalign;
    mb_mb64_hrt_t *mb64_hrt;
} mb_data_t;


static int 
is_elf (uint8_t *data, uint64_t size)
{
    if (*((uint32_t*)data)==ELF_MAGIC) {
        return 1;
    } else { 
        return 0;
    }
}


static mb_header_t *
find_mb_header (uint8_t *data, uint64_t size)
{
    uint64_t limit = size > 32768 ? 32768 : size;
    uint64_t i;

    DEBUG("scanning for mb header at %p (%lu bytes)\n",data,size);
    // Scan for the .boot magic cookie
    // must be in first 32K, assume 4 byte aligned
    for (i=0;i<limit;i+=4) { 
        if (*((uint32_t*)&data[i])==MB2_MAGIC) {
            DEBUG("Found multiboot header at offset 0x%llx\n",i);
            return (mb_header_t *) &data[i];
        } 
    }

    return 0;
}


static int 
checksum4_ok (void *ptr, uint64_t size)
{
    int i;
    uint32_t sum=0;
    uint32_t *data=(uint32_t*)ptr;

    for (i=0;i<size;i++) {
        sum+=data[i];
    }

    return sum==0;
}


static int 
parse_multiboot_header (void *data, uint64_t size, mb_data_t *mb)
{
    uint64_t i;
    
    mb_header_t *mb_header=0;
    mb_tag_t *mb_tag=0;
    mb_info_t *mb_inf=0;
    mb_addr_t *mb_addr=0;
    mb_entry_t *mb_entry=0;
    mb_flags_t *mb_flags=0;
    mb_framebuf_t *mb_framebuf=0;
    mb_modalign_t *mb_modalign=0;
    mb_mb64_hrt_t *mb_mb64_hrt=0;

    if (!is_elf(data,size)) { 
        ERROR("HRT is not an ELF\n");
        return -1;
    }

    mb_header = find_mb_header(data,size);

    if (!mb_header) { 
        ERROR("No multiboot header found\n");
        return -1;
    }

    // Checksum applies only to the header itself, not to 
    // the subsequent tags... 
    if (!checksum4_ok(mb_header, 4)) { 
        ERROR("Multiboot header has bad checksum\n");
        return -1;
    }

    DEBUG("Multiboot header: arch=0x%x, headerlen=0x%x\n", mb_header->arch, mb_header->headerlen);

    mb_tag = (mb_tag_t*)((void*)mb_header+16);

    while (!(mb_tag->type==0 && mb_tag->size==8)) {
	DEBUG("tag: type 0x%x flags=0x%x size=0x%x\n",mb_tag->type, mb_tag->flags,mb_tag->size);
	switch (mb_tag->type) {
	    case MB_TAG_INFO: {
		if (mb_inf) { 
		    ERROR("Multiple info tags found!\n");
		    return -1;
		}
		mb_inf = (mb_info_t*)mb_tag;
		DEBUG(" info request - types follow\n");
		for (i=0;(mb_tag->size-8)/4;i++) {
		    DEBUG("  %llu: type 0x%x\n", i, mb_inf->types[i]);
		}
	    }
		break;

	    case MB_TAG_ADDRESS: {
		if (mb_addr) { 
		    ERROR("Multiple address tags found!\n");
		    return -1;
		}
		mb_addr = (mb_addr_t*)mb_tag;
		DEBUG(" address\n");
		DEBUG("  header_addr     =  0x%x\n", mb_addr->header_addr);
		DEBUG("  load_addr       =  0x%x\n", mb_addr->load_addr);
		DEBUG("  load_end_addr   =  0x%x\n", mb_addr->load_end_addr);
		DEBUG("  bss_end_addr    =  0x%x\n", mb_addr->bss_end_addr);
	    }
		break;

	    case MB_TAG_ENTRY: {
		if (mb_entry) { 
		    ERROR("Multiple entry tags found!\n");
		    return -1;
		}
		mb_entry=(mb_entry_t*)mb_tag;
		DEBUG(" entry\n");
		DEBUG("  entry_addr      =  0x%x\n", mb_entry->entry_addr);
	    }
		break;
		
	    case MB_TAG_FLAGS: {
		if (mb_flags) { 
		    ERROR("Multiple flags tags found!\n");
		    return -1;
		}
		mb_flags = (mb_flags_t*)mb_tag;
		DEBUG(" flags\n");
		DEBUG("  console_flags   =  0x%x\n", mb_flags->console_flags);
	    }
		break;
		
	    case MB_TAG_FRAMEBUF: {
		if (mb_framebuf) { 
		    ERROR("Multiple framebuf tags found!\n");
		    return -1;
		}
		mb_framebuf = (mb_framebuf_t*)mb_tag;
		DEBUG(" framebuf\n");
		DEBUG("  width           =  0x%x\n", mb_framebuf->width);
		DEBUG("  height          =  0x%x\n", mb_framebuf->height);
		DEBUG("  depth           =  0x%x\n", mb_framebuf->depth);
	    }
		break;

	    case MB_TAG_MODALIGN: {
		if (mb_modalign) { 
		    ERROR("Multiple modalign tags found!\n");
		    return -1;
		}
		mb_modalign = (mb_modalign_t*)mb_tag;
		DEBUG(" modalign\n");
		DEBUG("  size            =  0x%x\n", mb_modalign->size);
	    }
		break;

	    case MB_TAG_MB64_HRT: {
		if (mb_mb64_hrt) { 
		    ERROR("Multiple mb64_hrt tags found!\n");
		    return -1;
		}
		mb_mb64_hrt = (mb_mb64_hrt_t*)mb_tag;
		DEBUG(" mb64_hrt\n");
	    }
		break;

	    default: 
		DEBUG("Unknown tag... Skipping...\n");
		break;
	}
	mb_tag = (mb_tag_t *)(((void*)mb_tag) + mb_tag->size);
    }

    // copy out to caller
    mb->header=mb_header;
    mb->info=mb_inf;
    mb->addr=mb_addr;
    mb->entry=mb_entry;
    mb->flags=mb_flags;
    mb->framebuf=mb_framebuf;
    mb->modalign=mb_modalign;
    mb->mb64_hrt=mb_mb64_hrt;

    return 0;
}


#define MB_LOAD (2*PAGE_SIZE_4KB)

// load executable from file, do not run
struct nk_exec *nk_load_exec(char *path)
{
    nk_fs_fd_t fd=FS_BAD_FD;
    void *page = 0;
    struct nk_exec *e = 0;
     
    DEBUG("Loading executable at path %s\n", path);

    if (!(page = malloc(MB_LOAD))) { 
        ERROR("Failed to allocate temporary space for loading file %s\n",path);
        goto out_bad;
    }

    memset(page,0,MB_LOAD);
    
    if (FS_FD_ERR(fd = nk_fs_open(path,O_RDONLY,0666))) { 
        ERROR("Executable file %s could not be opened\n", path);
        goto out_bad;
    }

    if (nk_fs_read(fd,page,MB_LOAD)!=MB_LOAD) { 
        ERROR("Could not read first page of file %s\n", path);
        goto out_bad;
    }

    // the MB header should be in the first 2 pages by construction

    mb_data_t m;

    if (parse_multiboot_header(page, MB_LOAD, &m)) { 
        ERROR("Cannot parse multiboot kernel header from first page of %s\n", path);
        goto out_bad;
    }

    DEBUG("Parsed MB header from %s\n", path);

    if (!m.mb64_hrt) { 
        ERROR("%s is not a MB64 image\n", path);
        goto out_bad;
    }

#define REQUIRED_FLAGS (MB_TAG_MB64_HRT_FLAG_RELOC | MB_TAG_MB64_HRT_FLAG_EXE)

    if ((m.mb64_hrt->hrt_flags & REQUIRED_FLAGS)!=REQUIRED_FLAGS) {
        ERROR("%s's flags (%lx) do not include %lx\n", path, m.mb64_hrt->hrt_flags, REQUIRED_FLAGS);
        goto out_bad;
    }
    
    uint64_t load_start, load_end, bss_end;
    uint64_t blob_size;

    // although these are target addresses, we assume 
    // we can use them as offsets as well.   The next page we load
    // will be the "real" start of the text segment
    load_start = m.addr->load_addr;
    load_end = m.addr->load_end_addr;
    bss_end = m.addr->bss_end_addr;

#define ALIGN_UP(x) (((x) % PAGE_SIZE_4KB) ? PAGE_SIZE_4KB*(1 + (x)/PAGE_SIZE_4KB) : (x))

    blob_size = ALIGN_UP(bss_end - load_start + 1);

    DEBUG("Load continuing... start=0x%lx, end=0x%lx, bss_end=0x%lx, blob_size=0x%lx\n",
	  load_start, load_end, bss_end, blob_size);
    

    e = malloc(sizeof(struct nk_exec));
    
    if (!e) { 
        ERROR("Cannot allocate executable exec for %s\n", path);
        goto out_bad;
    }

    memset(e,0,sizeof(*e));

    e->blob = malloc(blob_size);

    if (!e->blob) { 
        ERROR("Cannot allocate executable blob for %s\n",path);
        goto out_bad;
    }
    
    e->blob_size = blob_size;
    e->entry_offset = m.entry->entry_addr - PAGE_SIZE_4KB; 
    
    // now copy it to memory
    ssize_t n;
    
    
    if ((n = nk_fs_read(fd,e->blob,e->blob_size))<0) {
        ERROR("Unable to read blob from %s\n", path);
        goto out_bad;
    }

    DEBUG("Tried to read 0x%lx byte blob, got 0x%lx bytes\n", e->blob_size, n);
    
    DEBUG("Successfully loaded executable %s\n",path);

    memset(e->blob+(load_end-load_start),0,bss_end-load_end);

    DEBUG("Cleared BSS\n");

    nk_fs_close(fd);
    DEBUG("file closed\n");
    free(page);

    return e;
	
 out_bad:
    if (!FS_FD_ERR(fd)) { nk_fs_close(fd); }
    if (page) { free(page); }
    if (e && e->blob) { free(e->blob); }
    if (e) { free(e); }

    return 0;
}

// run executable's entry point - this is a blocking call on the current thread
// user I/O is via the current VC

static void * (*__nk_func_table[])() = {
    [NK_VC_PRINTF] = (void * (*)()) nk_vc_printf,
};


int 
nk_start_exec (struct nk_exec *exec, void *in, void **out)
{
    int (*start)(void *, void **,void * (**)());

    if (!exec) { 
        ERROR("Exec of null\n");
        return -1;
    }

    if (!exec->blob) { 
        ERROR("Exec of null blob\n");
        return -1;
    }

    if (exec->entry_offset > exec->blob_size) { 
        ERROR("Exec attempt beyond end of blob\n");
        return -1;
    }

    start = exec->blob + exec->entry_offset;

    DEBUG("Starting executable %p loaded at address %p with entry address %p and arguments %p and %p\n", exec, exec->blob, start, in, out);

    int rc =  start(in, out, __nk_func_table);

    DEBUG("Executable %p has returned with rc=%d and *out=%p\n", exec, rc, out ? *out : 0);
    
    return rc;
}


int 
nk_unload_exec (struct nk_exec *exec)
{
    if (exec && exec->blob) {
        free(exec->blob);
    }
    if (exec) { 
        free(exec);
    }
    return 0;
}


// these don't do much now
int  
nk_loader_init( )
{
    DEBUG("init\n");
    return 0;
}

void 
nk_loader_deinit ()
{
    DEBUG("deinit\n");
}


static int
handle_run (char * buf, void * priv)
{
    char path[80];

    if (sscanf(buf,"run %s", path)!=1) { 
        nk_vc_printf("Can't determine what to run\n");
        return 0;
    }

    struct nk_exec *e = nk_load_exec(path);

    if (!e) { 
        nk_vc_printf("Can't load %s\n", path);
        return 0;
    }

    nk_vc_printf("Loaded executable, now running\n");

    if (nk_start_exec(e,0,0)) { 
        nk_vc_printf("Failed to run %s\n", path);
    }

    nk_vc_printf("Unloading executable\n");

    if (nk_unload_exec(e)) { 
        nk_vc_printf("Failed to unload %s\n",path);
    }

    return 0;
}    

static struct shell_cmd_impl run_impl = {
    .cmd      = "run",
    .help_str = "run path",
    .handler  = handle_run,
};
nk_register_shell_cmd(run_impl);
