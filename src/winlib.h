/* $Id: winlib.h,v 1.1.2.4 2008/05/17 14:22:06 uehira Exp $ */

#ifndef _WIN_LIB_H_
#define _WIN_LIB_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "filter.h"
#include "win_log.h"

#include "subst_func.h"

typedef unsigned long  WIN_blocksize;
typedef unsigned short WIN_ch;
typedef unsigned long  WIN_sr;

/* High sampling rate format */
#define  HEADER_4B    4096     /* SR<2^12  (   1 Hz --    4095 Hz) */
#define  HEADER_5B    1048576  /* SR<2^20  (4096 Hz -- 1048575 Hz) */

#define  SWAPL(a)  a = (((a) << 24) | ((a) << 8) & 0xff0000 |\
			((a) >> 8) & 0xff00 | ((a) >> 24) & 0xff)
#define  SWAPS(a)  a = (((a) << 8) & 0xff00 | ((a) >> 8) & 0xff)
#define  SWAPF(a)  *(long *)&(a) =\
    (((*(long *)&(a)) << 24) | ((*(long *)&(a)) << 8) & 0xff0000 |\
     ((*(long *)&(a)) >> 8) & 0xff00 | ((*(long *)&(a)) >> 24) & 0xff)
#define  LongFromBigEndian(a) \
  ((((unsigned char *)&(a))[0] << 24) + (((unsigned char *)&(a))[1] << 16) +\
   (((unsigned char *)&(a))[2] << 8) + ((unsigned char *)&(a))[3])

/* BCD to DEC */
static int b2d[] = {
  /* 0x00 - 0x0F */
  0,   1,  2,  3,  4 , 5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, -1, -1, -1, -1, -1, -1,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, -1, -1, -1, -1, -1, -1,
  30, 31, 32, 33, 34, 35, 36, 37, 38, 39, -1, -1, -1, -1, -1, -1,
  40, 41, 42, 43, 44, 45, 46, 47, 48, 49, -1, -1, -1, -1, -1, -1,
  50, 51, 52, 53, 54, 55, 56, 57, 58, 59, -1, -1, -1, -1, -1, -1,
  60, 61, 62, 63, 64, 65, 66, 67, 68, 69, -1, -1, -1, -1, -1, -1,
  70, 71, 72, 73, 74, 75, 76, 77, 78, 79, -1, -1, -1, -1, -1, -1,
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89, -1, -1, -1, -1, -1, -1,
  /* 0x90 - 0x9F */
  90, 91, 92, 93, 94, 95, 96, 97, 98, 99, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

/* DEC to BCD */
static unsigned char d2b[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,   
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99};

/** prototypes **/
void get_time(int []);
unsigned long mklong(unsigned char *);
unsigned short mkshort(unsigned char *);
int bcd_dec(int [], unsigned char *);
int dec_bcd(unsigned char *, unsigned int *);
void adj_time_m(int []);
void adj_time(int []);
int time_cmp(int *, int *, int);
int winform(long *, unsigned char *, int, unsigned short);
int win2fix(unsigned char *, long *, long *, long *);

#endif  /* !_WIN_LIB_H_*/
