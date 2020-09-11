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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The Interweaving Project  <http://interweaving.org>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __GPU_DEV
#define __GPU_DEV

#include <nautilus/dev.h>


/*
  Don't get too excited by the use of the term "GPU".   

  This is currently simply an abstraction for 2D accelerated graphics
  cards.   Video modes are either 2D graphics or text

  text: all are basic VGA 1 byte char, 1 byte attr

  graphics: all are 4 bytes, 1 byte per channel, 
            rgb + optional alpha, selective order

 */


typedef struct nk_gpu_dev_video_mode {
    enum {NK_GPU_DEV_MODE_TYPE_TEXT, NK_GPU_DEV_MODE_TYPE_GRAPHICS_2D } type;
    uint32_t width;                  // width and height of screen (pixels or
    uint32_t height;

    uint8_t  channel_offset[4];      // for graphics, channel offsets into a pixel (-1 means channel does not exist)
#define NK_GPU_DEV_CHANNEL_OFFSET_RED   0   // for text, channel 0 is the character, channel 1 is the attribute
#define NK_GPU_DEV_CHANNEL_OFFSET_GREEN 1
#define NK_GPU_DEV_CHANNEL_OFFSET_BLUE  2
#define NK_GPU_DEV_CHANNEL_OFFSET_ALPHA 3
#define NK_GPU_DEV_CHANNEL_OFFSET_TEXT  0
#define NK_GPU_DEV_CHANNEL_OFFSET_ATTR  1

    uint64_t flags;
#define NK_GPU_DEV_HAS_CLIPPING         0x1   // does the mode support clipping by box?
#define NK_GPU_DEV_HAS_CLIPPING_REGION  0x2   // does the mode support clipping by arbitrary region?
#define NK_GPU_DEV_HAS_MOUSE_CURSOR     0x100 // does the mode allow setting a mouse cursor?
    

    uint32_t mouse_cursor_width;     // size of mouse cursor, if supported
    uint32_t mouse_cursor_height;    // bitmap format is assumed to be the same as the mode's format

    void *mode_data;                 // opaque pointer to any info driver wants to associate with it
                                     // should be static data, caller will not delete this
} nk_gpu_dev_video_mode_t;


// coordinate (0,0) is top left and (width-1,height-1) is bottom right
typedef struct nk_gpu_dev_coordinate {
    uint32_t x;
    uint32_t y;
} nk_gpu_dev_coordinate_t;


// bounding box, clipping, etc
// coordinate system as with nk_gpu_dev_coordinate_t
typedef struct nk_gpu_dev_box {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} nk_gpu_dev_box_t;

typedef struct nk_gpu_dev_region {
    // CURRENTLY NOT IMPLEMENTED
    // A region is an arbitrary clump of binary pixels
    // The pixels that are on are part of the region
    // drawing can be clipped to a region
    // a possible efficient region implementation is a quadtree
} nk_gpu_dev_region_t;

typedef union nk_gpu_dev_char {
    uint16_t raw;
    struct {
	uint8_t symbol;      // Assume classic PC north american code page  (ASCII+Europe+graphics chars)
	uint8_t attribute;   // standard VGA attributes
    };
} nk_gpu_dev_char_t;

typedef union nk_gpu_dev_pixel {
    uint32_t raw;
    // the channel offsets are given in a mode
    uint8_t  channel[4];
} nk_gpu_dev_pixel_t;


// 32 bit full color bitmap, same as screen
typedef struct nk_gpu_dev_bitmap {
    uint32_t width;
    uint32_t height;
    nk_gpu_dev_pixel_t pixels[0];  // row major order, no interleaving
} nk_gpu_dev_bitmap_t;


