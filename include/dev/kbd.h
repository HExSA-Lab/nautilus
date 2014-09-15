#ifndef __KBD_H__
#define __KBD_H__

#define KBD_CMD_REG  0x64
#define KBD_DATA_REG 0x60

#define STATUS_OUTPUT_FULL 0x1

struct naut_info;

int kbd_init(struct naut_info * naut);


#endif
