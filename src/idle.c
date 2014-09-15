#include <nautilus.h>
#include <cga.h>
#include <idle.h>
#include <cpu.h>
#include <thread.h>

#define TIMEOUT 1000000
#define INNER_LOOP_DELAY 50000

/*
                     ####
                     #  #
                     #  #
                     ####
*/

void 
idle (void * arg)
{
    while (1) {
        yield();
    }
}


void 
screensaver(void * arg)
{
    uint8_t color = COLOR_LIGHT_RED;
    char c = '#';
    int start_row = 15;
    int start_col = 22;
    int i;
    int n = TIMEOUT;

    udelay(10*1000000);
    printk("Running idle loop on core %d\n", my_cpu_id());
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

        yield();
    }

    term_clear();
    term_setpos(0,0);

}


void 
side_screensaver(void * arg)
{
    uint8_t color = COLOR_LIGHT_RED;
    char c = '#';
    int start_row = 13;
    int start_col = 55;
    int i;
    int n = TIMEOUT;
    size_t old_col,old_row;

    printk("Running idle loop on core %d\n", my_cpu_id());

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

        yield();
    }

    term_setpos(0,0);

}

