/* 
 *
 * Userspace Console for Nautilus on the Xeon Phi 
 *
 * Kyle C. Hale (c) 2015
 *
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <curses.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define DEBUG 1

#if DEBUG == 0
#define NODEBUG
#endif

#include <assert.h>

#define DEBUG_SCREEN_LINES 10
#define INFO_WIN_LINES 8

#define MAX_HIST_LINES 100000

// We only need a few megs of it though
#define PHI_GDDR_RANGE_SZ (1024*1024*512)

#define MEM_BASE_PATH "/sys/class/mic/mic"
#define MEM_FILE      "/device/resource0"
#define DEFAULT_TIMEOUT 60
#define MAX_TIMEOUT     (60*5)
#define STATUS_ADDR 0x7a7a70
#define STATUS_OK   0xd007d007

#define MIN_TTY_COLS 80
#define MIN_TTY_ROWS 25

#define PHI_FB_ADDR   0xb8000
#define PHI_FB_WIDTH  80
#define PHI_FB_HEIGHT 25
// in bytes
#define PHI_FB_LEN    (PHI_FB_WIDTH*PHI_FB_HEIGHT*2)

#define PHI_FB_REGS_OFFSET PHI_FB_LEN
// is there something to draw?
#define OUTPUT_AVAIL_REG_OFFSET 0x0

// ready for next update
#define OUTPUT_DRAWN_REG_OFFSET 0x1

#define PHI_BASE_PATH "/sys/class/mic/mic"
#define PHI_STATE_FILE "/state"
#define PHI_BOOT_STR "boot:linux:"


// character was drawn
// upper = x
// lower = y
#define CHAR_REG_OFFSET 0x2

// cursor update
// upper = x
// lower = y
#define CURSOR_REG_OFFSET 0x3

#define LINE_REG_OFFSET 0x4


typedef enum {
    TYPE_NO_UPDATE = 0,
    TYPE_CHAR_DRAWN,
    TYPE_LINE_DRAWN,
    TYPE_CURSOR_UPDATE,
    TYPE_SCREEN_REDRAW,
    TYPE_CONSOLE_SHUTDOWN,
    TYPE_SCROLLUP,
    TYPE_INVAL
} update_type_t;

typedef unsigned char uchar_t;

static struct {
    WINDOW * win;
    WINDOW * debug;
    uint16_t x;
    uint16_t y;
    uint16_t rows;
    uint16_t cols;
    void * gddr;
    unsigned long gddr_size;
    volatile uint16_t * fb;
    volatile uint32_t * ctrl_regs;
    struct termios termios_old;

    struct line_elm * line_hist;
    struct line_elm * win_ptr; 
    int line_idx; // 0 is pointing at top of framebuffer, > 0 is pointing at hist buff
    unsigned hist_cnt;

} console;


struct line_elm {
    uint16_t          line[PHI_FB_WIDTH];
    struct line_elm * next;
    struct line_elm * prev;
};


#if DEBUG == 1
#define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG: " fmt, ##args);
#define DEBUG_SCREEN(fmt, args...) wprintw(console.debug, fmt, ##args); wrefresh(console.debug);
#else
#define DEBUG_PRINT(fmt, args...) 
#define DEBUG_SCREEN(fmt, args...)
#endif


static char* micnum = "0";

static void
usage (char * prog)
{
    fprintf(stderr, "\nUsage: %s -b <bootloader> -k <kernel> [-t <timeout> ] [-m <micnum> ] [-o <file>] [-h] \n"
                    "\t-t    Timeout to use. Default is 60 seconds\n"
                    "\t-m    Which MIC device to use. Default is 0\n"
                    "\t-b    Path to the bootloader to boot with\n"
                    "\t-k    Path to the kernel to boot with\n"
                    "\t-o    Output to file\n"
                    "\t-h    Print this message\n\n", prog);
    exit(EXIT_SUCCESS);
}


static inline uint32_t 
console_read_reg (uint32_t reg)
{
    assert(console.ctrl_regs);
    return *(volatile uint32_t*)(console.ctrl_regs + reg);
}


static inline void
console_write_reg (uint32_t reg, uint32_t val)
{
    assert(console.ctrl_regs);
    *(volatile uint32_t*)(console.ctrl_regs + reg) = val;
    msync(console.gddr, console.gddr_size, MS_SYNC);
}


static update_type_t
wait_for_cons_update (void)
{
    update_type_t res;
    unsigned delay_cnt = 1000;

    while ((res = console_read_reg(OUTPUT_AVAIL_REG_OFFSET)) == TYPE_NO_UPDATE &&
           delay_cnt--) {
    }

    return res;
}


static void 
draw_done (void)
{
    console_write_reg(OUTPUT_DRAWN_REG_OFFSET, 1);
}


static int 
check_terminal_size (void)
{
    uint16_t ncols = 0;
    uint16_t nrows = 0;
    struct winsize winsz;
    ioctl(fileno(stdin), TIOCGWINSZ, &winsz);
    ncols = winsz.ws_col;
    nrows = winsz.ws_row;

    if (ncols < MIN_TTY_COLS || nrows < MIN_TTY_ROWS)
    {
        printf("Your window is not large enough.\n");
        printf("It must be at least %ux%u, but yours is %ux%u\n", MIN_TTY_COLS, MIN_TTY_ROWS, ncols, nrows);
        return -1;
    }

    return 0;
}


/* NOTE: this console will support only 16 colors */
static void
init_colors (void)
{
    start_color();
    int i;
    for (i = 0; i < 0x100; i++) {
        init_pair(i, i & 0xf, (i >> 4) & 0xf);
    }
}




