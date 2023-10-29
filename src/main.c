/*
Implementation of BBC BASIC for Neotron Pico

Inspired by and in some places copied from https://github.com/Memotech-Bill/PicoBB

*/

#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "BBC.h"

// UART registers for the MPS3-AN547 machine that QEMU emulates
#define UART0_BASE ((unsigned int *)0x59303000)
#define UART0_STATUS (UART0_BASE + 1)
#define UART0_CONTROL (UART0_BASE + 2)
#define UART0_BAUDDIV (UART0_BASE + 4)

#define PAGE_OFFSET ACCSLEN + 0x1300
#define HISTORY 10

unsigned int palette[256];
void *TTFcache[1];
char userRAMBuffer[16384];
void *userRAM = userRAMBuffer;
void *userTOP = NULL;
char *szLoadDir;
char *szLibrary;
char *szUserDir;
char *szTempDir;
char *szCmdLine;
const char szNotice[] = "This is Neotron BBC BASIC\n";

extern char vflags;

// defined by the linker it's the stack top variable (End of ram)
extern unsigned long _stack_top;
// defined by the liker, this are just start and end marker for each section.
// .text (code)
extern unsigned long _start_text;
extern unsigned long _end_text;
// .data (data to be copied on ram)
extern unsigned long _start_data_flash;
extern unsigned long _start_data;
extern unsigned long _end_data;
// .bss (uninitialized data to set to 0);
extern unsigned long __bss_start__;
extern unsigned long __bss_end__;
// bottom of heap
extern char _heap_bottom;
// top of heap
extern char _heap_top;

extern int basic(void *ecx, void *edx, void *prompt);
extern void error(int code, const char *msg);

int main(void);

void rst_handler(void)
{
    // Copy the .data section pointers to ram from flash.
    // Look at LD manual (Optional Section Attributes).

    // source and destination pointers
    unsigned long *src;
    unsigned long *dest;

    // this should be good!
    src = &_start_data_flash;
    dest = &_start_data;

    // this too
    while (dest < &_end_data)
    {
        *dest++ = *src++;
    }

    // now set the .bss segment to 0!
    dest = &__bss_start__;
    while (dest < &__bss_end__)
    {
        *dest++ = 0;
    }

    // after setting copying .data to ram and "zero-ing" .bss we are good
    // to start the main() method!
    // There you go!
    main();

    /* Oops! You fell off the end of main. */
    while (1)
    {
        /* Spin */
    }
}

void empty_def_handler(void)
{
}

// NVIC ISR table
__attribute__((section(".nvic_table"))) unsigned long myvectors[] =
    {
        // This are the fixed priority interrupts and the stack pointer loaded at startup at R13 (SP).
        //                                              VECTOR N (Check Datasheet)
        // here the compiler it's boring.. have to figure that out
        (unsigned long)&_stack_top,       // stack pointer should be
                                          // placed here at startup.          0
        (unsigned long)rst_handler,       // code entry point                 1
        (unsigned long)empty_def_handler, // NMI handler.                     2
        (unsigned long)empty_def_handler, // hard fault handler.              3
                                          // Configurable priority interruts handler start here.
        (unsigned long)empty_def_handler, // Memory Management Fault          4
        (unsigned long)empty_def_handler, // Bus Fault                        5
        (unsigned long)empty_def_handler, // Usage Fault                      6
        0,                                // Reserved                         7
        0,                                // Reserved                         8
        0,                                // Reserved                         9
        0,                                // Reserved                         10
        (unsigned long)empty_def_handler, // SV call                          11
        (unsigned long)empty_def_handler, // Debug monitor                    12
        0,                                // Reserved                         13
        (unsigned long)empty_def_handler, // PendSV                           14
        (unsigned long)empty_def_handler, // SysTick                          15
};

void *sysadr(char *_name)
{
    return NULL;
}

// Wait for keypress:
unsigned char osrdch(void)
{
    unsigned char ch;
    while (read(1, &ch, 1) != 1)
    {
        // spin
    }
    return ch;
}

// Wait a maximum period for a keypress, or test key asynchronously:
int oskey(int wait)
{
    return -1;
}

// Handle OS command
void oscli(char *)
{
}

// Write to display or other output stream (VDU)
void oswrch(unsigned char ch)
{
    write(1, &ch, 1);
}

