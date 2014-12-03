#ifndef __I8254_H__
#define __I8254_H__

#ifdef __cplusplus
extern "C" {
#endif

struct naut_info;

ulong_t i8254_calib_tsc(void);
int i8254_init(struct naut_info * naut);

#define PIT_TIMER_IRQ 0

/* for configuring the PC speaker ports */
#define KB_CTRL_DATA_OUT 0x60
#define KB_CTRL_PORT_B   0x61

#define PIT_CHAN0_DATA 0x40
#define PIT_CHAN1_DATA 0x41
#define PIT_CHAN2_DATA 0x42
#define PIT_CMD_REG    0x43

/* for cmd reg */

#define PIT_CHAN_SEL_0 0x0
#define PIT_CHAN_SEL_1 0x1
#define PIT_CHAN_SEL_2 0x2

#define PIT_CHAN(x) ((x) << 6)

#define PIT_ACC_MODE_LATCH_CNT 0x0
#define PIT_ACC_MODE_JUST_LO   0x1
#define PIT_ACC_MODE_JUST_HI   0x2
#define PIT_ACC_MODE_BOTH      0x3

#define PIT_ACC_MODE(x) ((x) << 4)

#define PIT_MODE_ONESHOT  0x1 
#define PIT_MODE_RATEGEN  0x2
#define PIT_MODE_SQUARE   0x3
#define PIT_MODE_SWSTROBE 0x4
#define PIT_MODE_HWSTROBE 0x5
#define PIT_MODE_RATEGEN2 0x6
#define PIT_MODE_SQUARE2  0x7

#define PIT_MODE(x) ((x) << 1)

#define PIT_BIT_MODE_BIN 0
#define PIT_BIT_MODE_BCD 1 /* 4-digit BCD */

#define PIT_RATE 1193182UL

#define MS      10
#define LATCH   (PIT_RATE / (1000 / MS))
#define LOOPS   1000

#ifdef __cplusplus
}
#endif

#endif
