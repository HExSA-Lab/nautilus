/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Peter Dinda
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __NK_VESA
#define __NK_VESA


/*
  Basic functionality to get/set VESA modes and to
  acquire a linear framebuffer for drawing into for
  the graphics modes
*/

typedef uint16_t vesa_mode_t;
#define VESA_MODE_DONT_CLEAR (1<<15)
#define VESA_MODE_LINEAR_FB_ENABLE (1<<14)
#define VESA_MODE_ACCEL (1<<13)
#define VESA_MODE_USER_CRTC (1<<12)
#define VESA_MODE_MODE_MASK (0x1ff)   // 1xx are VESA-defined, 0xx are vendor

#define VESA_MODE_INFO_SIZE 256

#define VESA_MODE_INFO_WIDTH(info) (*((uint16_t*)(((void*)info)+0x12)))
#define VESA_MODE_INFO_HEIGHT(info) (*((uint16_t*)(((void*)info)+0x14)))
#define VESA_MODE_INFO_BPP(info) (*((uint8_t*)(((void*)info)+0x19)))
#define VESA_MODE_INFO_BPSL(info) (*((uint16_t*)(((void*)info)+0x32)))


#define VESA_MODE_INFO_LINEAR_FB_PHYS_ADDR(info) ((void*)((uint64_t)(*((uint32_t*)(((void*)info)+0x28)))))

#define VESA_MODE_TEXT_80_25    0x003
#define VESA_MODE_TEXT_GENERIC  VESA_MODE_TEXT_80_25
#define VESA_MODE_TEXT_80_60    0x108
#define VESA_MODE_TEXT_132_60   0x10c

#define VESA_MODE_GRAPHICS_640_480_24   0x112
#define VESA_MODE_GRAPHICS_800_600_24   0x115
#define VESA_MODE_GRAPHICS_1024_768_24  0x118
#define VESA_MODE_GRAPHICS_1280_1024_24 0x11b

#define VESA_MODE_GRAPHICS_800_600_16   0x113
#define VESA_MODE_GRAPHICS_1600_1200_16 0x122


// vbe_info and vbe_mode_info as per
// osdev.org / http://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
struct vesa_adapter_info {
    char     signature[4];  // "VBE2" on entry, "VESA" on exit
    uint16_t version;       // VBE version; high byte is major version, low byte is minor version
    uint32_t oem;           // segment:offset pointer to OEM
    uint32_t capabilities;  // bitfield that describes card capabilities
    uint32_t video_modes;   // segment:offset pointer to list of supported video modes
    uint16_t video_memory;  // amount of video memory in 64KB blocks
    uint16_t software_rev;  // software revision
    uint32_t vendor;        // segment:offset to card vendor string
    uint32_t product_name;  // segment:offset to card model name
    uint32_t product_rev;   // segment:offset pointer to product revision
    char reserved[222];	    // reserved for future expansion
    char oem_data[256];	    // OEM BIOSes store their strings in this area
} __packed;

struct vesa_mode_info {
    uint16_t attributes;        // deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
#define VESA_ATTR_HWSUPPORT  0x01
#define VESA_ATTR_TTY        0x04
#define VESA_ATTR_COLOR      0x08
#define VESA_ATTR_GRAPHICS   0x10
#define VESA_ATTR_NOTVGA     0x20
#define VESA_ATTR_NOTWINDOW  0x40
#define VESA_ATTR_LINEARFB   0x80
#define VESA_ATTR_DOUBLESCAN 0x100
#define VESA_ATTR_INTERLACE  0x200
#define VESA_ATTR_TRIPLEBUF  0x400
#define VESA_ATTR_STEREO     0x800
#define VESA_ATTR_DUALDISP   0x1000

    uint8_t window_a;	       // deprecated
    uint8_t window_b;          // deprecated
    uint16_t granularity;      // deprecated; used while calculating bank numbers
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;       // deprecated; used to switch banks from protected mode without returning to real mode
    uint16_t pitch;            // number of bytes per horizontal line
    uint16_t width;            // width in pixels
    uint16_t height;           // height in pixels
    uint8_t w_char;            // unused...
    uint8_t y_char;            // ...
    uint8_t planes;
    uint8_t bpp;               // bits per pixel in this mode
    uint8_t banks;             // deprecated; total number of banks in this mode
    uint8_t memory_model;
    uint8_t bank_size;        // deprecated; size of a bank, almost always 64 KB but may be 16 KB...
    uint8_t image_pages;
    uint8_t reserved0;
    
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;
    
    uint32_t framebuffer;            // physical address of the linear frame buffer; write here to draw to the screen
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;    // size of memory in the framebuffer but not being displayed on the screen
    uint8_t reserved1[206];
} __packed;

struct vesa_mode_request {
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;
    int      text;   // is a text mode
    int      lfb;    // has linear framebuffer
};
    

#define REAL_TO_LINEAR(seg,off) ((void*)((uint64_t)((((uint32_t)(seg))<<4)+((uint32_t)(off)))))
#define VESA_PTR_TO_SEG(x) ((uint16_t)(((x)>>16)&0xffff))
#define VESA_PTR_TO_OFF(x) ((uint16_t)((x)&0xffff))
#define VESA_PTR_TO_LINEAR(x) REAL_TO_LINEAR(VESA_PTR_TO_SEG(x),VESA_PTR_TO_OFF(x))

int  vesa_init();
void vesa_deinit();

void vesa_test();

int vesa_get_adapter_info(struct vesa_adapter_info *info);

int vesa_get_cur_mode(vesa_mode_t *mode);

int vesa_get_mode_info(vesa_mode_t mode, struct vesa_mode_info *info); 

int vesa_find_matching_mode(struct vesa_mode_request *r, vesa_mode_t *mode);

int vesa_set_cur_mode(vesa_mode_t mode);

void vesa_draw_pixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);

#endif
