/************************************************************************
*************************************************************************
**  program "pmon.c" for NEWS/SPARC                             *********
**  plot ddr system monitor data                                *********
**  7/19/89 - 5/22/90 urabe (PC-9801 version)                   *********
**  5/24/90 - 9/26/91 urabe (NEWS version)                      *********
**  4/27/92 - 4/27/92 urabe (86 ch -> 103 ch max.)              *********
**  5/25/92 - 5/25/92 urabe (touch "off" file)                  *********
**  1/14/93 - 1/14/93 for RISC NEWS                             *********
**  2/24/93 - 3/27/93 for SUN SPARC                             *********
**  4/25/93 - 7/12/93 for new data format                       *********
**  2/ 7/94 - 2/ 7/94 increased channels (#define MAXCH 1)      *********
**  3/15/94 - 3/15/94 fixed bug of long trigger for absent      *********
**  channel                                                     *********
**  5/16/94 - 5/16/94 fixed bug for illegal data file           *********
**  5/19/94 - 5/19/94 report no-data stations to syslog         *********
**  5/25/94 - 5/25/94 directory for just signal files           *********
**  9/17/94 - 9/17/94 TIME_TOO_LOW/DEBUG                        *********
**  10/11/94 syslog deleted                                     *********
**  12/16/94 bug fix(read USED directory)                       *********
**  1/6/95 bug fix(adj_time(tm[0]--))                           *********
**  2/23/96-2/28/96 MAXCH deleted and lock file introduced      *********
**  8/21/96 - 8/22/96 each station can belong upto 10 zones     *********
**  97.8.21   allows empty line in zones.tbl                    *********
**  97.10.1   FreeBSD                                           *********
**  97.10.21  use PID in lock file                              *********
**  98.1.22   not use lpr -s if defined(__FreeBSD__)            *********
**  98.5.12   use lp in __SVR4(Solaris2.X)                      *********
**  98.6.24   yo2000                                            *********
**  98.6.29   eliminate get_time()                              *********
**  99.2.3    confirm_off() fixed (cnt_zone=0 in if rep_level)  *********
**  99.2.4    put signal(HUP) in hangup()                       *********
**                                                              *********
**  font files ("font16", "font24" and "font32") are            *********
**  not necessary                                               *********
**   link with "-lm" option                                     *********
**                                                              *********
**  rep_level = 0: no report, 1: begins and ends                *********
**              2: groups,    3: channels                       *********
*************************************************************************/

#include  <stdio.h>
#include  <string.h>
#include  <math.h>
#include  <signal.h>
#include  <sys/types.h>
#include  <sys/file.h>
#include  <sys/time.h>
#include  <fcntl.h>
#include  <sys/ioctl.h>
#include  <ctype.h>

#define DEBUG         0
#define M_CH          1000   /* absolute max n of traces */
                             /* n of chs in data file is unlimited */
#define MAX_ZONES     10     /* max n of zones for a station */
#define MIN_PER_LINE  10     /* min/line */
#define WIDTH_LBP     392    /* in bytes (must be even) */
#define HEIGHT_LBP    4516   /* in pixels */
#define LENGTH        200000 /* buffer size */
#define SR_MON        5
#define TOO_LOW       0.0
#define TIME_TOO_LOW  10.0

#define X_BASE        136
#define Y_BASE        216
#define Y_SCALE       12
#define Y_SPACE       50
#define HEIGHT_FONT16 16
#define HEIGHT_FONT24 24
#define HEIGHT_FONT32 32
#define WIDTH_FONT16  8
#define WIDTH_FONT24  12
#define WIDTH_FONT32  16
#define CODE_START    32
#define CODE_END      126
#define N_CODE        (CODE_END-CODE_START+1)
#define SIZE_FONT16   ((WIDTH_FONT16*N_CODE+15)/16*2*HEIGHT_FONT16)
  /* 96*16 */
#define SIZE_FONT24   ((WIDTH_FONT24*N_CODE+15)/16*2*HEIGHT_FONT24)
#define SIZE_FONT32   ((WIDTH_FONT32*N_CODE+15)/16*2*HEIGHT_FONT32)
#define PLOTHEIGHT    (HEIGHT_LBP-Y_BASE-HEIGHT_FONT32)

/*****************************/
/* 8/21/96 for little-endian (uehira) */
#ifndef         LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234    /* LSB first: i386, vax */
#endif
#ifndef         BIG_ENDIAN
#define BIG_ENDIAN      4321    /* MSB first: 68000, ibm, net */
#endif
#ifndef  BYTE_ORDER
#define  BYTE_ORDER      BIG_ENDIAN
#endif

#define SWAPU  union { long l; float f; short s; char c[4];} swap
#define SWAPL(a) swap.l=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPF(a) swap.f=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPS(a) swap.s=(a); ((char *)&(a))[0]=swap.c[1];\
    ((char *)&(a))[1]=swap.c[0]
/*****************************/

