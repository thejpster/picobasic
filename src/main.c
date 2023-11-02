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

#include "neotron.h"

#include "bbccon.h"

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
const char szNotice[] = "(C) Copyright R. T. Russell, " YEAR;
const char szVersion[] = "BBC BASIC for Neotron version " VERSION;
static NeotronApi *g_api = NULL;

static char *heap_end = 0;

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

/// How much heap is being used right now
int heap_used = 0;

extern int basic(void *ecx, void *edx, void *prompt);
extern void error(int code, const char *msg);

void *sysadr(char *name)
{
    printf("sysadr(%s)\n", name);
    return NULL;
}

// Wait for keypress:
unsigned char osrdch(void)
{
    unsigned char ch;
    while (read(0, &ch, 1) != 1)
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
void oscli(char *command)
{
    if (strcmp(command, "INFO\r") == 0)
    {
        puts("INFO:");
        printf("    heap_used = %d\n", (int)heap_used);
        printf("userRAMBuffer = %p\n", userRAMBuffer);
        printf("      userRAM = %p\n", userRAM);
        printf("      userTOP = %p\n", userTOP);
    }
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
void reset(void)
{
    printf("reset()\n");
}

// Test for ESCape
void trap(void)
{
#ifdef DEBUG_OUTPUT
    printf("trap()\n");
#endif
}

// Load a file to memory
void osload(char *name, void *buffer, int len)
{
    printf("osload(%s, %p, %d)\n", name, buffer, len);
}

// Save a file from memory
void ossave(char *name, void *buffer, int len)
{
    printf("ossave(%s, %p, %d)\n", name, buffer, len);
}

// Open a file
int osopen(int x, char *y)
{
    printf("osopen(%d, %s)\n", x, y);
    return -1;
}

// Read a byte from a file
unsigned char osbget(int x, int *y)
{
    printf("osbget(%d, %p)\n", x, y);
    return 0;
}

// Close file(s)
void osshut(int x)
{
    printf("osshut(%d)\n", x);
}

// No idea what this does
int osbyte(int al, int xy)
{
    printf("osbyte(%d, %d)\n", al, xy);
    error(255, "Sorry, not implemented");
    return -1;
}

// No idea what this does
void osword(int al, void *xy)
{
    printf("osword(%d, %p)\n", al, xy);
    error(255, "Sorry, not implemented");
    return;
}

// Write a byte:
void osbput(void *chan, unsigned char byte)
{
    printf("osbyte(%p, %d)\n", chan, byte);
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
        printf("oshwm(%p, %d) -> %p\n", addr, settop, addr);
        return (size_t)addr;
    }
}

// Call an emulated OS subroutine (if CALL or USR to an address < 0x10000)
int oscall(int addr)
{
    printf("oscall(%d)\n", addr);
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
    printf("putevt(%p, %d, %d, %d)\n", (void *)handler, msg, wparam, lparam);
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
    printf("Fatal error: %s\n", msg);
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

void *_sbrk(int incr)
{
#define HEAP_SIZE 4096
    static char HEAP_MEMORY[HEAP_SIZE] = {0};
    if (heap_end == 0)
    {
        heap_end = HEAP_MEMORY;
    }
    char *prev_heap_end = heap_end;
    if ((heap_end + incr) > (HEAP_MEMORY + HEAP_SIZE))
    {
        __asm("bkpt");
        return NULL;
    }

    heap_end += incr;

    heap_used = heap_end - HEAP_MEMORY;

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
        Handle fd = {._0 = 1};
        FfiByteSlice buffer = {.data = (const uint8_t *)ptr, .data_len = len};
        FfiResult_void result = g_api->write(fd, buffer);
        if (result.tag == FfiResult_Ok)
        {
            return len;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

int _read(int file, char *ptr, int len)
{
    if (file == 0)
    {
        Handle fd = {._0 = 0};
        FfiBuffer buffer = {.data = (uint8_t *)ptr, .data_len = len};
        FfiResult_usize result = g_api->read(fd, buffer);
        if (result.tag == FfiResult_Ok)
        {
            return result.ok;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

void app_entry(NeotronApi *api)
{
    // Turn off newlib output buffering
    setbuf(stdout, NULL);
    g_api = api;

    accs = (char *)userRAM;           // String accumulator
    buff = (char *)accs + ACCSLEN;    // Temporary string buffer
    path = (char *)buff + 0x100;      // File path
    keystr = (char **)(path + 0x100); // *KEY strings
    keybdq = (char *)keystr + 0x100;  // Keyboard queue
    eventq = (void *)keybdq + 0x100;  // Event queue
    filbuf[0] = (eventq + 0x200 / 4); // File buffers n.b. pointer arithmetic!!

    farray = 1;                         // @hfile%() number of dimensions
    fasize = MAX_PORTS + MAX_FILES + 4; // @hfile%() number of elements

    vflags = UTF8; // Not |= (fails on Linux build)

    memset(keystr, 0, 256);
    spchan = NULL;
    exchan = NULL;

    puts(szVersion);
    puts(szNotice);

    userTOP = userRAM + sizeof(userRAMBuffer);
    void *progRAM = userRAM + PAGE_OFFSET; // Will be raised if @cmd$ exceeds 255 bytes
    basic(progRAM, userTOP, 0);
}
