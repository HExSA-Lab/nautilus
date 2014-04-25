#ifndef __PAGING_H__
#define __PAGING_H__

#include <types.h>
#include <idt.h>

#define PAGE_SHIFT     12U
#define PAGE_SHIFT_4KB 12U
#define PAGE_SHIFT_2MB 21U
#define PAGE_SIZE      4096U
#define PAGE_SIZE_4KB  4096U
#define PAGE_SIZE_2MB  2097152U

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

#define PAGE_TO_PADDR(n) ((n) << PAGE_SHIFT)
#define PADDR_TO_PAGE(n) ((n) >> PAGE_SHIFT)

#define PTE_ADDR_MASK (~((1<<12)-1))


#define PTE_PRESENT_BIT 1UL
#define PTE_WRITABLE_BIT 2UL
#define PTE_KERNEL_ONLY_BIT (1UL<<3)
#define PTE_WRITE_THROUGH_BIT (1UL<<4)

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


typedef ulong_t pml4e_t;
typedef ulong_t pdpte_t;
typedef ulong_t pde_t;
typedef ulong_t pte_t;


struct pf_error {
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


typedef struct pf_error pf_err_t;

struct mem_info {
    ulong_t * page_map;
    addr_t    pm_start;
    addr_t    pm_end;
    ulong_t   phys_mem_avail;
    ulong_t   npages;
};



// find the first zero bit in a word
/*
static inline uint8_t
ff_zero (ulong_t srch)
{
    asm volatile("rep; bsf %1, %0"
            : "=r" (srch)
            : "r" (~srch));

    return srch;
}

static inline void
set_bit (int idx, volatile ulong_t * addr)
{
    asm volatile("bts %1, %0":
                 "=m" (*(volatile long *) (addr)) :
                 "Ir" (idx) : 
                 "memory");
}


static inline void
unset_bit (int idx, volatile ulong_t * addr)
{
    asm volatile("btr %1, %0" : 
                 "=m" (*(volatile long *) (addr)) :
                 "Ir" (idx));
}
*/


// TODO: optimize this with native instructions
// s and e must be page-aligned
static inline void
mark_range_reserved (uchar_t * m, 
                     ulong_t s, 
                     ulong_t e)
{
    ulong_t start_page  = s>>PAGE_SHIFT;
    ulong_t end_page    = e>>PAGE_SHIFT;
    ulong_t start_byte  = start_page>>3;
    ulong_t end_byte    = end_page>>3;
    uint_t i;

    if (start_byte == end_byte) {
        m[start_byte] |= (((uchar_t)~0) >> (7-(end_page%8))) &
                         (((uchar_t)~0) << (start_page%8));
    } else {

        m[start_byte] |= ((uchar_t)~0) >> (start_page%8);
        m[end_byte]   |= ((uchar_t)~0) << (7-(end_page%8));

        for (i = start_byte + 1; i < end_byte; i++) {
            m[i] |= 0xffu;
        }
    }
}


void init_page_frame_alloc(ulong_t mbd);
addr_t alloc_page(void);
int free_page(addr_t addr);

/* hooks */
int free_pages(void * addr, int num);
addr_t alloc_pages(int num);

int pf_handler(excp_entry_t * excp, excp_vec_t vector, addr_t fault_addr);

#endif