typedef enum nk_gpu_dev_bit_blit_op {
				     NK_GPU_DEV_BIT_BLIT_OP_COPY=0,
				     NK_GPU_DEV_BIT_BLIT_OP_NOT,
				     NK_GPU_DEV_BIT_BLIT_OP_AND,
 				     NK_GPU_DEV_BIT_BLIT_OP_OR,
 				     NK_GPU_DEV_BIT_BLIT_OP_NAND,
 				     NK_GPU_DEV_BIT_BLIT_OP_NOR,
 				     NK_GPU_DEV_BIT_BLIT_OP_XOR,
 				     NK_GPU_DEV_BIT_BLIT_OP_XNOR,
 				     NK_GPU_DEV_BIT_BLIT_OP_PLUS, // arithmetic bit blit ops should use saturating arithmetic
 				     NK_GPU_DEV_BIT_BLIT_OP_MINUS,
 				     NK_GPU_DEV_BIT_BLIT_OP_MULTIPLY,
 				     NK_GPU_DEV_BIT_BLIT_OP_DIVIDE,
} nk_gpu_dev_bit_blit_op_t;

// simple bitmap fonts (256 symbols, assume ASCII)
// binary bitmap. 0=> empty, 1=>filled
typedef struct nk_gpu_dev_font {
    uint32_t width;     // size of character in pixels
    uint32_t height;
    uint8_t  data[0];   // contains 256 character bitmaps, row major order
} nk_gpu_dev_font_t;


struct nk_gpu_dev_int {
    // this must be first so it derives cleanly
    // from nk_dev_int
    struct nk_dev_int dev_int;
    
    // gpudev-specific interface - set to zero if not available
    // an interface either succeeds (returns zero) or fails (returns -1)

    // discover the modes supported by the device
    //     modes = array of modes on entry, filled out on return
    //     num = size of array on entry, number of modes found on return
    // 
    int (*get_available_modes)(void *state, nk_gpu_dev_video_mode_t modes[], uint32_t *num);

    // grab the current mode - useful in case you need to reset it later
    int (*get_mode)(void *state, nk_gpu_dev_video_mode_t *mode);
    
    // set a video mode based on the modes discovered
    // this will switch to the mode before returning
    int (*set_mode)(void *state, nk_gpu_dev_video_mode_t *mode);

    // drawing commands
    
    // each of these is asynchronous - the implementation should start the operation
    // but not necessarily finish it.   In particular, nothing needs to be drawn
    // until flush is invoked

    // flush - wait until all preceding drawing commands are visible by the user
    int (*flush)(void *state);

    // text mode drawing commands
    int (*text_set_char)(void *state, nk_gpu_dev_coordinate_t *location, nk_gpu_dev_char_t *val);
    // cursor location in text mode
    int (*text_set_cursor)(void *state, nk_gpu_dev_coordinate_t *location, uint32_t flags);
#define NK_GPU_DEV_TEXT_CURSOR_ON    1
#define NK_GPU_DEV_TEXT_CURSOR_BLINK 2

    // graphics mode drawing commands
    // confine drawing to this box or region
    int (*graphics_set_clipping_box)(void *state, nk_gpu_dev_box_t *box);
    int (*graphics_set_clipping_region)(void *state, nk_gpu_dev_region_t *region);

    // draw stuff 
    int (*graphics_draw_pixel)(void *state, nk_gpu_dev_coordinate_t *location, nk_gpu_dev_pixel_t *pixel);
    int (*graphics_draw_line)(void *state, nk_gpu_dev_coordinate_t *start, nk_gpu_dev_coordinate_t *end, nk_gpu_dev_pixel_t *pixel);
    int (*graphics_draw_poly)(void *state, nk_gpu_dev_coordinate_t *coord_list, uint32_t count, nk_gpu_dev_pixel_t *pixel);
    int (*graphics_fill_box_with_pixel)(void *state, nk_gpu_dev_box_t *box, nk_gpu_dev_pixel_t *pixel, nk_gpu_dev_bit_blit_op_t op);
    int (*graphics_fill_box_with_bitmap)(void *state, nk_gpu_dev_box_t *box, nk_gpu_dev_bitmap_t *bitmap, nk_gpu_dev_bit_blit_op_t op);
    int (*graphics_copy_box)(void *state, nk_gpu_dev_box_t *source_box, nk_gpu_dev_box_t *dest_box, nk_gpu_dev_bit_blit_op_t op);
    int (*graphics_draw_text)(void *state, nk_gpu_dev_coordinate_t *location, nk_gpu_dev_font_t *font, char *string);