static inline uint16_t
get_cursor_x (void)
{
    return console.x;
}


static inline uint16_t
get_cursor_y (void)
{
    return console.y;
}


static int
handle_cursor_update (void)
{
    /* TODO */
    //wmove(console.win, y, x);
    return 0;
}




static int
handle_char_update (void)
{
    uint32_t cinfo = console_read_reg(CHAR_REG_OFFSET);
    uint16_t x     = cinfo & 0xff;
    uint16_t y     = cinfo >> 16;

    assert(console.fb);

    uint16_t entry = console.fb[y*PHI_FB_WIDTH+x];
    char c      = entry & 0xff;
    uint8_t color  = entry >> 8; 

    DEBUG_SCREEN("Handling char update '%c' @(%u, %u)\n", c, x, y);

    wattron(console.win, COLOR_PAIR(color));
    /* draw the character */
    mvwaddch(console.win, y, x, isprint(c) ? c : '@');
    wattroff(console.win, COLOR_PAIR(color));

    /* display it */
    wrefresh(console.win);

    return 0;
}

static void 
draw_line_buf (uint16_t * buf, unsigned row) 
{
    assert(row < PHI_FB_HEIGHT);
    int i;
    for (i = 0; i < PHI_FB_WIDTH; i++) {
        char c     = buf[i] & 0xff;
        char color = buf[i] >> 8;
        wattron(console.win, COLOR_PAIR(color));
        mvwaddch(console.win, 
                row, 
                i, 
                isprint(c) ? c : '@');
        wattroff(console.win, COLOR_PAIR(color));
    }
}


static void 
draw_fb (unsigned offset, unsigned nlines)
{
    assert(nlines <= PHI_FB_HEIGHT);
    assert(offset < PHI_FB_HEIGHT);
    assert(nlines + offset <= PHI_FB_HEIGHT);
    int i;
    DEBUG_SCREEN("Draw  fb offset:%u nlines:%u\n", offset, nlines);

    for (i = 0; i < nlines; i++) {
        draw_line_buf((uint16_t*)(console.fb + i*PHI_FB_WIDTH), offset++);
    }
}


