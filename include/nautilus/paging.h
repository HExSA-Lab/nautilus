#ifndef __PAGING_H__
#define __PAGING_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>
#include <nautilus/idt.h>
#include <nautilus/printk.h>
    
typedef ulong_t pml4e_t;
typedef ulong_t pdpte_t;
typedef ulong_t pde_t;
typedef ulong_t pte_t;


struct nk_pf_error {
    union {
        uint32_t val;
        struct {
            uint8_t p       : 1;
            uint8_t wr      : 1;
            uint8_t usr     : 1;
            uint8_t rsvd_bit: 1;
            uint8_t ifetch  : 1;
            uint32_t rsvd   : 27;
        };
    };
} __packed;


typedef struct nk_pf_error nk_pf_err_t;

struct nk_mem_info {
    ulong_t * page_map;
    addr_t    pm_start;
    addr_t    pm_end;
    ulong_t   phys_mem_avail;
    ulong_t   npages;
};


int nk_create_page_mapping (addr_t vaddr, addr_t paddr, uint64_t flags);
int nk_map_page_nocache (addr_t paddr, uint64_t flags);
void nk_paging_init(struct nk_mem_info * mem, ulong_t mbd);
addr_t nk_alloc_page(void);
int nk_free_page(addr_t addr);
int nk_reserve_pages(addr_t paddr, unsigned n);
int nk_reserve_page(addr_t paddr);
int nk_reserve_range(addr_t start, addr_t end);

/* hooks */
int nk_free_pages(void * addr, unsigned num);
addr_t nk_alloc_pages(unsigned num);

int nk_pf_handler(excp_entry_t * excp, excp_vec_t vector, addr_t fault_addr);

#define PAGE_SHIFT_4KB 12UL
#define PAGE_SHIFT_2MB 21UL
#define PAGE_SHIFT_1GB 30UL
#define PAGE_SHIFT     PAGE_SHIFT_2MB
#define PAGE_SIZE_4KB  4096UL
#define PAGE_SIZE_2MB  2097152UL
#define PAGE_SIZE_1GB  1073741824UL
#define PAGE_SIZE      PAGE_SIZE_2MB

#define MEM_1GB        0x40000000ULL
#define MEM_2MB        0x200000ULL

#define NUM_PT_ENTRIES    512
#define NUM_PD_ENTRIES    512
#define NUM_PDPT_ENTRIES  512
#define NUM_PML4_ENTRIES  512

#define GEN_PT_MASK(x) ((((1ULL<<((x)+9)))-1) & ((~0ULL) << (x)))

#define PML4_SHIFT 39
#define PML4_MASK  GEN_PT_MASK(PML4_SHIFT)

#define PDPT_SHIFT  30
#define PDPT_MASK   GEN_PT_MASK(PDPT_SHIFT)

#define PD_SHIFT   21
#define PD_MASK    GEN_PT_MASK(PD_SHIFT)

#define PT_SHIFT   12
#define PT_MASK    GEN_PT_MASK(PT_SHIFT)

#define PADDR_TO_PML4_IDX(x)   ((PML4_MASK & (x)) >> PML4_SHIFT)
#define PADDR_TO_PDPT_IDX(x) ((PDPT_MASK & (x)) >> PDPT_SHIFT)
#define PADDR_TO_PD_IDX(x)   ((PD_MASK & (x))   >> PD_SHIFT)
#define PADDR_TO_PT_IDX(x)   ((PT_MASK & (x))   >> PT_SHIFT)

#define PAGE_TO_PADDR(n) ((unsigned long long)(n) << PAGE_SHIFT)
#define PADDR_TO_PAGE(n) ((n) >> PAGE_SHIFT)

#define PTE_ADDR_MASK (~((1<<12)-1))


#define PTE_PRESENT_BIT       1ULL
#define PTE_WRITABLE_BIT      2ULL
#define PTE_KERNEL_ONLY_BIT   (1ULL<<2)
#define PTE_WRITE_THROUGH_BIT (1ULL<<3)
#define PTE_CACHE_DISABLE_BIT (1ULL<<4)
#define PTE_ACCESSED_BIT      (1ULL<<5)
#define PTE_DIRTY_BIT         (1ULL<<6)
#define PTE_PAGE_SIZE_BIT     (1ULL<<7)
#define PTE_GLOBAL_BIT        (1ULL<<8)
#define PTE_PAT_BIT           (1ULL<<12)
#define PTE_NX_BIT            (1ULL<<63)

#define PML4E_PRESENT(x) ((x) & 1)
#define PML4E_WRITABLE(x) ((x) & 2)
#define PML4E_KERNEL_ONLY(x) ((x) & (1<<3))
#define PML4E_WRITE_THROUGH(x) ((x) & (1<<4))

#define PDPTE_PRESENT(x) ((x) & 1)
#define PDPTE_WRITABLE(x) ((x) & 2)
#define PDPTE_KERNEL_ONLY(x) ((x) & (1<<3))
#define PDPTE_WRITE_THROUGH(x) ((x) & (1<<4))

#define PDE_PRESENT(x) ((x) & 1)
#define PDE_WRITABLE(x) ((x) & 2)
#define PDE_KERNEL_ONLY(x) ((x) & (1<<3))
#define PDE_WRITE_THROUGH(x) ((x) & (1<<4))

#define PTE_PRESENT(x) ((x) & 1)
#define PTE_WRITABLE(x) ((x) & 2)
#define PTE_KERNEL_ONLY(x) ((x) & (1<<3))
#define PTE_WRITE_THROUGH(x) ((x) & (1<<4))
#define PTE_CACHE_DISABLED(x) ((x) & (1<<5))




#define PF_ERR_PROT(x)         ((x)&1)       /* fault caused by page-level prot. violation */
#define PF_ERR_NOT_PRESENT(x)  (~(x)&1)      /* fault caused by not-present page */
#define PF_ERR_READ            (~(x)&2)      /* fault was a read */
#define PF_ERR_WRITE           ((x)&2)       /* fault was a write */
#define PF_ERR_SUPER           (~(x)&(1<<3)) /* fault was in supervisor mode */
#define PF_ERR_USR             ((x)&(1<<3))  /* fault was in user mode */
#define PF_ERR_NO_RSVD         (~(x)&(1<<4)) /* fault not caused by rsvd bit violation */
#define PF_ERR_RSVD_BIT        ((x)&(1<<4)   /* fault caused by reserved bit set in page directory */
#define PF_ERR_NO_IFETCH       (~(x)&(1<<5)) /* fault not caused by ifetch */
#define PF_ERR_IFETCH          ((x)&(1<<5))  /* fault caused by ifetch */


#define __page_align __attribute__ ((aligned(PAGE_SIZE)))

// given a page num, what's it byte offset in the page map
#define PAGE_MAP_OFFSET(n)   (n >> 3)

// given a page num, what's the bit number within the byte
#define PAGE_MAP_BIT_IDX(n)  (n % 8)



#ifdef __cplusplus
}
#endif

#endif
