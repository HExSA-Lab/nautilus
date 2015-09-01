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
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/cga.h>
#include <nautilus/idle.h>
#include <nautilus/cpu.h>
#include <nautilus/thread.h>

#define TIMEOUT 1000000
#define INNER_LOOP_DELAY 50000

/*
                     ####
                     #  #
                     #  #
                     ####
*/


static inline void 
idle_delay (unsigned long long n)
{
    int i = 0;
    while (--n) {
        i += 1;
        asm volatile ("":::"memory");
    }

    /* force compiler to emit */
    n = n + i;
}


void 
idle (void * in, void ** out)
{
    get_cur_thread()->is_idle = 1;

    while (1) {

        nk_yield();

#ifdef NAUT_CONFIG_XEON_PHI
        udelay(1);
#else
        idle_delay(100);
#endif

#ifdef NAUT_CONFIG_HALT_WHILE_IDLE
        sti();
        halt();
#endif
    }
}


void 
screensaver(void * in, void ** out)
{
    uint8_t color = COLOR_LIGHT_RED;
    char c = '#';
    int start_row = 15;
    int start_col = 22;
    int i;
    int n = TIMEOUT;

    printk("Running screensaver on core %d\n", my_cpu_id());
    udelay(2*1000000);
    
    term_clear();
    term_setpos(0, 0);

    show_splash();

    printk( "Idle Loop (Running on core %d)\n\n", my_cpu_id());

    while (1) {

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, ++start_col, start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, start_col, ++start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, --start_col, start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, start_col, --start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        nk_yield();
    }

    term_clear();
    term_setpos(0,0);

}


void 
side_screensaver(void * in, void ** out)
{
    uint8_t color = COLOR_LIGHT_RED;
    char c = '#';
    int start_row = 13;
    int start_col = 55;
    int i;
    int n = TIMEOUT;
    size_t old_col,old_row;

    printk("Running screensaver on core %d\n", my_cpu_id());

    term_getpos(&old_col, &old_row);
    term_setpos(0, 0);

    show_splash();

    term_setpos(old_col, old_row);
    
    while (1) {

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, ++start_col, start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, start_col, ++start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, --start_col, start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            term_putc(c, color, start_col, --start_row);
            udelay(INNER_LOOP_DELAY);
        } 

        nk_yield();
    }

    term_setpos(0,0);

}