static void
draw_hist_buf (struct line_elm * head, 
               unsigned offset,
               unsigned nlines)
{
    assert(nlines <= PHI_FB_HEIGHT);
    assert(offset < PHI_FB_HEIGHT);
    assert(nlines + offset <= PHI_FB_HEIGHT);

    DEBUG_SCREEN("Draw hist buf offset:%u nlines:%u\n", offset, nlines);

    int i;
    for (i = 0; i < nlines && head != NULL && head != console.line_hist; i++) {
        /* copy this line bufer into the window at offset n */
        draw_line_buf(head->line, offset++);
        head = head->prev;
    }
}


/* 
 * move our window around in the history buffer.
 * an offset of 0 means that we're not including any
 * of the history buffer lines in the output window 
 */
static void
scroll_output_win (int nlines)
{
    int i;
    struct line_elm * line = (console.win_ptr == NULL) ? console.line_hist : console.win_ptr;
    int fb_lines;
    int hist_lines;
    int scroll = 0;

    assert(line);

    /* move the window */
    if (nlines > 0) { 

        if (console.line_idx >= MAX_HIST_LINES) return;


        while (scroll != nlines && 
                console.line_idx < MAX_HIST_LINES && 
                console.line_idx < console.hist_cnt) {

            scroll++;
            console.line_idx++;

            if (line->next) {
                line = line->next;
            } else {
                break;
            }
        
        }


        console.win_ptr = line;
        
    } else if (nlines < 0) {

        if (console.line_idx == 0) return;
        
        while (scroll != nlines && line && console.line_idx > 0) {
            line = line->prev;
            scroll--;
            console.line_idx--;
        }

        console.win_ptr = line;
        
    } 

    DEBUG_SCREEN("Tried %d Scrolled %d lines, line idx is now %u\n", nlines, scroll, console.line_idx);

}

static void 
scroll_top (void) 
{
    if (console.line_hist && console.line_hist->prev) {
        console.win_ptr = console.line_hist->prev;
        console.line_idx = console.hist_cnt;
    } 
}

static void
scroll_bottom (void)
{
    console.win_ptr = NULL;
    console.line_idx = 0;
}


static void
draw_output_win (void)
{

    /* only need to draw FB */
    if (console.line_idx == 0) {
        DEBUG_SCREEN("drawing only fb\n");

        draw_fb(0, PHI_FB_HEIGHT);

    /* only need to draw history buf */
    } else if (console.line_idx >= PHI_FB_HEIGHT) {
        DEBUG_SCREEN("drawing only history\n");

        draw_hist_buf(console.win_ptr, 0, PHI_FB_HEIGHT);

    /* need to draw from both */
    } else {

        DEBUG_SCREEN("drawing mix\n");
        draw_hist_buf(console.win_ptr, 0, console.line_idx);
        draw_fb(console.line_idx, PHI_FB_HEIGHT - console.line_idx);

    }

    wrefresh(console.win);
}

static int
handle_line_update (void)
{
    uint32_t row = console_read_reg(LINE_REG_OFFSET);
    unsigned i;

    assert(row < PHI_FB_HEIGHT);
    assert(console.fb);

    DEBUG_SCREEN("Handling line update for line %u\n", row);
    
    draw_output_win();
#if 0
    for (i = 0; i < PHI_FB_WIDTH; i++) {
        uint16_t entry = console.fb[(row * PHI_FB_WIDTH) + i];
        char c = entry & 0xff;
        uint8_t color = entry >> 8;
        wattron(console.win, COLOR_PAIR(color));
        /* draw the character */
        mvwaddch(console.win, row, i, isprint(c) ? c : '@');
        wattroff(console.win, COLOR_PAIR(color));
    }
#endif
}

/* dir = 1 => forward 
 * dir = 0 => backward
 */
static void 
scroll_page (uint8_t dir)
{
    if (dir == 1) {
        scroll_output_win(PHI_FB_HEIGHT);
    } else if (dir == 0) {
        scroll_output_win(-PHI_FB_HEIGHT);
    }
}


