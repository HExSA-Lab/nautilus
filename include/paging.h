#ifndef __PAGING_H__
#define __PAGING_H__

#include <types.h>

#define PAGE_SHIFT     12U
#define PAGE_SHIFT_4KB 12U
#define PAGE_SHIFT_2MB 21U
#define PAGE_SIZE      4096U
#define PAGE_SIZE_4KB  4096U
#define PAGE_SIZE_2MB  2097152U


// s and e must be page-aligned
static inline void
mark_range_reserved (uchar_t * m, 
                     ulong_t s, 
                     ulong_t e)
{
    ulong_t start_page = s>>12;
    ulong_t end_page    = e>>12;
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
            m[i] |= 0xFF;
        }
    }
}


void init_page_frame_alloc(ulong_t mbd);

#endif