/* (8 x 16) x (128-32) */
/* start = 32, end = 126, n of codes = 126-32+1 = 95 */
/* bitmap of 16(tate) x 96(yoko) bytes */
  unsigned char font16[SIZE_FONT16]={
0x00,0x00,0x6c,0x00,0x10,0x02,0x00,0xe0,0x02,0x80,0x00,0x00,0x00,0x00,0x00,0x02,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x80,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x00,0xf0,0x10,0x00,
0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x06,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x10,0xc0,0xfe,0x00,
0x00,0x38,0x6c,0x12,0x38,0x62,0x30,0xe0,0x04,0x40,0x00,0x00,0x00,0x00,0x00,0x02,
0x18,0x10,0x18,0x38,0x08,0xfc,0x3c,0xfe,0x38,0x38,0x00,0x00,0x04,0x00,0x40,0x38,
0x3c,0x10,0xf8,0x3a,0xf8,0xfe,0xfe,0x1a,0xe7,0xfe,0x1f,0xe6,0xf0,0x82,0x87,0x38,
0xf8,0x38,0xf8,0x34,0xfe,0xe7,0xc6,0xc6,0xee,0xc6,0xfe,0x10,0xc6,0x10,0x28,0x00,
0x30,0x00,0xc0,0x00,0x06,0x00,0x0e,0x00,0xc0,0x18,0x06,0xc0,0x78,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00,
0x00,0x38,0x24,0x12,0x54,0x94,0x48,0x20,0x08,0x20,0x00,0x00,0x00,0x00,0x00,0x04,
0x24,0x70,0x24,0x44,0x18,0x80,0x42,0x82,0x44,0x44,0x00,0x00,0x04,0x00,0x40,0x44,
0x42,0x28,0x44,0x46,0x44,0x42,0x42,0x26,0x42,0x10,0x02,0x44,0x40,0xc6,0xc2,0x44,
0x44,0x44,0x44,0x4c,0x92,0x42,0x82,0x82,0x44,0x82,0x84,0x10,0x82,0x10,0x44,0x00,
0x20,0x00,0x40,0x00,0x04,0x00,0x11,0x00,0x40,0x00,0x00,0x40,0x08,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00,
0x00,0x38,0x24,0x12,0x92,0x94,0x48,0x20,0x08,0x20,0x10,0x10,0x00,0x00,0x00,0x04,
0x24,0x10,0x42,0x82,0x28,0x80,0x46,0x82,0x82,0x82,0x00,0x00,0x08,0x00,0x20,0x82,
0x82,0x28,0x42,0x42,0x44,0x42,0x42,0x42,0x42,0x10,0x02,0x44,0x40,0xaa,0xa2,0x82,
0x42,0x44,0x42,0x84,0x92,0x42,0x82,0x82,0x44,0x44,0x88,0x10,0x44,0x10,0x82,0x00,
0x20,0x00,0x40,0x00,0x04,0x00,0x10,0x00,0x40,0x00,0x00,0x40,0x08,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00,
0x00,0x38,0x48,0x7f,0x96,0x94,0x48,0xc0,0x10,0x10,0x38,0x10,0x00,0x00,0x00,0x08,
0x42,0x10,0x62,0x82,0x28,0x80,0x80,0x04,0x82,0x82,0x00,0x00,0x08,0x00,0x20,0xc2,
0x9a,0x28,0x42,0x80,0x42,0x40,0x40,0x40,0x42,0x10,0x02,0x48,0x40,0xaa,0xa2,0x82,
0x42,0x82,0x42,0x80,0x10,0x42,0x82,0x82,0x28,0x44,0x08,0x10,0x44,0x10,0x00,0x00,
0x10,0x00,0x40,0x00,0x04,0x00,0x10,0x00,0x40,0x00,0x00,0x40,0x08,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x20,0x00,0x00,
0x00,0x38,0x00,0x24,0x90,0x98,0x50,0x00,0x10,0x10,0x92,0x10,0x00,0x00,0x00,0x08,
0x42,0x10,0x02,0x02,0x48,0xb8,0x80,0x04,0x82,0x82,0x38,0x38,0x10,0xfe,0x10,0x02,
0xa6,0x44,0x42,0x80,0x42,0x48,0x48,0x80,0x42,0x10,0x02,0x48,0x40,0xaa,0xa2,0x82,
0x42,0x82,0x42,0x80,0x10,0x42,0x82,0x92,0x28,0x44,0x10,0x10,0x44,0x10,0x00,0x00,
0x00,0x3c,0x78,0x3a,0x3c,0x38,0xfe,0x3b,0x5c,0x78,0x3e,0x42,0x08,0x6c,0xdc,0x38,
0xf8,0x3e,0xec,0x3a,0xfc,0xc6,0xc6,0x92,0xee,0xe7,0x7e,0x08,0x10,0x20,0x00,0x00,
0x00,0x38,0x00,0x24,0x50,0x68,0x20,0x00,0x10,0x10,0xd6,0x10,0x00,0x00,0x00,0x08,
0x42,0x10,0x04,0x04,0x48,0xc4,0xb8,0x04,0x44,0x82,0x38,0x38,0x10,0x00,0x10,0x04,
0xa2,0x44,0x44,0x80,0x42,0x48,0x48,0x80,0x7e,0x10,0x02,0x70,0x40,0x92,0x92,0x82,
0x42,0x82,0x44,0x60,0x10,0x42,0x44,0x92,0x10,0x28,0x10,0x10,0x28,0x10,0x00,0x00,
0x00,0x42,0x44,0x46,0x44,0x44,0x10,0x44,0x62,0x08,0x02,0x44,0x08,0x92,0x62,0x44,
0x44,0x44,0x32,0x46,0x20,0x42,0x82,0x92,0x44,0x42,0x44,0x10,0x10,0x10,0x00,0x00,
0x00,0x10,0x00,0x24,0x38,0x10,0x2e,0x00,0x10,0x10,0x38,0xfe,0x00,0xfe,0x00,0x10,
0x42,0x10,0x08,0x38,0x88,0x82,0xc4,0x08,0x38,0x46,0x00,0x00,0x20,0x00,0x08,0x04,
0xa2,0x44,0x78,0x80,0x42,0x78,0x78,0x8f,0x42,0x10,0x02,0x50,0x40,0x92,0x92,0x82,
0x44,0x82,0x78,0x18,0x10,0x42,0x44,0x92,0x28,0x28,0x10,0x10,0xfe,0x10,0x00,0x00,
0x00,0x02,0x42,0x82,0x84,0x82,0x10,0x44,0x42,0x08,0x02,0x48,0x08,0x92,0x42,0x82,
0x42,0x84,0x22,0x42,0x20,0x42,0x82,0x92,0x28,0x22,0x08,0x20,0x10,0x08,0x00,0x00,
0x00,0x10,0x00,0x24,0x14,0x10,0x54,0x00,0x10,0x10,0xd6,0x10,0x00,0x00,0x00,0x10,
0x42,0x10,0x08,0x04,0x88,0x02,0x82,0x08,0x44,0x3a,0x00,0x00,0x20,0x00,0x08,0x08,
0xa2,0x44,0x44,0x80,0x42,0x48,0x48,0x82,0x42,0x10,0x02,0x48,0x40,0x92,0x92,0x82,
0x78,0x82,0x48,0x04,0x10,0x42,0x44,0xaa,0x28,0x10,0x20,0x10,0x10,0x10,0x00,0x00,
0x00,0x3e,0x42,0x80,0x84,0xfe,0x10,0x44,0x42,0x08,0x02,0x58,0x08,0x92,0x42,0x82,
0x42,0x84,0x20,0x40,0x20,0x42,0x44,0x92,0x28,0x24,0x08,0x10,0x10,0x10,0x00,0x00,
0x00,0x10,0x00,0x24,0x12,0x2c,0x54,0x00,0x10,0x10,0x92,0x10,0x00,0x00,0x00,0x10,
0x42,0x10,0x10,0x02,0xfe,0x02,0x82,0x08,0x82,0x02,0x00,0x00,0x10,0xfe,0x10,0x10,
0xa6,0x7c,0x42,0x80,0x42,0x48,0x48,0x82,0x42,0x10,0x82,0x48,0x40,0x82,0x8a,0x82,
0x40,0x82,0x44,0x82,0x10,0x42,0x44,0xaa,0x28,0x10,0x20,0x10,0xfe,0x10,0x00,0x00,
0x00,0x42,0x42,0x80,0x84,0x80,0x10,0x38,0x42,0x08,0x02,0x64,0x08,0x92,0x42,0x82,
0x42,0x84,0x20,0x3c,0x20,0x42,0x44,0xaa,0x10,0x14,0x10,0x08,0x10,0x20,0x00,0x00,
0x00,0x10,0x00,0xfe,0xd2,0x32,0x94,0x00,0x10,0x10,0x38,0x10,0x00,0x00,0x00,0x20,
0x42,0x10,0x20,0x82,0x08,0xc2,0x82,0x08,0x82,0x02,0x00,0x00,0x10,0x00,0x10,0x10,
0x9a,0x82,0x42,0x82,0x42,0x42,0x40,0x82,0x42,0x10,0x82,0x44,0x42,0x82,0x8a,0x82,
0x40,0xba,0x44,0x82,0x10,0x42,0x28,0xaa,0x44,0x10,0x42,0x10,0x10,0x10,0x00,0x00,
0x00,0x82,0x42,0x80,0x84,0x80,0x10,0x40,0x42,0x08,0x02,0x44,0x08,0x92,0x42,0x82,
0x42,0x84,0x20,0x02,0x20,0x42,0x44,0xaa,0x28,0x08,0x10,0x08,0x10,0x20,0x00,0x00,
0x00,0x00,0x00,0x48,0x92,0x52,0x88,0x00,0x10,0x10,0x10,0x10,0xe0,0x00,0x40,0x20,
0x42,0x10,0x22,0x82,0x08,0x82,0x82,0x10,0x82,0x82,0x00,0x38,0x08,0x00,0x20,0x00,
0x80,0x82,0x42,0x42,0x44,0x42,0x40,0x42,0x42,0x10,0x82,0x44,0x42,0x82,0x8a,0x82,
0x40,0x44,0x44,0x82,0x10,0x42,0x28,0x44,0x44,0x10,0x42,0x10,0x10,0x10,0x00,0x00,
0x00,0x82,0x42,0x82,0x84,0x82,0x10,0x78,0x42,0x08,0x02,0x42,0x08,0x92,0x42,0x82,
0x44,0x44,0x20,0x82,0x22,0x42,0x28,0x44,0x28,0x08,0x22,0x08,0x10,0x20,0x00,0x00,
0x00,0x00,0x00,0x48,0x94,0x52,0x8c,0x00,0x08,0x20,0x00,0x00,0xe0,0x00,0xe0,0x40,
0x24,0x10,0x42,0x44,0x08,0x44,0x44,0x10,0x44,0x44,0x38,0x38,0x08,0x00,0x20,0x00,
0x42,0x82,0x42,0x42,0x44,0x42,0x40,0x66,0x42,0x10,0x44,0x42,0x42,0x82,0x86,0x44,
0x40,0x44,0x42,0xc4,0x10,0x42,0x10,0x44,0x82,0x10,0x82,0x10,0x10,0x10,0x00,0x00,
0x00,0x86,0x44,0x42,0x44,0x42,0x10,0x84,0x42,0x08,0x82,0x42,0x08,0x92,0x42,0x44,
0x78,0x3c,0x20,0xc2,0x22,0x46,0x28,0x44,0x44,0x10,0x42,0x08,0x10,0x20,0x00,0x00,
0x00,0x10,0x00,0x48,0x78,0x52,0x72,0x00,0x08,0x20,0x00,0x00,0x20,0x00,0xe0,0x40,
0x24,0x7c,0x7e,0x38,0x3c,0x38,0x38,0x10,0x38,0x38,0x38,0x18,0x04,0x00,0x40,0x10,
0x3c,0xc6,0xfc,0x3c,0xf8,0xfe,0xf0,0x1a,0xe7,0xfe,0x38,0xe3,0xfe,0xc6,0xc2,0x38,
0xf0,0x38,0xe3,0xb8,0x7c,0x3c,0x10,0x44,0xc6,0x7c,0xfe,0x10,0x7c,0x10,0x00,0x00,
0x00,0x7b,0x78,0x3c,0x3e,0x3c,0x7c,0x82,0xe7,0xff,0x82,0xe3,0xff,0xdb,0xe7,0x38,
0x40,0x04,0xfc,0xbc,0x1c,0x39,0x10,0x44,0xee,0x90,0xfe,0x08,0x10,0x20,0x00,0x00,
0x00,0x38,0x00,0x48,0x10,0x8c,0x00,0x00,0x04,0x40,0x00,0x00,0x20,0x00,0x40,0x80,
0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x10,0x04,0x00,0x40,0x38,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x10,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x00,0x00,0x44,0x00,0x00,0x00,0x00,0x00,
0x40,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0x00,0x08,0x10,0x20,0x00,0x00,
0x00,0x10,0x00,0x00,0x10,0x80,0x00,0x00,0x02,0x80,0x00,0x00,0xc0,0x00,0x00,0x80,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x02,0x00,0x80,0x10,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x00,0xf0,0x00,0xfe,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7c,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x00,
0xf0,0x1e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x06,0x10,0xc0,0x00,0x00};

