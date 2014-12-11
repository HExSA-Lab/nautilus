#ifndef __KBD_H__
#define __KBD_H__

#define KBD_DATA_REG 0x60
#define KBD_ACK_REG  0x61
#define KBD_CMD_REG  0x64

#define KBD_LED_CODE 0xed
#define KBD_RELEASE  0x80

#define KEY_SPECIAL_FLAG 0x0100

#define SPECIAL_KEY(x) (KEY_SPECIAL_FLAG | (x))
#define KEY_CTRL_C   0x2e03

#define KEY_LCTRL    SPECIAL_KEY(13)
#define KEY_RCTRL    SPECIAL_KEY(14)
#define KEY_LSHIFT   SPECIAL_KEY(15)
#define KEY_RSHIFT   SPECIAL_KEY(16)

#define KBD_BIT_KDATA 0 /* keyboard data in buffer */
#define KBD_BIT_UDATA 1 /* user data in buffer */
#define KBD_RESET     0xfe

#define STATUS_OUTPUT_FULL 0x1

struct naut_info;

int kbd_init(struct naut_info * naut);


#endif