    //  mouse functions, if supported
    int (*graphics_set_cursor_bitmap)(void *state, nk_gpu_dev_bitmap_t *bitmap);
    // the location is the position of the top-left pixel in the bitmap
    int (*graphics_set_cursor)(void *state, nk_gpu_dev_coordinate_t *location);
    
};


struct nk_gpu_dev {
    // must be first member 
    struct nk_dev dev;
};


typedef struct nk_gpu_dev nk_gpu_dev_t;

//
// device registry - called by device drivers
//
//
struct nk_gpu_dev * nk_gpu_dev_register(char *name, uint64_t flags, struct nk_gpu_dev_int *inter, void *state); 
int                 nk_gpu_dev_unregister(struct nk_gpu_dev *);



//
// Functions below are called by users of this abstraction
//

// device find by name
struct nk_gpu_dev * nk_gpu_dev_find(char *name);

// tell me what modes the device has 
int nk_gpu_dev_get_available_modes(nk_gpu_dev_t *dev, nk_gpu_dev_video_mode_t modes[], uint32_t *num);

// give me a snapshot of the current mode (overwrites mode argument)
int nk_gpu_dev_get_mode(nk_gpu_dev_t *dev, nk_gpu_dev_video_mode_t *mode);
    

// switch to a specific mode, ideally one returned grom get_available_modes
int nk_gpu_dev_set_mode(nk_gpu_dev_t *dev, nk_gpu_dev_video_mode_t *mode);


// flush output to the user.   drawing commands are allowed to be buffered by the device.
// it is not until you call this function that you know your output is on the screen
int nk_gpu_dev_flush(nk_gpu_dev_t *dev);

// output a character in a text mode
int nk_gpu_dev_text_set_char(nk_gpu_dev_t *dev, nk_gpu_dev_coordinate_t *location, nk_gpu_dev_char_t *val);
// locate and configure the cursor in a text mode
// you probably want to use the "on" flag
int nk_gpu_dev_text_set_cursor(nk_gpu_dev_t *dev, nk_gpu_dev_coordinate_t *location, uint32_t flags);

// graphics mode drawing commands

// confine drawing to this subregion
// this requires that clipping be supported by the driver
int nk_gpu_dev_graphics_set_clipping_box(nk_gpu_dev_t *dev, nk_gpu_dev_box_t *box);
int nk_gpu_dev_graphics_set_clipping_region(nk_gpu_dev_t *dev, nk_gpu_dev_region_t *region);

// draw stuff
//   note that fills and copies are bit blits
int nk_gpu_dev_graphics_draw_pixel(nk_gpu_dev_t *dev, nk_gpu_dev_coordinate_t *location, nk_gpu_dev_pixel_t *pixel);
int nk_gpu_dev_graphics_draw_line(nk_gpu_dev_t *dev, nk_gpu_dev_coordinate_t *start, nk_gpu_dev_coordinate_t *end, nk_gpu_dev_pixel_t *pixel);
int nk_gpu_dev_graphics_draw_poly(nk_gpu_dev_t *dev, nk_gpu_dev_coordinate_t *coord_list, uint32_t count, nk_gpu_dev_pixel_t *pixel);
int nk_gpu_dev_graphics_fill_box_with_pixel(nk_gpu_dev_t *dev, nk_gpu_dev_box_t *box, nk_gpu_dev_pixel_t *pixel, nk_gpu_dev_bit_blit_op_t op);
int nk_gpu_dev_graphics_fill_box_with_bitmap(nk_gpu_dev_t *dev, nk_gpu_dev_box_t *box, nk_gpu_dev_bitmap_t *bitmap, nk_gpu_dev_bit_blit_op_t op);
int nk_gpu_dev_graphics_copy_box(nk_gpu_dev_t *dev, nk_gpu_dev_box_t *source_box, nk_gpu_dev_box_t *dest_box, nk_gpu_dev_bit_blit_op_t op);
int nk_gpu_dev_graphics_draw_text(nk_gpu_dev_t *dev, nk_gpu_dev_coordinate_t *location, nk_gpu_dev_font_t *font, char *string);

