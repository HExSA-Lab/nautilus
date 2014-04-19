#include <cga.h>
#include <string.h>
#include <types.h>
 
 
static uint8_t 
make_color (enum vga_color fg, enum vga_color bg) 
{
    return fg | bg << 4;
}

 
static uint16_t 
make_vgaentry (char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}
 
 
static size_t    term_row;
static size_t    term_col;
static uint8_t   term_color;
static uint16_t* term_buf;
 

void term_init()
{
    term_row = 0;
    term_col = 0;
    term_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    term_buf = (uint16_t*) CGA_BASE_ADDR;
    for ( size_t y = 0; y < VGA_HEIGHT; y++ )
    {
        for ( size_t x = 0; x < VGA_WIDTH; x++ )
        {
            const size_t index = y * VGA_WIDTH + x;
            term_buf[index] = make_vgaentry(' ', term_color);
        }
    }
}
 

static void 
term_setcolor (uint8_t color)
{
    term_color = color;
}
 

static void 
term_putc
(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    term_buf[index] = make_vgaentry(c, color);
}
 

static void 
term_scrollup (void) 
{
    int i;
    for (i = 1; i < VGA_HEIGHT; i++) {
        size_t prev_row = VGA_WIDTH * (i-1);
        size_t this_row = VGA_WIDTH * i;
        memcpy(&term_buf[prev_row], &term_buf[this_row], VGA_WIDTH);
    }

    size_t index = (VGA_HEIGHT-1) * VGA_WIDTH;
    for (i = 0; i < VGA_WIDTH; i++, index++) {
        term_buf[index] = make_vgaentry(' ', term_color);
    }
}


void 
putchar (char c)
{
    if (c == '\n') {
        term_col = 0;
        if (++term_row == VGA_HEIGHT) {
            term_scrollup();
            term_row--;
        }
        return;
    }

    term_putc(c, term_color, term_col, term_row);
    if (++term_col == VGA_WIDTH) {
        term_col = 0;
        if (++term_row == VGA_HEIGHT) {
            term_scrollup();
            term_row--;
        }
    }
}


int puts
(const char *s)
{
    while (*s)
    {
        putchar(*s);
        s++;
    }
    putchar('\n');
    return 0;
}
 

static void 
get_cursor (unsigned row, unsigned col) 
{
    return;
}

static void
show_cursor (void)
{
    /* not implemented */
    return;
}

static void
hide_cursor (void)
{
    /* not implemented */
    return;
}

void 
term_print (const char* data)
{
    size_t datalen = strlen(data);
    for (size_t i = 0; i < datalen; i++) {
        putchar(data[i]);
    }
}