int fd,n_ch,min_trig[M_CH],tim[6],n_zone,n_trig[M_CH],n_stn[M_CH],
  max_trig[M_CH],rep_level,n_zone_trig,cnt_zone,req_print,
  i_zone[M_CH],n_min_trig,not_yet,m_ch,ppt_half,m_limit,made_lock_file,
  min_per_sheet,pixels_per_trace,n_rows,max_ch,max_ch_flag;
long long_max[M_CH][SR_MON],long_min[M_CH][SR_MON];
short idx[65536];
unsigned char frame[HEIGHT_LBP][WIDTH_LBP],zone[M_CH][20],
  file_trig[80],param_file[80],buf[LENGTH],file_zone[80],
  font24[SIZE_FONT24],font32[SIZE_FONT32],last_line[400],
  temp_done[80],line[400],latest[80],ch_file[80],time_text[20],
  file_trig_lock[80],idx2[65536];
double dt=1.0/(double)SR_MON,time_on,time_off,time_lta,time_lta_off,
  time_sta,a_sta,b_sta,a_lta,b_lta,a_lta_off,b_lta_off;

struct
  {
  int ch;
  int gain;
  unsigned char name[6];
  unsigned char comp[4];
  int zone[MAX_ZONES];
  int n_zone;
  int alive;       /* 0:alive 1:dead */
  int use;         /* 0:unuse 1:use */
  int status;      /* 0:OFF, 1:ON but not comfirmed */
                   /* 2:ON confirmed, 3:OFF but not confirmed */
  double lta;      /* long term average */
  double sta;      /* short term average */
  double ratio;    /* sta/lta ratio */
  double lta_save; /* lta just before trigger */
  long max;        /* maximum deflection */
  double sec_on;   /* duration time */
  double sec_off;  /* time after on->off */
  double cnt;      /* counter for initialization */
  double sec_zero; /* length of successive zeros */
  int tm[7];       /* year to sec/10 */
  } tbl[M_CH];

strncmp2(s1,s2,i)
char *s1,*s2;             
int i; 
{
  if(*s1=='0' && *s2=='9') return 1;
  else if(*s1=='9' && *s2=='0') return -1;
  else return strncmp(s1,s2,i);
}

strcmp2(s1,s2)        
char *s1,*s2;
{
  if(*s1=='0' && *s2=='9') return 1;
  else if(*s1=='9' && *s2=='0') return -1;
  else return strcmp(s1,s2);
}

mklong(ptr)
  unsigned char *ptr;
  {
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPU;
#endif
  unsigned char *ptr1;
  unsigned long a;
  ptr1=(unsigned char *)&a;
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1  =(*ptr);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(a);
#endif
  return a;
  }

mkfont(y,ii,buf_font,width,jj,k,buf_font16,width16)
  int y,ii,k,width,jj,width16;
  unsigned char *buf_font;
  unsigned short *buf_font16;
  {
  int i,j,x;
  union { long l; char c[4]; } u;
  static unsigned long
    bits[2][16]={0xc0000000,0x60000000,0x18000000,0x0c000000,
          0x03000000,0x01800000,0x00600000,0x00300000,
          0x000c0000,0x00060000,0x00018000,0x0000c000,
          0x00003000,0x00001800,0x00000600,0x00000300,
          0xc0000000,0x30000000,0x0c000000,0x03000000,
          0x00c00000,0x00300000,0x000c0000,0x00030000,
          0x0000c000,0x00003000,0x00000c00,0x00000300,
          0x000000c0,0x00000030,0x0000000c,0x00000003};
  static unsigned short bit_mask[16]={0x8000,0x4000,0x2000,0x1000,
          0x800,0x400,0x200,0x100,0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};
#if BYTE_ORDER == LITTLE_ENDIAN
  unsigned short s;
  SWAPU;
#endif

  for(j=0;j<width*2;j++) buf_font[ii*width*2+j]=0;
  j=0;
  for(x=0;x<width16;x++)
    {
#if BYTE_ORDER == BIG_ENDIAN
    u.c[0]=buf_font[ii*width*2+j];
    u.c[1]=buf_font[ii*width*2+j+1];
    u.c[2]=buf_font[ii*width*2+j+2];
    u.c[3]=buf_font[ii*width*2+j+3];
    for(i=0;i<16;i++) if(buf_font16[y*width16+x]&bit_mask[i]) u.l|=bits[k][i];
    buf_font[ii*width*2+j]  =u.c[0];
    buf_font[ii*width*2+j+1]=u.c[1];
    buf_font[ii*width*2+j+2]=u.c[2];
    buf_font[ii*width*2+j+3]=u.c[3];
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
    u.c[3]=buf_font[ii*width*2+j];
    u.c[2]=buf_font[ii*width*2+j+1];
    u.c[1]=buf_font[ii*width*2+j+2];
    u.c[0]=buf_font[ii*width*2+j+3];
    s=buf_font16[y*width16+x];
    SWAPS(s);
    for(i=0;i<16;i++) if(s&bit_mask[i]) u.l|=bits[k][i];
    buf_font[ii*width*2+j]  =u.c[3];
    buf_font[ii*width*2+j+1]=u.c[2];
    buf_font[ii*width*2+j+2]=u.c[1];
    buf_font[ii*width*2+j+3]=u.c[0];
#endif
    j+=jj;
    }
  }

