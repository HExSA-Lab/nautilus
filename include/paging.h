#ifndef __PAGING_H__
#define __PAGING_H__

#define PAGE_SHIFT     12U
#define PAGE_SHIFT_4KB 12U
#define PAGE_SHIFT_2MB 21U
#define PAGE_SIZE      4096U
#define PAGE_SIZE_4KB  4096U
#define PAGE_SIZE_2MB  2097152U


// s and e must be page-aligned
static inline void
mark_range_reserved (unsigned char * m, 
                     unsigned long s, 
                     unsigned long e)
{
    unsigned long start_page = s>>12;
    unsigned long end_page    = e>>12;
    unsigned long start_byte  = start_page>>3;
    unsigned long end_byte    = end_page>>3;
    unsigned i;

    if (start_byte == end_byte) {
        m[start_byte] |= (((unsigned char)~0) >> (7-(end_page%8))) &
                         (((unsigned char)~0) << (start_page%8));
    } else {

        m[start_byte] |= ((unsigned char)~0) >> (start_page%8);
        m[end_byte]   |= ((unsigned char)~0) << (7-(end_page%8));

        for (i = start_byte + 1; i < end_byte; i++) {
            m[i] |= 0xFF;
        }
    }
}


void init_page_alloc(unsigned long mbd);

#endif