#define IOFLAG BIT0 // Insert/overtype
#define EGAFLG BIT1 // EGA-compatible modes (*EGA [ON])
#define CGAFLG BIT2 // CGA-compatible modes (*EGA OFF)
#define PTFLAG BIT3 // VDU 2 active
#define HRGFLG BIT4 // VDU 5 active
#define VDUDIS BIT5 // VDU 21 active
#define UFONT BIT6  // User font selected
#define UTF8 BIT7   // UTF-8 mode selected

// Read line of input
void osline(char *buffer)
{
    static char *history[HISTORY] = {NULL};
    static int empty = 0;
    int current = empty;
    char *eol = buffer;
    char *p = buffer;
    *buffer = 0x0D;
    int n;

    while (1)
    {
        unsigned char key;

        key = osrdch();
        switch (key)
        {
        case 0x0A:
        case 0x0D:
            n = (char *)memchr(buffer, 0x0D, 256) - buffer;
            if (n == 0)
                return;
            if ((current == (empty + HISTORY - 1) % HISTORY) &&
                (0 == memcmp(buffer, history[current], n)))
                return;
            history[empty] = malloc(n + 1);
            memcpy(history[empty], buffer, n + 1);
            empty = (empty + 1) % HISTORY;
            if (history[empty])
            {
                free(history[empty]);
                history[empty] = NULL;
            }
            return;

        case 8:
        case 127:
            if (p > buffer)
            {
                char *q = p;
                do
                    p--;
                while ((vflags & UTF8) && (*(signed char *)p < -64));
                oswrch(8);
                memmove(p, q, buffer + 256 - q);
            }
            break;

        case 21:
            while (p > buffer)
            {
                char *q = p;
                do
                    p--;
                while ((vflags & UTF8) && (*(signed char *)p < -64));
                oswrch(8);
                memmove(p, q, buffer + 256 - q);
            }
            break;

        case 130:
            while (p > buffer)
            {
                oswrch(8);
                do
                    p--;
                while ((vflags & UTF8) && (*(signed char *)p < -64));
            }
            break;

        case 131:
            while (*p != 0x0D)
            {
                oswrch(9);
                do
                    p++;
                while ((vflags & UTF8) && (*(signed char *)p < -64));
            }
            break;

        case 134:
            vflags ^= IOFLAG;
            if (vflags & IOFLAG)
                printf("\033[1 q");
            else
                printf("\033[3 q\033[7 q");
            break;

        case 135:
            if (*p != 0x0D)
            {
                char *q = p;
                do
                    q++;
                while ((vflags & UTF8) && (*(signed char *)q < -64));
                memmove(p, q, buffer + 256 - q);
            }
            break;

        case 136:
            if (p > buffer)
            {
                oswrch(8);
                do
                    p--;
                while ((vflags & UTF8) && (*(signed char *)p < -64));
            }
            break;

        case 137:
            if (*p != 0x0D)
            {
                oswrch(9);
                do
                    p++;
                while ((vflags & UTF8) && (*(signed char *)p < -64));
            }
            break;

        case 138:
        case 139:
            if (key == 138)
                n = (current + 1) % HISTORY;
            else
                n = (current + HISTORY - 1) % HISTORY;
            if (history[n])
            {
                char *s = history[n];
                while (*p != 0x0D)
                {
                    oswrch(9);
                    do
                        p++;
                    while ((vflags & UTF8) &&
                           (*(signed char *)p < -64));
                }
                while (p > buffer)
                {
                    oswrch(127);
                    do
                        p--;
                    while ((vflags & UTF8) &&
                           (*(signed char *)p < -64));
                }
                while ((*s != 0x0D) && (p < (buffer + 255)))
                {
                    oswrch(*s);
                    *p++ = *s++;
                }
                *p = 0x0D;
                current = n;
            }
            break;

        case 132:
        case 133:
        case 140:
        case 141:
            break;

        case 9:
            key = ' ';
        default:
            if (p < (buffer + 255))
            {
                if (key != 0x0A)
                {
                    oswrch(key);
                }
                if (key >= 32)
                {
                    memmove(p + 1, p, buffer + 255 - p);
                    *p++ = key;
                    *(buffer + 255) = 0x0D;
                    if ((*p != 0x0D) && (vflags & IOFLAG))
                    {
                        char *q = p;
                        do
                            q++;
                        while ((vflags & UTF8) &&
                               (*(signed char *)q < -64));
                        memmove(p, q, buffer + 256 - q);
                    }
                }
            }
        }

        if (*p != 0x0D)
        {
            oswrch(23);
            oswrch(1);
            for (n = 8; n != 0; n--)
                oswrch(0);
        }
        n = 0;
        while (*p != 0x0D)
        {
            oswrch(*p++);
            n++;
        }
        for (int i = 0; i < (eol - p); i++)
            oswrch(32);
        for (int i = 0; i < (eol - p); i++)
            oswrch(8);
        eol = p;
        while (n)
        {
            if (!(vflags & UTF8) || (*(signed char *)p >= -64))
                oswrch(8);
            p--;
            n--;
        }
        if (*p != 0x0D)
        {
            oswrch(23);
            oswrch(1);
            oswrch(1);
            for (n = 7; n != 0; n--)
                oswrch(0);
        }
    }
}