make_fonts(buf_font16,buf_font24,buf_font32)
  unsigned char *buf_font16,*buf_font24,*buf_font32;
  {
  int ii,y,width16,width24,width32;
  width16=(WIDTH_FONT16*N_CODE+15)/16;
  width24=(WIDTH_FONT24*N_CODE+15)/16;
  width32=(WIDTH_FONT32*N_CODE+15)/16;
/* make buf_font24 */
  ii=0;
  for(y=0;y<HEIGHT_FONT16;y++)
    {
    mkfont(y,ii++,buf_font24,width24,3,0,(unsigned short *)buf_font16,width16);
    mkfont(y,ii,buf_font24,width24,3,0,(unsigned short *)buf_font16,width16);
    y++;
    mkfont(y,ii++,buf_font24,width24,3,0,(unsigned short *)buf_font16,width16);
    mkfont(y,ii++,buf_font24,width24,3,0,(unsigned short *)buf_font16,width16);
    }

/* make buf_font32 */
  ii=0;
  for(y=0;y<HEIGHT_FONT16;y++)
    {
    mkfont(y,ii++,buf_font32,width32,4,1,(unsigned short *)buf_font16,width16);
    mkfont(y,ii++,buf_font32,width32,4,1,(unsigned short *)buf_font16,width16);
    }
  }

time_to_sec(tts_yy,tts_mm,tts_dd,tts_h,tts_m,tts_s)
  int tts_yy,tts_mm,tts_dd,tts_h,tts_m,tts_s;
  {
  static int days_in_month[13]=
      {0,31,28,31,30,31,30,31,31,30,31,30,31};
  int tts_iy,tts_im;
  tts_dd--;
  for(tts_iy=90;tts_iy<tts_yy;tts_iy++)
    {
    if(tts_iy%4) tts_dd+=365;
    else tts_dd+=366;
    }
  for(tts_im=1;tts_im<tts_mm;tts_im++)
    {
    tts_dd+=days_in_month[tts_im];
    if(tts_im==2 && tts_yy%4==0) tts_dd++;
    }
  return ((tts_dd*24+tts_h)*60+tts_m)*60+tts_s;
  }

write_file(fname,text)
  unsigned char *fname,*text;
  {
  FILE *fp;
  while((fp=fopen(fname,"a"))==NULL)
    {
    printf("pmon:'%s' not open (%d)",fname,getpid());
    sleep(60);
    }
  fputs(text,fp);
  while(fclose(fp)==EOF)
    {
    printf("pmon:'%s' not close (%d)",fname,getpid());
    sleep(60);
    }
  }

adj_time(tm)
  int *tm;
  {
  if(tm[5]==60)
    {
    tm[5]=0;
    if(++tm[4]==60)
      {
      tm[4]=0;
      if(++tm[3]==24)
        {
        tm[3]=0;
        tm[2]++;
        switch(tm[1])
          {
          case 2:
            if(tm[0]%4==0)
              {
              if(tm[2]==30)
                {
                tm[2]=1;
                tm[1]++;
                }
              break;
              }
            else
              {
              if(tm[2]==29)
                {
                tm[2]=1;
                tm[1]++;
                }
              break;
              }
          case 4:
          case 6:
          case 9:
          case 11:
            if(tm[2]==31)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
          default:
            if(tm[2]==32)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
          }
        if(tm[1]==13)
          {
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
          }
        }
      }
    }
  else if(tm[5]==-1)
    {
    tm[5]=59;
    if(--tm[4]==-1)
      {
      tm[4]=59;
      if(--tm[3]==-1)
        {
        tm[3]=23;
        if(--tm[2]==0)
          {
          switch(--tm[1])
            {
            case 2:
              if(tm[0]%4==0)
                tm[2]=29;else tm[2]=28;
              break;
            case 4:
            case 6:
            case 9:
            case 11:
              tm[2]=30;
              break;
            default:
              tm[2]=31;
              break;
            }
          if(tm[1]==0)
            {
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
            }
          }
        }
      }
    }
  }

confirm_on(ch)
  int ch;
  {
  int i,j,tm[6],z;
  static char prev_on[15];
  tbl[ch].status=2;
  sprintm(time_text,tbl[ch].tm);
  sprintf(time_text+strlen(time_text),".%d",tbl[ch].tm[6]*(10/SR_MON));

  for(j=0;j<tbl[ch].n_zone;j++)
    {
    z=tbl[ch].zone[j];
    max_trig[z]=(++n_trig[z]);
    if(rep_level>=3)
      {
      sprintf(line,"  %-4s(%-7.7s) on, %s %5d\n",tbl[ch].name,
        zone[z],time_text,(int)tbl[ch].lta_save);
      write_file(file_trig,line);
      }
    if(n_trig[z]==min_trig[z])
      {
      n_zone_trig++;
      i_zone[cnt_zone++]=z;
      if(rep_level>=2)
        {
        sprintf(line," %s %-7.7s on,  min=%d\n",time_text,
          zone[z],min_trig[z]);
        write_file(file_trig,line);
        }
      if(n_zone_trig==1 && rep_level>=1)
        {
        if(strncmp(prev_on,time_text,13)==0)
          {
          cptm(tm,tbl[ch].tm);
          tm[5]++;
          adj_time(tm);
          sprintm(time_text,tm);
          }
        strncpy(prev_on,time_text,13);
        sprintf(line,"%13.13s on, at %.7s\n",time_text,zone[z]);
        if(strncmp2(time_text,last_line,13)>0)
          {
          write_file(file_trig,line);
          not_yet=0;
          }
        /* delayed cont message */
        if(tbl[ch].tm[4]!=tim[4])
          {
          sprintf(line,"%02d%02d%02d.%02d%02d00 cont, %d+",
            tim[0],tim[1],tim[2],tim[3],tim[4],n_zone_trig);
          if(n_zone_trig>1) sprintf(line+strlen(line)," zones\n");
          else sprintf(line+strlen(line)," zone\n");
          if(not_yet==0) write_file(file_trig,line);
          }
        }
      }
    }
  }

sprintm(tb,tm)
  char *tb;
  int *tm;
  {
  sprintf(tb,"%02d%02d%02d.%02d%02d%02d",
    tm[0],tm[1],tm[2],tm[3],tm[4],tm[5]);
  }

cptm(dst,src)
  int *dst,*src;
  {
  int i;
  for(i=0;i<6;i++) dst[i]=src[i];
  }

confirm_off(ch,sec,i)
  int ch,sec,i;
  {
  int ii,j,tm[6],z;
  static char prev_off[15];
  tbl[ch].status=0;

  for(j=0;j<tbl[ch].n_zone;j++)
    {
    z=tbl[ch].zone[j];
    if(rep_level>=3)
      {
      sprintf(line,"  %-4s(%-7.7s) off, %5.1f %7d\n",
        tbl[ch].name,zone[z],tbl[ch].sec_on,tbl[ch].max);
      write_file(file_trig,line);
      }
    if(--n_trig[z]==min_trig[z]-1)
      {
      sprintm(time_text,tim);
      sprintf(time_text+strlen(time_text),".%d",i*(10/SR_MON));
      if(rep_level>=2)
        {
        sprintf(line,
          " %s %-7.7s off, max=%d\n",time_text,zone[z],max_trig[z]);
        write_file(file_trig,line);
        }
      if(--n_zone_trig==0)
        {
        if(rep_level>=1)
          {
          if(strncmp(prev_off,time_text,13)==0)
            {
            cptm(tm,tim);
            tm[5]++;
            adj_time(tm);
            sprintm(time_text,tm);
            }
          strncpy(prev_off,time_text,13);
          sprintf(line,"%13.13s off,",time_text);
          for(ii=1;ii<cnt_zone;ii++)
            sprintf(line+strlen(line)," %.7s",zone[i_zone[ii]]);
          sprintf(line+strlen(line),"\n");
          if(not_yet==0) write_file(file_trig,line);
          }
        cnt_zone=0;
        }
      }
    }
  }

