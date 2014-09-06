#include <nautilus.h>
#include <cga.h>
#include <idle.h>
#include <cpu.h>

/*
                     ####
                     #  #
                     #  #
                     ####
*/


void idle(void)
{
    uint8_t color = COLOR_LIGHT_RED;
    char c = '#';
    //int start_row = VGA_HEIGHT/2 - ICON_HEIGHT/2;
    //int start_col = VGA_WIDTH/2 - ICON_WIDTH/2;
    int start_row = 15;
    int start_col = 22;
    int i;
    int n = 1000000;

    udelay(10*1000000);
    printk("Running idle loop...\n");
    udelay(2*1000000);
    
    term_clear();
    term_setpos(0, 0);

    show_splash();

    printk( "                    Idle Loop\n\n");

    while (n--) {

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            udelay(100);
            term_putc(c, color, ++start_col, start_row);
            udelay(1000);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            udelay(100);
            term_putc(c, color, start_col, ++start_row);
            udelay(1000);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            udelay(100);
            term_putc(c, color, --start_col, start_row);
            udelay(1000);
        } 

        for (i = 0; i < ICON_WIDTH; i++) {
            term_putc(' ', color, start_col, start_row);
            udelay(100);
            term_putc(c, color, start_col, --start_row);
            udelay(1000);
        } 
    }

    while (1) {
        halt();
    }
}