// Prepare for reporting an error
void reset(void) {}

// Test for ESCape
void trap(void) {}

// Load a file to memory
void osload(char *, void *, int) {}

// Save a file from memory
void ossave(char *, void *, int) {}

// Open a file
int osopen(int, char *)
{
    return -1;
}

// Read a byte from a file
unsigned char osbget(int, int *)
{
    return 0;
}

// Close file(s)
void osshut(int) {}

// No idea what this does
int osbyte(int al, int xy)
{
    error(255, "Sorry, not implemented");
    return -1;
}

// No idea what this does
void osword(int al, void *xy)
{
    error(255, "Sorry, not implemented");
    return;
}

// Write a byte:
void osbput(void *chan, unsigned char byte)
{
}

// Request memory allocation above HIMEM:
heapptr oshwm(void *addr, int settop)
{
    char dummy;
    if ((addr < userRAM) || (addr > (userRAM + sizeof(userRAMBuffer))) || (addr >= (void *)&dummy))
    {
        printf("Above MaximumRAM = %p\n", userRAM + sizeof(userRAMBuffer));
        error(0, NULL); // 'No room'
        return 0;
    }
    else
    {
        if (settop && (addr > userTOP))
        {
            userTOP = addr;
        }
        return (size_t)addr;
    }
}

// Call an emulated OS subroutine (if CALL or USR to an address < 0x10000)
int oscall(int addr)
{
    int al = stavar[1];
    void *xy = (void *)((size_t)stavar[24] | ((size_t)stavar[25] << 8));
    switch (addr)
    {
    case 0xFFE0: // OSRDCH
        return (int)osrdch();

    case 0xFFE3: // OSASCI
        if (al != 0x0D)
        {
            oswrch(al);
            return 0;
        }

    case 0xFFE7: // OSNEWL
        printf("\r\n");
        return 0;

    case 0xFFEE: // OSWRCH
        oswrch(al);
        return 0;

    case 0xFFF7: // OSCLI
        oscli(xy);
        return 0;

    default:
        error(8, NULL); // 'Address out of range'
    }
    return 0;
}

// Get file pointer:
long long getptr(void *chan)
{
    return 0;
}

// Set file pointer:
void setptr(void *chan, long long ptr)
{
}

// Get file size
long long getext(void *chan)
{
    return 0;
}

// Get EOF status:
long long geteof(void *chan)
{
    return 0;
}

int getims(void)
{
    strcpy(accs, "Mon.10 May 2023,09:10:30");
    return 24;
}

// Put event into event queue, unless full:
int putevt(heapptr handler, int msg, int wparam, int lparam)
{
    return -1;
}

// Get event from event queue, unless empty:
static heapptr getevt(void)
{
    return 0;
}

// Return centisecond count
int getime(void)
{
    return 0;
}

// Put TIME
void putime(int n)
{
}

// Wait for a specified number of centiseconds:
// On some platforms specifying a negative value causes a task switch
void oswait(int cs)
{
}

// ADVAL(n)
int adval(int n)
{
    error(255, "Sorry, not implemented");
    return -1;
}

// Get string width in graphics units:
int widths(unsigned char *s, int l)
{
    error(255, "Sorry, not implemented");
    return -1;
}