read_one_sec(sec)
  int *sec;
  {
  int i,j,k,lower_min,lower_max,aa,bb,kk,sys,ch,ret,size;
  unsigned char *ptr,*ptr_lim;
  static unsigned long upper[4][8]={
    0x00000000,0x00000010,0x00000020,0x00000030,
    0xffffffc0,0xffffffd0,0xffffffe0,0xfffffff0,
    0x00000000,0x00000100,0x00000200,0x00000300,
    0xfffffc00,0xfffffd00,0xfffffe00,0xffffff00,
    0x00000000,0x00010000,0x00020000,0x00030000,
    0xfffc0000,0xfffd0000,0xfffe0000,0xffff0000,
    0x00000000,0x01000000,0x02000000,0x03000000,
    0xfc000000,0xfd000000,0xfe000000,0xff000000};

  if((ret=read(fd,buf,4))<4) return 0;
  size=mklong(buf);
  if(size<0 || size>LENGTH) return 0;
  if((ret=read(fd,buf+4,size-4))<size-4) return 0;
  ptr=buf+9;
  ptr_lim=buf+size;
  *sec=(((*ptr)>>4)&0x0f)*10+((*ptr)&0x0f);
  ptr++;
  j=0;
  for(i=0;i<m_ch;i++) idx[tbl[i].ch]=(-1);

  while(ptr<ptr_lim)  /* ch loop */
    {
    sys=(*ptr++);
    ch=(sys<<8)+(*(ptr++));
    for(i=0;i<SR_MON;i++)
      {
      aa=(*(ptr++));
      bb=aa&3;
      if(bb)
        {
        lower_min=lower_max=0;
        kk=0;
        for(k=0;k<bb;k++)
          {
          lower_min|=(*ptr++)<<kk;
          kk+=8;
          }
        kk=0;
        for(k=0;k<bb;k++)
          {
          lower_max|=(*ptr++)<<kk;
          kk+=8;
          }
        }
      else
        {
        lower_min=(*ptr>>4)&0xf;
        lower_max=(*ptr)&0xf;
        ptr++;
        }
      if(idx2[ch])
        {
        long_min[j][i]=lower_min | upper[bb][(aa>>5)&7];
        long_max[j][i]=lower_max | upper[bb][(aa>>2)&7];
        }
      }
    if(idx2[ch]) idx[ch]=j++;
    }
  /* 1-sec data prepared */
  return j;
  }

plot_wave(xbase,ybase)
  int xbase,ybase;
  {
  int x_bit,x_byte,yy,i,j,k,x,y,y_min,y_max,ch,sec;
  double data;
  static unsigned char bit_mask[8]={
    0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};

  /* draw ticks */
  for(sec=0;sec<=60;sec+=10)
    {
    if((x=xbase+sec*SR_MON)==WIDTH_LBP*8) x=WIDTH_LBP*8-1;
    x_bit=x&0x7;
    x_byte=x>>3;
    for(y=0;y<Y_SCALE;y++)
      {
      frame[ybase-Y_SCALE+y][x_byte]|=bit_mask[x_bit];
      frame[ybase+(pixels_per_trace*m_ch)+Y_SCALE-y][x_byte]|=bit_mask[x_bit];
      }
    }
  for(x=xbase;x<xbase+Y_SCALE;x++)
    {
    x_bit=x&0x7;
    x_byte=x>>3;
    frame[ybase-Y_SCALE][x_byte]|=bit_mask[x_bit];
    frame[ybase+(pixels_per_trace*m_ch)+Y_SCALE][x_byte]|=bit_mask[x_bit];
    }
  for(x=xbase+60*SR_MON;x>xbase+60*SR_MON-Y_SCALE;x--)
    {
    if(x==WIDTH_LBP*8) x=WIDTH_LBP*8-1;
    x_bit=x&0x7;
    x_byte=x>>3;
    frame[ybase-Y_SCALE][x_byte]|=bit_mask[x_bit];
    frame[ybase+(pixels_per_trace*m_ch)+Y_SCALE][x_byte]|=bit_mask[x_bit];
    }

  if(n_zone_trig>0 && rep_level>=1)
    {
    sprintf(line,"%02d%02d%02d.%02d%02d00 cont, %d",
      tim[0],tim[1],tim[2],tim[3],tim[4],n_zone_trig);
    if(n_zone_trig>1) sprintf(line+strlen(line)," zones\n");
    else sprintf(line+strlen(line)," zone\n");
    if(not_yet==0) write_file(file_trig,line);
    }

  /* plot loop */
  while(read_one_sec(&tim[5]))
    {
    for(i=0;i<SR_MON;i++)
      {
      x=xbase+tim[5]*SR_MON+i;
      yy=ybase+ppt_half;

      for(ch=0;ch<m_ch;ch++)
        {
        if(idx[tbl[ch].ch]!=(-1) && tbl[ch].name[0]!='*')
          {
          if(tim[5]==0 && i==0 && m_limit==0 && !tbl[ch].alive)
            {
            printf("pmon:'%s' present (pmon)",tbl[ch].name);
            tbl[ch].alive=1;
            }
        /* plot data */
          y_min=(-(long_min[idx[tbl[ch].ch]][i]>>tbl[ch].gain));
          if(y_min>ppt_half) y_min=yy+ppt_half;
          else if(y_min<(-ppt_half)) y_min=yy-ppt_half;
          else y_min+=yy;
          y_max=(-(long_max[idx[tbl[ch].ch]][i]>>tbl[ch].gain));
          if(y_max>ppt_half) y_max=yy+ppt_half;
          else if(y_max<(-ppt_half)) y_max=yy-ppt_half;
          else y_max+=yy;
          x_bit=x&0x7;
          x_byte=x>>3;
          for(y=y_max;y<=y_min;y++) /* y_min>=y_max */
            frame[y][x_byte]|=bit_mask[x_bit];
          }
        else if(tim[5]==0 && i==0 && tbl[ch].name[0]!='*' &&
            m_limit==0 && tbl[ch].alive)
          {
          printf("pmon:'%s' absent (pmon)",tbl[ch].name);
          tbl[ch].alive=0;
          }
        yy+=pixels_per_trace;

      /* check trigger */
        if(tbl[ch].use==0) continue;
        if(idx[tbl[ch].ch]==(-1)) data=0.0; /* ch not found */
        else data=(double)(long_max[idx[tbl[ch].ch]][i]-
            long_min[idx[tbl[ch].ch]][i]);
        if(data<=TOO_LOW) /* i.e. max==min for 0.2 s period */
        /* this means that the channel is not working */
          {
        /* if '0' continued more than 1 s, disable channel */
          if((tbl[ch].sec_zero+=dt)>TIME_TOO_LOW)
            tbl[ch].cnt=0.0;
        /* when data==0, disable calculation of STA and LTA */
          }
        else  /* if data is valid */
          {
        /* restart '0 data' counter */
          tbl[ch].sec_zero=0.0;
        /* calculate STA */
          tbl[ch].sta=tbl[ch].sta*a_sta+data*b_sta;
        /* calculate LTA and LTA_OFF */
          if(tbl[ch].status==1 || tbl[ch].status==2)
            tbl[ch].lta=tbl[ch].lta*a_lta_off+data*b_lta_off;
          else tbl[ch].lta=tbl[ch].lta*a_lta+data*b_lta;
          }
#if DEBUG
/*if(tbl[ch].ch==0x81 && tim[4]>=20)
*/
printf(
"%5d %s %5d-%5d %5.1f/%5.1f(%5.1f) %5.1f %5.1f %d %5.1f %5.1f\n",
tim[5],tbl[ch].name,long_max[idx[tbl[ch].ch]][i],long_min[idx[tbl[ch].ch]][i],
tbl[ch].sta,tbl[ch].lta,tbl[ch].lta_save,tbl[ch].sec_on,
tbl[ch].sec_off,
tbl[ch].status,data,tbl[ch].cnt);
#endif
        if((tbl[ch].cnt+=dt)<time_lta*2.0)  /* station is disabled */
    /* not enough time has passed after recovery from '0 data' */
          {
          switch(tbl[ch].status)      /* force to OFF */
            {
            case 0:   /* OFF */
              break;
            case 1:   /* ON but not confirmed */
              tbl[ch].sec_on+=dt;
              tbl[ch].status=0;
              break;
            case 2:   /* ON confirmed */
              tbl[ch].sec_on+=dt;
            case 3:   /* OFF but not confirmed */
              confirm_off(ch,tim[5],i);
              break;
            }
          continue;
          }

      /* calculate STA/LTA */
        if(tbl[ch].sta/tbl[ch].lta>=tbl[ch].ratio)
          {
          switch(tbl[ch].status)
            {
            case 0:   /* OFF */
              cptm(tbl[ch].tm,tim);
              tbl[ch].tm[6]=i;
              tbl[ch].sec_on=0.0;
              tbl[ch].max=data;
              tbl[ch].lta_save=tbl[ch].lta;
              tbl[ch].status=1;
              break;
            case 1:   /* ON but not confirmed */
              if((tbl[ch].sec_on+=dt)>=time_on) confirm_on(ch);
              if(tbl[ch].max<data) tbl[ch].max=data;
              break;
            case 2:   /* ON confirmed */
              tbl[ch].sec_on+=dt;
              if(tbl[ch].max<data) tbl[ch].max=data;
              break;
            case 3:   /* OFF but not confirmed */
              tbl[ch].sec_on+=(tbl[ch].sec_off+dt);
              tbl[ch].status=2;
              if(tbl[ch].max<data) tbl[ch].max=data;
              break;
            }
          }
        else
          {
          switch(tbl[ch].status)
            {
            case 0:   /* OFF */
              break;
            case 1:   /* ON but not confirmed */
              tbl[ch].sec_on+=dt;
              tbl[ch].status=0;
              break;
            case 2:   /* ON confirmed */
              tbl[ch].sec_on+=dt;
              tbl[ch].sec_off=0.0;
              tbl[ch].status=3;
              break;
            case 3:   /* OFF but not confirmed */
              if((tbl[ch].sec_off+=dt)>=time_off)
                confirm_off(ch,tim[5],i);
              break;
            }
          }
        }
      }
    if(m_limit) {printf(".");fflush(stdout);}
    if(tim[5]==59) break;
    }
  if(m_limit) printf("\n");
  }