/* save the first line, it's about to go away */
static int
handle_scrollup (void)
{
    int i;
    assert(console.fb);

    /* make a new history line elm */
    struct line_elm * new = malloc(sizeof(struct line_elm));
    if (!new) {
        fprintf(stderr, "Could not allocate new line\n");
        return -1;
    }
    memset(new, 0, sizeof(struct line_elm));

    for (i = 0; i < PHI_FB_WIDTH; i++) {
        new->line[i] = console.fb[i];
    }
    
    /* kill the last one */
    if (console.hist_cnt == MAX_HIST_LINES) {

        assert(console.line_hist->prev);
        assert(console.line_hist->prev->prev);

        new->prev = console.line_hist->prev->prev;
        console.line_hist->prev->prev->next = NULL;

        free(console.line_hist->prev);

    } else {

        if (console.line_hist) {
            new->prev = console.line_hist->prev;
        } else {
            new->prev = new;
        }

        console.hist_cnt++;
    }

    if (console.line_hist) {
        console.line_hist->prev = new;
    } 

    /* if this is the first one, we're going to assign NULL anyhow */
    new->next = console.line_hist;

    /* add to the head of the list */
    console.line_hist = new;
    
    DEBUG_SCREEN("Handling scrollup: histcount=%u\n", console.hist_cnt);
}


static int
handle_screen_redraw (void)
{
    assert(console.fb);
    int i;

    DEBUG_SCREEN("Handling screen redraw\n");

/*
    for (i = 0; i < PHI_FB_LEN/2; i++) {
        char c = console.fb[i] & 0xff;
        char color = console.fb[i] >> 8;
        wattron(console.win, COLOR_PAIR(color));
        mvwaddch(console.win, 
                i / PHI_FB_WIDTH,
                i % PHI_FB_WIDTH, 
                isprint(c) ? c : '@');
        wattroff(console.win, COLOR_PAIR(color));
    }
    wrefresh(console.win);
*/

    draw_output_win();
}




static int
console_main_loop (void) 
{
    while (1) {

        update_type_t update = wait_for_cons_update();

        switch (update) {

            case TYPE_NO_UPDATE: 
                break;
            case TYPE_CHAR_DRAWN: 
                if (handle_char_update() != 0) {
                    fprintf(stderr, "Error handling character update\n");
                    return -1;
                }
                break;
            case TYPE_LINE_DRAWN:
                if (handle_line_update() != 0) {
                    fprintf(stderr, "Error handling line update\n");
                    return -1;
                }
                break;
            case TYPE_SCREEN_REDRAW:
                if (handle_screen_redraw() != 0) {
                    fprintf(stderr, "Error handling screen redraw\n");
                    return -1;
                }
                break;
            case TYPE_CURSOR_UPDATE:  /* TODO */
                if (handle_cursor_update() != 0) {
                    fprintf(stderr, "Error handling cursor update\n");
                    return -1;
                }
                break;
            case TYPE_CONSOLE_SHUTDOWN:
                printf("Received console shutdown signal\n");
                break;
            case TYPE_SCROLLUP:
                if (handle_scrollup() != 0) {
                    fprintf(stderr, "Error handling scrollup\n");
                    return -1;
                }
                break;
            default: 
                fprintf(stderr, "Unknown update type (0x%x)\n", update);
                return -1;
        }

        console_write_reg(OUTPUT_AVAIL_REG_OFFSET, 0);
        draw_done();

        int c = wgetch(console.win);

        switch (c) {
            case KEY_LEFT:
                DEBUG_SCREEN("history count: %u\n", console.hist_cnt);
                break;
            case KEY_UP:
                DEBUG_SCREEN("Scrolling back one line\n");
                scroll_output_win(1);
                draw_output_win();
                break;
            case KEY_DOWN:
                DEBUG_SCREEN("Scrolling forward one line\n");
                scroll_output_win(-1);
                draw_output_win();
                break;
            case KEY_NPAGE:
                DEBUG_SCREEN("Scrolling forward one page\n");
                scroll_page(0);
                draw_output_win();
                break;
            case KEY_PPAGE:
                DEBUG_SCREEN("Scrolling back one page\n");
                scroll_page(1);
                draw_output_win();
                break;
            case 'g': 
                DEBUG_SCREEN("Scrolling to beginning\n");
                scroll_top();
                draw_output_win();
                break;
            case 'G': 
                DEBUG_SCREEN("Scrolling to end\n");
                scroll_bottom();
                draw_output_win();
                break;
            default:
                break;

        }
    }

    erase();
    printf("Console terminated\n");
    return 0;
}


