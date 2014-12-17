#ifndef __LOWLEVEL_H__
#define __LOWLEVEL_H__

#define GEN_NOP(x) .byte x

#define NOP_1BYTE 0x90
#define NOP_2BYTE 0x66,0x90
#define NOP_3BYTE 0x0f,0x1f,0x00
#define NOP_4BYTE 0x0f,0x1f,0x40,0
#define NOP_5BYTE 0x0f,0x1f,0x44,0x00,0
#define NOP_6BYTE 0x66,0x0f,0x1f,0x44,0x00,0
#define NOP_7BYTE 0x0f,0x1f,0x80,0,0,0,0
#define NOP_8BYTE 0x0f,0x1f,0x84,0x00,0,0,0,0

#define ENTRY(x)   \
    .globl x;      \
    .align 4, 0x90;\
    x:

#define GLOBAL(x)  \
    .globl x;      \
    x:

#define END(x) \
    .size x, .-x 

#endif