hangup()
  {
  req_print=1;
  signal(SIGHUP,(void *)hangup);
  }

owari()
  {
  close(fd);
  printf("*****  end of PMON  *****\n");
  if(made_lock_file) unlink(file_trig_lock);
  exit(0);
  }

put_font(bitmap,width_byte,xbase,ybase,text,font,
    height_font,width_font,erase)
  unsigned char *bitmap,*text,*font;
  int width_byte,xbase,ybase,height_font,width_font,erase;
  {
  int xx,yy,i,j,k,x_bit,x_byte,xx_bit,xx_byte,x,y;
  static unsigned char bit_mask[8]={
    0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};
  for(i=0;i<strlen(text);i++)
    {
    for(j=0;j<height_font;j++)
      {
      xx=(text[i]-CODE_START)*width_font+(((width_font*N_CODE+15)>>4)<<4)*j;
      x=xbase+(ybase+j)*(width_byte<<3)+i*width_font;
      for(k=0;k<width_font;k++)
        {
        xx_bit=xx&0x7;
        xx_byte=xx>>3;
        x_bit=x&0x7;
        x_byte=x>>3;
        if(erase) bitmap[x_byte]&=(~bit_mask[x_bit]);
        if(font[xx_byte]&bit_mask[xx_bit])
          bitmap[x_byte]|=bit_mask[x_bit];
        xx++;
        x++;
        }
      }
    }
  }

insatsu(tb1,tb2,tb3,path_spool,printer)
  unsigned char *tb1,*tb2,*tb3,*path_spool,*printer;
  {
  struct rasterfile {
    int ras_magic;    /* magic number */
    int ras_width;    /* width (pixels) of image */
    int ras_height;   /* height (pixels) of image */
    int ras_depth;    /* depth (1, 8, or 24 bits) of pixel */
    int ras_length;   /* length (bytes) of image */
    int ras_type;     /* type of file; see RT_* below */
    int ras_maptype;  /* type of colormap; see RMT_* below */
    int ras_maplength;/* length (bytes) of following map */
    /* color map follows for ras_maplength bytes, followed by image */
    } ras;
#define RAS_MAGIC 0x59a66a95
#define RT_STANDARD 1 /* Raw pixrect image in 68000 byte order */
#define RMT_NONE  0   /* ras_maplength is expected to be 0 */
  FILE *lbp;
  int i,j;
  static int serno;
  char filename[100];
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPU;
#endif

  put_font(frame,WIDTH_LBP,X_BASE-28*max_ch_flag,
    HEIGHT_LBP-HEIGHT_FONT32,tb1,font32,HEIGHT_FONT32,WIDTH_FONT32,0);
  put_font(frame,WIDTH_LBP,WIDTH_LBP*8-(strlen(tb2)+strlen(tb3))*WIDTH_FONT32,
    HEIGHT_LBP-HEIGHT_FONT32,tb2,font32,HEIGHT_FONT32,WIDTH_FONT32,0);
  put_font(frame,WIDTH_LBP,WIDTH_LBP*8-strlen(tb3)*WIDTH_FONT32,
    HEIGHT_LBP-HEIGHT_FONT32,tb3,font32,HEIGHT_FONT32,WIDTH_FONT32,1);
  put_font(frame,WIDTH_LBP,(WIDTH_LBP*8-3*WIDTH_FONT32)/2,
    0,"|",font32,HEIGHT_FONT32,WIDTH_FONT32,0);
  sprintf(filename,"%s/pmon%02d.%d",path_spool,serno,getpid());
  serno=(++serno)%100;
  lbp=fopen(filename,"w");
  ras.ras_magic=RAS_MAGIC;
  ras.ras_width=WIDTH_LBP*8;
  ras.ras_height=HEIGHT_LBP;
  ras.ras_depth=1;
  ras.ras_length=HEIGHT_LBP*WIDTH_LBP;
  ras.ras_type=RT_STANDARD;
  ras.ras_maptype=RMT_NONE;
  ras.ras_maplength=0;
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(ras.ras_magic);
  SWAPL(ras.ras_width);
  SWAPL(ras.ras_height);
  SWAPL(ras.ras_depth);
  SWAPL(ras.ras_length);
  SWAPL(ras.ras_type);
  SWAPL(ras.ras_maptype);
  SWAPL(ras.ras_maplength);
#endif
  fwrite((char *)&ras,1,sizeof(ras),lbp);   /* output header */
  fwrite(frame,1,HEIGHT_LBP*WIDTH_LBP,lbp); /* output image */
  fclose(lbp);
#if defined(__SVR4)
  if(m_limit) printf("cat %s|lp -d %s -T raster\n",filename,printer);
  sprintf(line,"cat %s|lp -d %s -T raster\n",filename,printer);
  system(line);
  unlink(filename);
#else
#if defined(__FreeBSD__)
  if(m_limit) printf("lpr -P%s -r -v %s\n",printer,filename);
  sprintf(line,"lpr -P%s -r -v %s",printer,filename);
#else
  if(m_limit) printf("lpr -P%s -s -r -v %s\n",printer,filename);
  sprintf(line,"lpr -P%s -s -r -v %s",printer,filename);
#endif
  system(line);
#endif
  if(req_print==0)
    {
    for(i=0;i<HEIGHT_LBP;i++) for(j=0;j<WIDTH_LBP;j++) frame[i][j]=0;
    }
  req_print=0;
  return;
  }

read_param(f_param,textbuf)
  FILE *f_param;
  unsigned char *textbuf;
  {
  do  {
    if(fgets(textbuf,200,f_param)==NULL) return 1;
    } while(*textbuf=='#');
  return 0;
  }

