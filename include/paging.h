#ifndef __PAGING_H__
#define __PAGING_H__

#define PAGE_SHIFT     12U
#define PAGE_SHIFT_4KB 12U
#define PAGE_SHIFT_2MB 21U
#define PAGE_SIZE      4096U
#define PAGE_SIZE_4KB  4096U
#define PAGE_SIZE_2MB  2097152U

void init_page_alloc(unsigned long mbd);

#endif
