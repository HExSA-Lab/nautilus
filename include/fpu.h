#ifndef __FPU_H__
#define __FPU_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MXCSR_IE 1
#define MXCSR_DE 2
#define MXCSR_ZE (1<<2)
#define MXCSR_OE (1<<3)
#define MXCSR_UE (1<<4)
#define MXCSR_PE (1<<5)
#define MXCSR_DAZ (1<<6)
#define MXCSR_IM (1<<7)
#define MXCSR_DM (1<<8)
#define MXCSR_ZM (1<<9)
#define MXCSR_OM (1<<10)
#define MXCSR_UM (1<<11)
#define MXCSR_PM (1<<12)
#define MXCSR_RC (1<<13)
#define MXCSR_FZ (1<<14)

struct naut_info;

void fpu_init(struct naut_info *);

#ifdef __cplusplus
}
#endif

#endif /* !__FPU_H__! */
