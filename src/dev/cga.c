#include <nautilus/nautilus.h>
#include <nautilus/cga.h>
#include <nautilus/naut_string.h>
#include <nautilus/spinlock.h>
#include <nautilus/cpu.h>

static struct term_info {
    size_t    row;
    size_t    col;
    uint8_t   color;
    volatile uint16_t * buf;
    spinlock_t lock;
} term;


static void
hide_cursor (void)
{
    outw(0x200a, 0x3d4);
}
 
 
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
 

void term_setpos (size_t x, size_t y)
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    term.row = y;
    term.col = x;
    spin_unlock_irq_restore(&(term.lock), flags);
}


void term_getpos (size_t* x, size_t* y)
{
    *x = term.col;
    *y = term.row;
}


void 
term_init (void)
{
    term.row = 0;
    term.col = 0;
    term.color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    term.buf = (uint16_t*) VGA_BASE_ADDR;
    spinlock_init(&(term.lock));
    size_t y;
    size_t x;
    for ( y = 0; y < VGA_HEIGHT; y++ )
    {
        for ( x = 0; x < VGA_WIDTH; x++ )
        {
            const size_t index = y * VGA_WIDTH + x;
            term.buf[index] = make_vgaentry(' ', term.color);
        }
    }

    hide_cursor();
}
 

uint8_t 
term_getcolor (void)
{
    return term.color;
}


void 
term_setcolor (uint8_t color)
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    term.color = color;
    spin_unlock_irq_restore(&(term.lock), flags);
}
 

static inline void 
debug_putc (char c)
{ 
    outb(c, 0xc0c0);
    outb(c, 0x402);
}


void 
debug_puts (const char * s) 
{
    while (*s) {
        debug_putc(*s);
        s++;
    }
    debug_putc('\n');
}


/* NOTE: should be holding a lock while in this function */
void 
term_putc (char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    term.buf[index] = make_vgaentry(c, color);
}
 

inline void
term_clear (void) 
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    size_t i;
    for (i = 0; i < VGA_HEIGHT*VGA_WIDTH; i++) {
        term.buf[i] = make_vgaentry(' ', term.color);
    }
    spin_unlock_irq_restore(&(term.lock), flags);
}


/* NOTE: should be holding a lock while in this function */
static void 
term_scrollup (void) 
{
    int i;
    int n = (((VGA_HEIGHT-1)*VGA_WIDTH*2)/sizeof(long));
    int lpl = (VGA_WIDTH*2)/sizeof(long);
    long * pos = (long*)term.buf;
    
    for (i = 0; i < n; i++) {
        *pos = *(pos + lpl);
        ++pos;
    }

    size_t index = (VGA_HEIGHT-1) * VGA_WIDTH;
    for (i = 0; i < VGA_WIDTH; i++) {
        term.buf[index++] = make_vgaentry(' ', term.color);
    }
}


void 
putchar (char c)
{
    uint8_t flags = spin_lock_irq_save(&(term.lock));
    
    if (c == '\n') {
        term.col = 0;
        if (++term.row == VGA_HEIGHT) {
            term_scrollup();
            term.row--;
        }
        spin_unlock_irq_restore(&(term.lock), flags);
        return;
    }

    term_putc(c, term.color, term.col, term.row);

    if (++term.col == VGA_WIDTH) {
        term.col = 0;
        if (++term.row == VGA_HEIGHT) {
            term_scrollup();
            term.row--;
        }
    }

    spin_unlock_irq_restore(&(term.lock), flags);
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



void 
term_print (const char* data)
{
    int i;
    size_t datalen = strlen(data);
    for (i = 0; i < datalen; i++) {
        putchar(data[i]);
    }
    
}