static int
console_setup_window (WINDOW * w)
{
    if (scrollok(w, 1) != OK) {
        fprintf(stderr, "Problem setting scrolling\n");
        return -1;
    }

    if (erase() != OK) {
        fprintf(stderr, "Problem erasing screen\n");
        return -1;
    }

    if (cbreak() != OK) {
        fprintf(stderr, "Error putting terminal in raw mode\n");
        return -1;
    }

    if (noecho() != OK) {
        fprintf(stderr, "Error setting noecho\n");
        return -1;
    }

    if (keypad(w, TRUE) != OK) {
        fprintf(stderr, "Error setting keypad options\n");
        return -1;
    }

    /* set non-blocking read for keyboard with no timer */
    wtimeout(w, 0);

    return 0;
}


static int 
pollit (void * base, 
        uint64_t offset,
        uint32_t doorbell_val)
{
    uint32_t val = 0;
    int i;

    val = *(uint32_t*)((char*)base + offset);

    if (val == doorbell_val) {
        return 1;
    }

    return 0;
}


static int
wait_for_doorbell (void * base,
                   uint64_t offset,
                   uint32_t doorbell_val,
                   uint32_t timeout,
                   const char * ok_msg)
{
    uint8_t ping_recvd = 0;

    /* need to give the card some time to boot */
    while (timeout) {

        if (pollit(base, offset, doorbell_val)) {
            ping_recvd = 1;
            break;
        }

        putchar('.');
        fflush(stdout);

        sleep(1);
        --timeout;
    }

    if (ping_recvd) {
        printf("OK: %s\n", ok_msg);
    } else {
        printf("FAIL: timeout\n");
        return -1;
    }

    return 0;
}


static void
reset_phi (void)
{
    char cmd_str[128];
    sprintf(cmd_str, "micctrl --reset mic%s\n", micnum);
    system(cmd_str);
}


/* 
 * this will use Intel's Xeon Phi driver sysfs interface
 * to initiate a boot on the card using a custom image
 *
 */
static int
boot_phi (const char * bpath, const char * kpath)
{
    char boot_str[128];
    char phi_path[128];
    char phi_state[64];
    struct stat s;
    FILE* fd = 0;

    /* first stat the kernel and bootloader to make sure they exist */
    if (stat(bpath, &s) != 0) {
        fprintf(stderr, "Invalid bootloader file %s (%s)\n", bpath, strerror(errno));
        return -1;
    }

    if (stat(kpath, &s) != 0) {
        fprintf(stderr, "Invalid kernel file %s (%s)\n", kpath, strerror(errno));
        return -1;
    }

    memset(boot_str, 0, sizeof(boot_str));
    strcat(boot_str, PHI_BOOT_STR);
    strcat(boot_str, bpath);
    strcat(boot_str, ":");
    strcat(boot_str, kpath);

    memset(phi_path, 0, sizeof(phi_path));

    strcat(phi_path, PHI_BASE_PATH);
    strcat(phi_path, micnum);
    strcat(phi_path, PHI_STATE_FILE);

    fd = fopen(phi_path, "r+");

    if (fd < 0) {
        fprintf(stderr, "Couldn't open file %s (%s)\n", phi_path,
                strerror(errno));
        return -1;
    }

    fscanf(fd, "%s", phi_state);

    /* Does this Phi need to be reset? */
    if (strncmp(phi_state, "ready", 5) != 0) {

        reset_phi();

        while (strncmp(phi_state, "ready", 5) != 0) {
            rewind(fd);
            memset(phi_state, 0, sizeof(phi_state));
            fscanf(fd, "%s", phi_state);
            usleep(100);
        }
    }

    rewind(fd);

    if (fwrite(boot_str, 1, strlen(boot_str), fd) < 0) {
        fprintf(stderr, "Couldnt write file %s (%s)\n", phi_path, strerror(errno));
        return -1;
    }

    fclose(fd);
    return 0;
}


