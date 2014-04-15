
#ifndef __MB_UTIL_H__
#define __MB_UTIL_H__

unsigned long long get_phys_mem(unsigned long mbd);
void parse_multiboot(unsigned long mbd, unsigned long magic);

#endif
