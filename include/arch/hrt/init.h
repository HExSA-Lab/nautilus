#ifndef __INIT_H__
#define __INIT_H__

void hrt_bsp_init (unsigned long mbd,
                   unsigned long magic,
                   unsigned long mycpuid);

void hrt_ap_init (unsigned long mbd, unsigned long mycpuid);

void default_init (unsigned long mbd,
                   unsigned long magic,
                   unsigned long mycpuid);

#endif