//  confiure mouse cursor, if supported
int nk_gpu_dev_graphics_set_cursor_bitmap(nk_gpu_dev_t *dev, nk_gpu_dev_bitmap_t *bitmap);
int nk_gpu_dev_graphics_set_cursor(nk_gpu_dev_t *dev, nk_gpu_dev_coordinate_t *location);

// helper tools for drawing

// pixels
#define NK_GPU_DEV_PIXEL_SET_RGBA(m,p,r,g,b,a)				\
    (p)->channel[(m)->channel_offset[NK_GPU_DEV_CHANNEL_OFFSET_RED]]=(r); \
    (p)->channel[(m)->channel_offset[NK_GPU_DEV_CHANNEL_OFFSET_GREEN]]=(g); \
    (p)->channel[(m)->channel_offset[NK_GPU_DEV_CHANNEL_OFFSET_BLUE]]=(b); \
    (p)->channel[(m)->channel_offset[NK_GPU_DEV_CHANNEL_OFFSET_ALPHA]]=(a);

// chars
#define NK_GPU_DEV_CHAR_SET_SYM_ATTR(d,c,s,a) c->symbol = s; c->attribute = a
    
// bitmaps
nk_gpu_dev_bitmap_t *nk_gpu_dev_bitmap_create(uint32_t width, uint32_t height);
// for reading/writing pixels in the bitmap
#define              NK_GPU_DEV_BITMAP_PIXEL(bitmap,x,y) ((bitmap)->pixels[(y)*((bitmap)->width)+(x)])
void                 nk_gpu_dev_bitmap_destroy(nk_gpu_dev_bitmap_t *bitmap);

// fonts
nk_gpu_dev_font_t   *nk_gpu_dev_font_create(uint32_t width, uint32_t height);
#define              NK_GPU_DEV_FONT_BIT_OFFSET(font,ch,x,y) ((ch)*(((font)->width) * ((font)->height)) + ((y)*((font)->width)) + (x))
#define              NK_GPU_DEV_FONT_BIT_GET(font,ch,x,y)    (((font)->data[NK_GPU_DEV_FONT_BIT_OFFSET(font,ch,x,y)/8] >> (NK_GPU_DEV_FONT_BIT_OFFSET(font,ch,x,y)%8)) & 0x1)
#define              NK_GPU_DEV_FONT_BIT_SET(font,ch,x,y)    ((font)->data[NK_GPU_DEV_FONT_BIT_OFFSET(font,ch,x,y)/8]) |= 0x1 << (NK_GPU_DEV_FONT_BIT_OFFSET(font,ch,x,y)%8)
#define              NK_GPU_DEV_FONT_BIT_RESET(font,ch,x,y)  ((font)->data[NK_GPU_DEV_FONT_BIT_OFFSET(font,ch,x,y)/8]) &= ~(0x1 << (NK_GPU_DEV_FONT_BIT_OFFSET(font,ch,x,y)%8))
void                 nk_gpu_dev_font_destroy(nk_gpu_dev_font_t *font);
    



// called on BSP at startup and shutdown, respectively
int nk_gpu_dev_init();
int nk_gpu_dev_deinit();

#endif