get_lastline(fname,lastline)
  char *fname,*lastline;
  {
  FILE *fp;
  long dp,last_dp;
  if((fp=fopen(fname,"r"))==NULL)
    {
    *lastline=0;
    return;
    }
  last_dp=(-1);
  dp=0;
  while(fgets(lastline,200,fp)!=NULL)
    {
    if(*lastline!=' ') last_dp=dp;
    dp=ftell(fp);
    }
  if(last_dp==(-1))
    {
    *lastline=0;
    return;
    }
  else
    {
    fseek(fp,last_dp,0);
    fgets(lastline,200,fp);
    }
  fclose(fp);
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  FILE *f_param,*fp;
  int i,j,k,ret,m_count,x_base,y_base,y,ch,tm[6],minutep,hourp;
  char *ptr,textbuf[500],textbuf1[500],fn1[200],fn2[100],
    fn3[100],path_font[80],path_temp[80],path_mon[80],area[20],
    timebuf1[80],timebuf2[80],printer[40],name[6],comp[4],path_mon1[80];

  if(argc<2)
    {
    printf("usage of pmon :\n");
    printf("  pmon [param file] ([YYMMDD] [hhmm] [length(min)])\n");
    exit(1);
    }
  printf("*****  pmon start  *****\n");
  strcpy(param_file,argv[1]);

  /* open parameter file */ 
  if((f_param=fopen(param_file,"r"))==NULL)
    {
    printf("pmon:'%s' not open\n",param_file);
    owari();
    }
  read_param(f_param,fn1);      /* (1) footnote */
  fn1[strlen(fn1)-1]=0;         /* delete CR */
  read_param(f_param,textbuf1); /* (2) mon data directory */
  sscanf(textbuf1,"%s",textbuf);
  if((ptr=strchr(textbuf,':'))==0)
    {
    sscanf(textbuf,"%s",path_mon);
    sscanf(textbuf,"%s",path_mon1);
    }
  else
    {
    *ptr=0;
    sscanf(textbuf,"%s",path_mon);
    sscanf(ptr+1,"%s",path_mon1);
    }
  read_param(f_param,textbuf);  /* (3) temporary work directory */
  sscanf(textbuf,"%s",path_temp);
  read_param(f_param,textbuf);  /* (4) channel table file */
  sscanf(textbuf,"%s",ch_file);
  read_param(f_param,textbuf);  /* (5) printer name */
  sscanf(textbuf,"%s",printer);
  read_param(f_param,textbuf);  /* (6) rows/sheet */
  sscanf(textbuf,"%d",&n_rows);
  read_param(f_param,textbuf);  /* (7) traces/row */
  sscanf(textbuf,"%d",&max_ch);
/**********************************************
  n_rows    max_ch(just for suggestion; i.e. pixels_per_trace=20)
    1        209
    2        103 (approx. 140 is possible actually)
    3         67
    6         32
    9         20
   12         14
 **********************************************/
  read_param(f_param,textbuf);  /* (8) trigger report file */
  sscanf(textbuf,"%s",file_trig);
  strcpy(file_trig_lock,file_trig);
  strcat(file_trig_lock,".lock");
  read_param(f_param,textbuf);  /* (9) zone table file */
  sscanf(textbuf,"%s",file_zone);
  fclose(f_param);
  sprintf(temp_done,"%s/USED",path_mon1);
  sprintf(latest,"%s/LATEST",path_mon);
  min_per_sheet=MIN_PER_LINE*n_rows;    /* min/sheet */

  if(argc<5)
    {
retry:
    if((fp=fopen(temp_done,"r"))==NULL) /* read USED file for when to start */
      {
      while((fp=fopen(latest,"r"))==NULL)
        {
        printf("'%s' not found. Waiting ...\n",path_mon);
        sleep(30);
        }
      }
    if(fscanf(fp,"%2d%2d%2d%2d.%2d",tim,tim+1,tim+2,tim+3,tim+4)<5)
      {
      fclose(fp);
      printf("'%s' illegal. Waiting ...\n",path_mon);
      sleep(30);
      goto retry;
      }
    fclose(fp);
    /* decide 'even' start time */
    i=((tim[3]*60+tim[4])/min_per_sheet)*min_per_sheet;
    tim[3]=i/60;
    tim[4]=i%60;
    /* no limit */
    m_limit=0;
    }
  else
    {
    sscanf(argv[2],"%2d%2d%2d",tim,tim+1,tim+2);
    sscanf(argv[3],"%2d%2d",tim+3,tim+4);
    sscanf(argv[4],"%d",&m_limit);
    if(m_limit>0) strcat(temp_done,"_");
    }
  made_lock_file=0;
  if(m_limit==0)
    {
    if((fp=fopen(file_trig_lock,"r"))!=NULL)
      {
      fscanf(fp,"%d",&i);
      fclose(fp);
      if(kill(i,0)==0)
        {
        printf("Can't run because lock file '%s' is valid for PID#%d.\n",
          file_trig_lock,i);
        owari();
        }
      unlink(file_trig_lock);
      }
    if((fp=fopen(file_trig_lock,"w+"))!=NULL)
      {
      fprintf(fp,"%d\n",getpid());  /* write process ID */
      fclose(fp);
      made_lock_file=1;
      }
    }
      
  make_fonts(font16,font24,font32);

  /* get the last line in trg file */
  get_lastline(file_trig,last_line);
  if(*last_line && m_limit) printf("last line in trig file = %s",last_line);

  pixels_per_trace=(((PLOTHEIGHT-(n_rows-1)*Y_SPACE)/n_rows)-Y_SCALE*2)/max_ch;
  if(pixels_per_trace<20) max_ch_flag=1;  /* not use 24dot font */
  else max_ch_flag=0;
  ppt_half=pixels_per_trace/2;
  m_count=0;
  fd=(-1);
  if(*last_line) not_yet=1;
  else not_yet=0;
  n_zone_trig=cnt_zone=0;
  x_base=X_BASE-28*max_ch_flag;
  y_base=Y_BASE;
  for(i=0;i<M_CH;i++) tbl[i].use=0;
  for(i=0;i<HEIGHT_LBP;i++) for(j=0;j<WIDTH_LBP;j++) frame[i][j]=0;

  signal(SIGINT,(void *)owari);
  signal(SIGTERM,(void *)owari);
  signal(SIGPIPE,(void *)owari);
  signal(SIGHUP,(void *)hangup);
  req_print=0;
  *timebuf1=0;

  while(1)
    {
    if(m_count%MIN_PER_LINE==0) /* print info */
      {
      sprintf(textbuf,"%02d/%02d/%02d %02d:%02d",
        tim[0],tim[1],tim[2],tim[3],tim[4]);
      put_font(frame,WIDTH_LBP,0,y_base-Y_SCALE-HEIGHT_FONT32,textbuf,
        font32,HEIGHT_FONT32,WIDTH_FONT32,0);
      /* update channels */
      while((f_param=fopen(param_file,"r"))==NULL)
        {
        printf("pmon:'%s' file not found (%d)",param_file,getpid());
        sleep(60);
        }
      for(i=0;i<9;i++) read_param(f_param,textbuf);
      read_param(f_param,textbuf);    /* (10) */
      sscanf(textbuf,"%d",&n_min_trig);
      read_param(f_param,textbuf);    /* (11) */
      sscanf(textbuf,"%lf",&time_sta);
      read_param(f_param,textbuf);    /* (12) */
      sscanf(textbuf,"%lf",&time_lta);
      read_param(f_param,textbuf);    /* (13) */
      sscanf(textbuf,"%lf",&time_lta_off);
      read_param(f_param,textbuf);    /* (14) */
      sscanf(textbuf,"%lf",&time_on);
      read_param(f_param,textbuf);    /* (15) */
      sscanf(textbuf,"%lf",&time_off);
      read_param(f_param,textbuf);    /* (16) */
      sscanf(textbuf,"%d",&rep_level);
      j=1;
      for(i=0;i<max_ch;i++)
        {
        if(tbl[i].use && tbl[i].status) j=0;
        tbl[i].alive=1;
        }
      if(j)   /* there is no triggered channel now */
        {
        n_zone=0;
        for(i=0;i<65536;i++) idx2[i]=0;
        for(i=0;i<max_ch;i++)
          {
          if(read_param(f_param,textbuf)) break;
          if(!isalnum(*textbuf)) /* make a blank line */
            {
            tbl[i].use=0;
            tbl[i].name[0]='*';
            continue;
            }
          sscanf(textbuf,"%s%s%d%lf",tbl[i].name,
            tbl[i].comp,&tbl[i].gain,&tbl[i].ratio);
          tbl[i].n_zone=0;
          /* get sys_ch from channel table file */
          while((fp=fopen(ch_file,"r"))==NULL)
            {
            printf("pmon:'%s' file not found (%d)",param_file,getpid());
            sleep(60);
            }
          while((ret=read_param(fp,textbuf1))==0)
            {
            sscanf(textbuf1,"%x%d%*d%s%s",&j,&k,name,comp);
            /*if(k==0) continue;*/  /* check 'record' flag */
            if(strcmp(tbl[i].name,name)) continue;
            if(strcmp(tbl[i].comp,comp)) continue;
            ch=j;
            ret=0;
            break; /* channel (j) found */
            }
          fclose(fp);
          if(ret)   /* channel not found in file */
            {
            printf("pmon:'%s-%s' not found\n",tbl[i].name,tbl[i].comp);
            tbl[i].use=0;
            tbl[i].name[0]='*';
            continue;
            }
          while((fp=fopen(file_zone,"r"))==NULL)
            {
            printf("pmon:'%s' file not found (%d)",param_file,getpid());
            sleep(60);
            }
          while(ptr=fgets(textbuf,500,fp))
            {
            if(*ptr=='#') ptr++;
            if((ptr=strtok(ptr," +/\t\n"))==NULL) continue;
            strcpy(area,ptr);
            while(ptr=strtok(0," +/\t\n")) if(strcmp(ptr,tbl[i].name)==0) break;
            if(ptr) /* a zone for station "tbl[i].name" found */
              {
              if(tbl[i].ratio!=0.0)
                {
                for(j=0;j<n_zone;j++) if(strcmp(area,zone[j])==0) break;
                if(j==n_zone) /* register a new zone */
                  {
                  n_stn[n_zone]=n_trig[n_zone]=0;
                  strcpy(zone[n_zone++],area);
                  }
                if(tbl[i].n_zone<MAX_ZONES)
                  {
                  tbl[i].zone[tbl[i].n_zone]=j; /* add zone for the stn */
                  tbl[i].n_zone++; /* n of zones for the stn */
                  n_stn[j]++;      /* n of stns for the zone */
                  }
                else printf("pmon:too many zones for station '%s'\n",
                  tbl[i].name);
                }
              }
            }
          fclose(fp);
          if(tbl[i].ratio!=0.0)
            {
            if(tbl[i].n_zone==0)
              {
              printf("pmon:zone for '%s' not found %d\n",tbl[i].name,i);
              tbl[i].ratio=0.0;
              }
            else
              {
              tbl[i].sec_zero=0.0;
              tbl[i].status=0;
              if(!(ch==tbl[i].ch) || !tbl[i].use)
                tbl[i].lta=tbl[i].sta=tbl[i].cnt=0.0;
              tbl[i].use=1;
              }
            }
          else tbl[i].use=0;
          tbl[i].ch=ch;
          idx2[ch]=1;
          }
        m_ch=i;
        if(m_limit)
          printf("N of valid channels = %d (in %d frames)\n",m_ch,max_ch);
        for(i=0;i<n_zone;i++)
          {
          if(n_stn[i]<=n_min_trig) min_trig[i]=n_stn[i];
          else min_trig[i]=n_min_trig;
          if(m_limit) printf("zone #%2d %-12s n=%2d min=%2d\n",
            i,zone[i],n_stn[i],min_trig[i]);
          }
        }
      fclose(f_param);
      /* calculate coefs a & b */
      a_sta=exp(-dt/time_sta);          /* nearly 1 */
      b_sta=1.0-a_sta;                  /* nearly 0 */
      a_lta=exp(-dt/time_lta);          /* nearly 1 */
      b_lta=1.0-a_lta;                  /* nearly 0 */
      a_lta_off=exp(-dt/time_lta_off);  /* nearly 1 */
      b_lta_off=1.0-a_lta_off;          /* nearly 0 */

      y=y_base;
      for(i=0;i<m_ch;i++)
        {
        if(tbl[i].name[0]!='*')
          {
          sprintf(textbuf,"%04X ",tbl[i].ch);
          put_font(frame,WIDTH_LBP,0,y+8-8*max_ch_flag,textbuf,font16,
            HEIGHT_FONT16,WIDTH_FONT16,0);
          if(i==0) sprintf(textbuf,"%-4s",tbl[i].name);
          else
            {
            if(strncmp(tbl[i].name,tbl[i-1].name,4)==0)
              sprintf(textbuf,"    ");
            else sprintf(textbuf,"%-4s",tbl[i].name);
            }
          strcat(textbuf,"-");
          sprintf(textbuf+strlen(textbuf),"%-2s",tbl[i].comp);
          if(max_ch_flag)
            put_font(frame,WIDTH_LBP,WIDTH_FONT16*5,y,textbuf,font16,
              HEIGHT_FONT16,WIDTH_FONT16,0);
          else
            put_font(frame,WIDTH_LBP,WIDTH_FONT16*5,y,textbuf,font24,
              HEIGHT_FONT24,WIDTH_FONT24,0);
          sprintf(textbuf,"%2d",tbl[i].gain);
          if(max_ch_flag)
            put_font(frame,WIDTH_LBP,WIDTH_FONT16*4+WIDTH_FONT16*7,y,
              textbuf,font16,HEIGHT_FONT16,WIDTH_FONT16,0);
          else
            put_font(frame,WIDTH_LBP,WIDTH_FONT16*4+WIDTH_FONT24*7,y+8,
              textbuf,font16,HEIGHT_FONT16,WIDTH_FONT16,0);
          }
        y+=pixels_per_trace;
        }
      }
    if(m_count%min_per_sheet==0)  /* make fns */
      sprintf(fn2,"%02d/%02d/%02d %02d:%02d",
        tim[0],tim[1],tim[2],tim[3],tim[4]);
    sprintf(timebuf2,"%02d%02d%02d%02d.%02d",
        tim[0],tim[1],tim[2],tim[3],tim[4]);
/*
  timebuf1 : LATEST
  timebuf2 : tim[] - data time
*/
    if(*timebuf1==0 || strcmp2(timebuf1,timebuf2)<0) while(1) /* wait LATEST */
      {
      if((fp=fopen(latest,"r"))==NULL) sleep(15);
      else
        {
        fscanf(fp,"%s",timebuf1);
        fclose(fp);
        if(strlen(timebuf1)>=11 && strcmp2(timebuf1,timebuf2)>=0) break;
        }
      if(isalpha(*printer) && req_print) insatsu(fn1,fn2,fn3,path_temp,printer);
      sleep(15);
      }
    sprintf(textbuf,"%s/%s",path_mon,timebuf2);
    if((fd=open(textbuf,O_RDONLY))!=-1)
      {
      if(m_limit) printf("%02d%02d%02d %02d%02d",
              tim[0],tim[1],tim[2],tim[3],tim[4]);
      fflush(stdout);
      plot_wave(x_base,y_base); /* one minute data */
      close(fd);
      if(m_limit==0){
        fp=fopen(temp_done,"w+");
        fprintf(fp,"%s\n",timebuf2);  /* write done file */
        fclose(fp);
        }
      }
    else if(m_limit) printf("%s failed\n",timebuf2);
    minutep=tim[4];
    hourp=tim[3];
    if(++minutep==60)
      {
      if(++hourp==24) hourp=0;
      minutep=0;
      }
    sprintf(fn3," - %02d:%02d",hourp,minutep);
    x_base+=SR_MON*60;
    if(++m_count==m_limit)
      {
      if(isalpha(*printer)) insatsu(fn1,fn2,fn3,path_temp,printer);
      owari();
      }
    else if(m_count%min_per_sheet==0)
      {
      if(isalpha(*printer)) insatsu(fn1,fn2,fn3,path_temp,printer);
      x_base=X_BASE-28*max_ch_flag;
      y_base=Y_BASE;
      }
    else
      {
      if(isalpha(*printer) && req_print) insatsu(fn1,fn2,fn3,path_temp,printer);
      if(m_count%MIN_PER_LINE==0)
        {
        x_base=X_BASE-28*max_ch_flag;
        y_base+=Y_SCALE*2+pixels_per_trace*max_ch+Y_SPACE;
        }
      }
    if(m_limit) {printf("\007");fflush(stdout);}
    tim[5]=60;
    adj_time(tim);
    }
  }