static void*
map_phi (void)
{
    int memfd;
    uint64_t range_sz = PHI_GDDR_RANGE_SZ;
    char mempath[128];
    
    memset(mempath, 0, sizeof(mempath));

    strcat(mempath, MEM_BASE_PATH);
    strcat(mempath, micnum);
    strcat(mempath, MEM_FILE);

    if (!(memfd = open(mempath, O_RDWR | O_SYNC))) {
        fprintf(stderr, "Error opening file %s\n", mempath);
        return NULL;
    }

    printf("Mapping MIC %s GDDR memory range (%u bytes)\n",
            micnum, range_sz);

    void * buf = mmap(NULL, 
                      range_sz,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, 
                      memfd,
                      0);

    DEBUG_PRINT("Mapped it into host process at %p\n", buf);

    if (buf == MAP_FAILED) {
        fprintf(stderr, "Couldn't map PHI MMIO range\n");
        return NULL;
    }

    return buf;
}


static void 
console_init (void * mapped_phi_gddr)
{
    console.x         = 0;
    console.y         = 0;
    console.rows      = PHI_FB_HEIGHT;
    console.cols      = PHI_FB_WIDTH;
    console.gddr      = mapped_phi_gddr;
    console.gddr_size = PHI_GDDR_RANGE_SZ;
    console.fb        = (void*)( ((char*)mapped_phi_gddr) + PHI_FB_ADDR );
    console.ctrl_regs = (void*)( ((char*)mapped_phi_gddr) + PHI_FB_ADDR + PHI_FB_REGS_OFFSET );


    console.line_idx = 0;
    console.line_hist = NULL;
    console.win_ptr  = NULL;
    console.hist_cnt = 0;

    DEBUG_PRINT("Console framebuffer at %p\n", (void*)console.fb);
    DEBUG_PRINT("Console ctrl_regs at %p\n", (void*)console.ctrl_regs);
}


static void
sigint_handler (int sig)
{
    wclear(console.win);
    mvwaddstr(console.win, 0,0, "Caught ^C, exiting and resetting MIC\n");
    wrefresh(console.win);
    sleep(1);
    reset_phi();
    
}

static void
sigsegv_handler (int sig)
{
    wclear(console.win);
    mvwaddstr( console.win,0,0, "Caught SIGSEGV, exiting and resetting MIC\n");
    wrefresh(console.win);
    sleep(2);
    reset_phi();
}

static void
handle_exit (void)
{
    printf("Exiting from console terminal\n");
    fflush(stdout);
    reset_phi();
    sleep(2);
    endwin();
}



int 
main (int argc, char ** argv)
{
    struct termios termios;
    int cons_fd;
    void * buf;
    int maxx, maxy;
    int c;
    char * bootloader = NULL;
    char * kernel = NULL;

    unsigned timeout  = DEFAULT_TIMEOUT;

    /* reset the phi on exit */
    signal(SIGINT, sigint_handler);
    signal(SIGSEGV, sigsegv_handler);

    opterr = 0;

    while ((c = getopt(argc, argv, "htm:b:k:")) != -1) {
        switch (c) {
            case 'h': 
                usage(argv[0]);
                break;
            case 't': 
                timeout = atoi(optarg);
                if (timeout > MAX_TIMEOUT) {
                    timeout = MAX_TIMEOUT; 
                } else if (timeout <= 0) {
                    timeout = DEFAULT_TIMEOUT;
                }
                break;
            case 'm': 
                micnum = optarg;
                break;
            case 'k':
                kernel = optarg;
                break;
            case 'b':
                bootloader = optarg;
                break;
            case '?':
                if (optopt == 'm' || optopt == 't') {
                    fprintf(stderr, "Option -%c requires argument\n", optopt);
                } else if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option `-%c'\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option character `\\x%x'\n", optopt);
                }
                exit(EXIT_FAILURE);
            default:
                abort();
        }
    }

    if (!bootloader || !kernel) { 
        usage(argv[0]);
    }

    buf = map_phi();

    printf("Booting PHI with custom image...");

    if (!boot_phi(bootloader, kernel)) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for Nautilus doorbell (timeout=%us)\n", timeout);

    if (wait_for_doorbell(buf,
                        0x7a7a90,
                        0xb003b003,
                        timeout,
                        "Made it to Nautilus boot code") == -1) {
        fprintf(stderr, "Exiting\n");
        exit(EXIT_FAILURE);
    }

    tcgetattr(fileno(stdin), &console.termios_old);
    atexit(handle_exit);

    DEBUG_PRINT("Initializing screen\n");

    console_init(buf);

    // create the screen
    initscr();
    curs_set(FALSE);
    init_colors();
    getmaxyx(stdscr, maxy, maxx);