// Get nearest palette index:
int vpoint(int x, int y)
{
    error(255, "Sorry, not implemented");
    return -1;
}

int vgetc(int x, int y)
{
    error(255, "Sorry, not implemented");
    return -1;
}

// ENVELOPE N,T,PI1,PI2,PI3,PN1,PN2,PN3,AA,AD,AS,AR,ALA,ALD
void envel(signed char *env)
{
}

// Disable sound generation:
void quiet(void)
{
}

// Get pixel RGB colour:
int vtint(int x, int y)
{
    error(255, "Sorry, not implemented");
    return -1;
}

// SOUND Channel,Amplitude,Pitch,Duration
void sound(short chan, signed char ampl, unsigned char pitch, unsigned char duration)
{
}

// Get text cursor (caret) coordinates:
void getcsr(int *px, int *py)
{
    if (px != NULL)
        *px = -1; // Flag unable to read
    if (py != NULL)
        *py = -1;
}

// Test for escape, kill, pause, single-step, flash and alert:
heapptr xtrap(void)
{
    trap();
    if (flags & ALERT)
    {
        return getevt();
    }
    return 0;
}

// Report a 'fatal' error:
void faterr(const char *msg)
{
    error(512, "");
}

// MOUSE x%, y%, b%
void mouse(int *px, int *py, int *pb)
{
    if (px)
        *px = 0;
    if (py)
        *py = 0;
    if (pb)
        *pb = 0;
}

// MOUSE ON [type]
void mouseon(int type)
{
}

// MOUSE OFF
void mouseoff(void)
{
}

// MOUSE TO x%, y%
void mouseto(int x, int y)
{
}

long long apicall_(long long (*APIfunc)(size_t, size_t, size_t, size_t, size_t, size_t,
                                        size_t, size_t, size_t, size_t, size_t, size_t,
                                        double, double, double, double, double, double, double, double),
                   PARM *p)
{
    return APIfunc(p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
                   p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11],
                   p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]);
}

// Call a function in the context of the GUI thread:
size_t guicall(void *func, PARM *parm)
{
    return apicall_(func, parm);
}

double fltcall_(double (*APIfunc)(size_t, size_t, size_t, size_t, size_t, size_t,
                                  size_t, size_t, size_t, size_t, size_t, size_t,
                                  double, double, double, double, double, double, double, double),
                PARM *p)
{
    return APIfunc(p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
                   p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11],
                   p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]);
}

// Dummy functions:
void gfxPrimitivesSetFont(void){};
void gfxPrimitivesGetFont(void){};
void RedefineChar(void){};

// newlib stuff
extern int _end;

void *_sbrk(int incr)
{
    static char *heap_end = 0;
    char *prev_heap_end;

    if (heap_end == 0)
    {
        heap_end = &_heap_bottom;
    }
    prev_heap_end = heap_end;
    if ((heap_end + incr) > &_heap_top)
    {
        return NULL;
    }

    heap_end += incr;
    return prev_heap_end;
}

int _close(int file)
{
    return -1;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;

    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

void _exit(int status)
{
    for (;;)
    {
        __asm("BKPT #0");
    }
}

void _kill(int pid, int sig)
{
    return;
}

int _getpid(void)
{
    return -1;
}

int _write(int file, char *ptr, int len)
{
    if (file == 1)
    {
        for (int idx = 0; idx < len; idx++)
        {
            while ((*UART0_STATUS & 1) != 0)
            {
                // spin because UART full
            }
            *UART0_BASE = ptr[idx];
        }
        return len;
    }
    else
    {
        return -1;
    }
}

int _read(int file, char *ptr, int len)
{
    if (file == 1)
    {
        for (int idx = 0; idx < len; idx++)
        {
            if ((*UART0_STATUS & 2) == 0)
            {
                // UART is empty
                return idx;
            }
            ptr[idx] = *UART0_BASE;
        }
        return len;
    }
    else
    {
        return -1;
    }
}

int main(void)
{
    // Init uart
    *UART0_BAUDDIV = 25000000 / 115200;
    *UART0_CONTROL = 3;

    char *userTOP = userRAM + sizeof(userRAMBuffer);
    char *progRAM = userRAM + PAGE_OFFSET;
    basic(progRAM, userTOP, 0);
    return 0;
}
