/* $Id: winlib.h,v 1.1.2.7.2.13 2009/08/25 04:00:16 uehira Exp $ */

#ifndef _WIN_LIB_H_
#define _WIN_LIB_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "filter.h"
#include "win_log.h"

#include "subst_func.h"

/***** re-define integer *****/
/* define 1 bye integer */
#if SIZEOF_CHAR == 1
#ifdef __CHAR_UNSIGNED__
typedef signed char      int8_w;   /*   signed 1 byte integer */
#else
typedef char             int8_w;   /*   signed 1 byte integer */
#endif
typedef unsigned char   uint8_w;   /* unsigned 1 byte integer */
#else
#error char is not 1 byte length.
#endif
/* define 2 bye integer */
#if SIZEOF_SHORT == 2
typedef short            int16_w;  /*   signed 2 byte integer */
typedef unsigned short  uint16_w;  /* unsigned 2 byte integer */
#else
#error short is not 2 byte lengths.
#endif
/* define 4 bye integer */
#if SIZEOF_INT == 4
typedef int              int32_w;  /*   signed 4 byte integer */
typedef unsigned int    uint32_w;  /* unsigned 4 byte integer */
#else
#error int is not 4 byte lengths.
#endif
/* check float size */
#if SIZEOF_FLOAT != 4
#error float is not 4 byte lengths.
#endif
/******************************/

typedef uint32_w  WIN_blocksize;
typedef uint32_w  WIN_bs;
typedef uint16_w  WIN_ch;
typedef uint32_w  WIN_sr;

#define WIN_BSLEN  (sizeof(WIN_bs))  /* WIN block size length in byte */
#define WIN_BLOCKSIZE_LEN  WIN_BSLEN
#define WIN_TM_LEN       6  /* byte */
#define WIN_TIME_LEN     WIN_TM_LEN

/* High sampling rate format */
#define  HEADER_4B    4096     /* SR<2^12  (   1 Hz --    4095 Hz) */
#define  HEADER_5B    1048576  /* SR<2^20  (4096 Hz -- 1048575 Hz) */

#define  WIN_CHMAX    65536   /* Max. number of channel: 2^16 */
#define  SR_MON       5   /* sampling rate of MON */
#define  TIME_OFFSET ((time_t)0)  /* time(0) offset */

#define  SWAPL(a)  a = ((((a) << 24)) | (((a) << 8) & 0xff0000) |	\
			(((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))
#define  SWAPS(a)  a = ((((a) << 8) & 0xff00) | (((a) >> 8) & 0xff))
#define  MKSWAPS(a)  (uint16_w)((((a) << 8) & 0xff00) | (((a) >> 8) & 0xff))
#define  SWAPF(a)  *(int32_w *)&(a) =\
    (((*(int32_w *)&(a)) << 24) | ((*(int32_w *)&(a)) << 8) & 0xff0000 |\
     ((*(int32_w *)&(a)) >> 8) & 0xff00 | ((*(int32_w *)&(a)) >> 24) & 0xff)

/* memory malloc utility macro */
#ifndef MALLOC
#define MALLOC(type, n) (type*)malloc((size_t)(sizeof(type)*(n)))
#endif
#ifndef REALLOC
#define REALLOC(type, ptr, n) \
(type*)realloc((void *)ptr, (size_t)(sizeof(type)*(n)))
#endif
#ifndef FREE
#define FREE(a)         (void)free((void *)(a))
#endif

/* structure of shared memory */
struct Shm {
  size_t  p;         /* write point */
  size_t  pl;        /* write limit */
  size_t  r;         /* latest */
  unsigned long  c;  /* counter */
  uint8_w  d[1];     /* data buffer */
};

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
static uint8_w d2b[] = {
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
uint32_w mkuint4(const uint8_w *);
uint16_w mkuint2(const uint8_w *);
int bcd_dec(int [], uint8_w *);
int dec_bcd(uint8_w *, int *);
void adj_time_m(int []);
void adj_time(int []);
int time_cmp(int *, int *, int);
WIN_bs winform(int32_w *, uint8_w *, WIN_sr, WIN_ch);
uint32_w win2fix(uint8_w *, int32_w *, WIN_ch *, WIN_sr *);
int strncmp2(char *, char *, int);
int strcmp2(char *, char *);
WIN_bs read_onesec_win(FILE *, uint8_w **);
void Shm_init(struct Shm *, size_t);
void WIN_version(void);
uint32_w win_chheader_info(const uint8_w *, WIN_ch *, WIN_sr *, int *);
void get_mon(WIN_sr, int32_w *, int32_w (*)[]);
uint8_w *compress_mon(int32_w *, uint8_w *);
void make_mon(uint8_w *, uint8_w *);

#endif  /* !_WIN_LIB_H_*/