#if DEBUG == 1
    console.win = newwin(maxy - DEBUG_SCREEN_LINES - INFO_WIN_LINES, maxx, 0, 0);
#else 
    console.win = newwin(maxy - INFO_WIN_LINES, maxx, 0, 0);
#endif

    if (!console.win) {
        fprintf(stderr, "Error initializing curses screen\n");
        exit(EXIT_FAILURE);
    }

    if (console_setup_window(console.win) != 0) {
        fprintf(stderr, "Error setting up console\n");
        exit(EXIT_FAILURE);
    }

#if DEBUG == 1
    WINDOW * phi_info_border = newwin(INFO_WIN_LINES, maxx, maxy - DEBUG_SCREEN_LINES - INFO_WIN_LINES, 0);
#else 
    WINDOW * phi_info_border = newwin(INFO_WIN_LINES, maxx, maxy  - INFO_WIN_LINES, 0);
#endif

    box(phi_info_border, 0, 0);
    mvwprintw(phi_info_border, 0, maxx/2 - 9, " CONNECTED TO MIC %u ", atoi(micnum));
    wrefresh(phi_info_border);
 
#if DEBUG == 1
    WINDOW * phi_info = newwin(INFO_WIN_LINES-2, maxx-4, maxy - DEBUG_SCREEN_LINES - INFO_WIN_LINES+1, 2);
#else
    WINDOW * phi_info = newwin(INFO_WIN_LINES-2, maxx-4, maxy - INFO_WIN_LINES+1, 2);
#endif
    
    mvwprintw(phi_info, 0, 0, "Up Arrow:   scroll back\n");
    mvwprintw(phi_info, 1, 0, "Down Arrow: scroll forward\n");
    mvwprintw(phi_info, 2, 0, "Page Up:    scroll back one page\n");
    mvwprintw(phi_info, 3, 0, "Page Down:  scroll forward one page\n");
    mvwprintw(phi_info, 4, 0, "Ctrl^C:     quit\n");
    int phix, phiy;
    getmaxyx(phi_info, phiy, phix);
    mvwprintw(phi_info, 0, phix/2, "g: scroll to beginning\n");
    mvwprintw(phi_info, 1, phix/2, "G: scroll to end\n");
    wrefresh(phi_info);
       

#if DEBUG == 1
    WINDOW * border = newwin(DEBUG_SCREEN_LINES, maxx, maxy - DEBUG_SCREEN_LINES, 0);
    box(border, 0, 0);
    mvwprintw(border, 0, maxx/2 - 12, "PHI CONSOLE DEBUG WINDOW");
    wrefresh(border);

    console.debug = newwin(DEBUG_SCREEN_LINES-2, maxx-4, maxy - DEBUG_SCREEN_LINES+ 1, 2);
    if (!console.debug) {
        fprintf(stderr, "Could not create output screen\n");
        exit(EXIT_FAILURE);
    }
    console_setup_window(console.debug);
#endif

    // won't come back from here until exit
    console_main_loop();

#if DEBUG == 1
    delwin(border);
    delwin(console.debug);
#endif

    delwin(console.win);
    delwin(phi_info);
    delwin(phi_info_border);
    endwin();

    return 0;
}

