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
 * Copyright (c) 2016, Yang Wu, Fei Luo and Yuanhui Yang
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Yang Wu, Fei Luo and Yuanhui Yang
 *          {YangWu2015, FeiLuo2015, YuanhuiYang2015}@u.northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __NK_VC
#define __NK_VC

// Keys and scancodes are represented internally as 16 bits
// so we can indicate nonexistence (via -1)
typedef uint16_t nk_keycode_t;
typedef uint16_t nk_scancode_t;

#define NO_KEY ((nk_keycode_t)(0xffff))
#define NO_SCANCODE ((nk_scancode_t)(0xffff))

enum nk_vc_type {RAW, COOKED, RAW_NOQUEUE};
struct nk_virtual_console;

struct nk_virtual_console *nk_create_vc(enum nk_vc_type new_vc_type,
					void (*raw_noqueue_callback)(nk_scancode_t)); 

int nk_destroy_vc(struct nk_virtual_console *vc);

int nk_bind_vc(struct nk_thread *thread, struct nk_virtual_console * cons);

int nk_release_vc(struct nk_thread *thread);

int nk_switch_to_vc(struct nk_virtual_console *vc);
int nk_switch_to_prev_vc();
int nk_switch_to_next_vc();

int nk_vc_putchar(uint8_t c);
int nk_vc_puts(char *s); 
int nk_vc_setattr(uint8_t attr);
int nk_vc_clear(uint8_t attr);
int nk_vc_scrollup (struct nk_virtual_console *vc);
int nk_vc_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y);

int nk_vc_enqueue_scancode(struct nk_virtual_console *vc, nk_scancode_t scan);
int nk_vc_enqueue_keycode(struct nk_virtual_console *vc, nk_keycode_t key);
nk_scancode_t nk_vc_dequeue_scancode(struct nk_virtual_console *vc);
nk_keycode_t  nk_vc_dequeue_keycode(struct nk_virtual_console *vc);

nk_keycode_t nk_vc_getchar();
nk_scancode_t nk_vc_get_scancode();

int nk_vc_handle_input(uint8_t scan);

int nk_vc_init();
int nk_vc_deinit();
#endif
