/* $Id: winlib.h,v 1.1.2.7.2.29 2010/06/23 08:13:05 uehira Exp $ */

#ifndef _WIN_LIB_H_
#define _WIN_LIB_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else				/* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else				/* !HAVE_SYS_TIME_H */
#include <time.h>
#endif				/* !HAVE_SYS_TIME_H */
#endif				/* !TIME_WITH_SYS_TIME */

#include "ulaw.h"
#include "filter.h"
#include "pltxy.h"
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

/* typedef uint32_w  WIN_blocksize; */
typedef uint32_w  WIN_bs;   /* win blocksize */
typedef uint16_w  WIN_ch;
typedef  int32_w  WIN_sr;   /* 0 < sr < 2^20 */

#define WIN_YEAR  70   /* 1970 -- 2069 */

#define WIN_BSLEN  (sizeof(WIN_bs))  /* WIN block size length in byte */
#define WIN_BLOCKSIZE_LEN  WIN_BSLEN
#define WIN_TM_LEN       6  /* byte */
#define WIN_TIME_LEN     WIN_TM_LEN

/* High sampling rate format */
#define HEADER_4B    4096     /* SR<2^12  (   1 Hz --    4095 Hz) */
#define HEADER_5B    1048576  /* SR<2^20  (4096 Hz -- 1048575 Hz) */

#define WIN_CHMAX    65536   /* Max. number of channel: 2^16 */
#define WIN_CH_MAX_NUM  WIN_CHMAX   /* 2^16 */
#define WIN_STANAME_LEN    11   /* (length of station code)+1 */
#define WIN_STACOMP_LEN     7   /* (length of component code)+1 */

#define SR_MON       5   /* sampling rate of MON */
#define TIME_OFFSET ((time_t)0)  /* time(0) offset */

/* MT device (fromtape.c rtape.c) */
#define   TRY_LIMIT 16
#define   TIME1   "9005151102"  /* 10 m / fm before this time */
#define   TIME2   "9005161000"  /* no fms before this time */
                    /* 60 m / fm after this time */
#define   TIME3   "9008031718"  /* 10 m / fm after this time */

/*** process control file ***/
#define N_LATEST     "LATEST"

/* 'wdisk' process makes the following files */
#define WDISK_OLDEST  "OLDEST"
#define WDISK_LATEST  N_LATEST
#define WDISK_BUSY    "BUSY"
#define WDISK_COUNT   "COUNT"
#define WDISK_MAX     "MAX"

/* 'wdiskt' process makes the following files */
#define WDISKT_OLDEST  "OLDEST"
#define WDISKT_LATEST  "LATEST"
#define WDISKT_BUSY    "BUSY"
#define WDISKT_COUNT   "COUNT"
#define WDISKT_MAX     "MAX"

/* 'events' process makes the following files */
#define EVENTS_OLDEST  "OLDEST"
#define EVENTS_LATEST  "LATEST"
#define EVENTS_BUSY    "BUSY"
#define EVENTS_COUNT   "COUNT"
#define EVENTS_FREESPACE "FREESPACE"

#define TRG_CHFILE_SUFIX ".ch"

/* 'wtape' process makes the following files */
#define WTAPE_UNITS    "UNITS"
#define WTAPE__UNITS   "_UNITS"
#define WTAPE_USED     "USED"
#define WTAPE_EXABYTE  "EXABYTE"
#define WTAPE_TOTAL    "TOTAL"
#define N_EXABYTE      8

/* 'fromtape' process makes the following files */
/* #define FROMTAPE_LATEST  "LATEST" */
#define FROMTAPE_LATEST  N_LATEST

/* 'pmon' process makes the following files */
#define PMON_USED   "USED"

/* 'ecore' process makes the following files */
#define ECORE_USED  "USED"

/* 'insert_raw' process makes the following file */
#define INSERT_RAW_USED  "USED_RAW"

/* 'insert_trg' process makes the following file */
#define INSERT_TRG_USED  "USED_TRG"

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

/* structure of binary hypo file (28 bytes / event) */
struct FinalB {
  int8_w time[8];  /* Y,M,D,h,m,s,s10,mag10 (in binary, not in BCD) */
  float  alat;     /* latitude */
  float  along;    /* longitude */
  float  dep;      /* depth */
  char   diag[4];  /* label */
  char   owner[4]; /* picker name */
};

/* channel table */
struct channel_tbl {
  WIN_ch  sysch;                  /* channel number [2byte hex] */
  int     flag;                   /* record flag */
  int     delay;                  /* line delay [ms] */
  char    name[WIN_STANAME_LEN];  /* station name */
  char    comp[WIN_STACOMP_LEN];  /* component name */
  int     scale;                  /* display scale */
  char    bit[256];                    /* bit value of A/D converter */
  double  sens;                   /* sensitivity of instruments */
  char    unit[256];              /* unit of sensitivity */
  double  t0;                     /* natural period [s] */
  double  h;                      /* damping factor */
  double  gain;                   /* gain [dB] */
  double  adc;                    /* [V/LSB] */
  double  lat;                    /* latitude of station */
  double  lng;                    /* longitude of station */
  double  higt;                   /* height of station */
  double  stcp;                   /* station correction of P phase */
  double  stcs;                   /* station correction of P phase */
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

/*-
  for old format (data before May, 1990)
   (1) compressed epo data, 
   (2) no 0.5 byte data, 
   (3) not differential data, 
   (4) epo channel 0-241 (not real ch)

   used in 'win.c', 'rtape.c'.
   defined in 'fromtape.c', but not used.
   -*/
static int e_ch[241] = {
  0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
  0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
  0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
  0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
  0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
  0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
  0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
  0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
  0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
  0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
  0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
  0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
  0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
  0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
  0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
  0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
  0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
  0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
  0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
  0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
  0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
  0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
  0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
  0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
  0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
  0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
  0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
  0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
  0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC,
  0x00ED, 0x00EE, 0x00EF, 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4,
  0x00F5};

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
uint32_w win_get_chhdr(const uint8_w *, WIN_ch *, WIN_sr *);
uint32_w get_sysch(const uint8_w *, WIN_ch *);
void get_mon(WIN_sr, int32_w *, int32_w (*)[]);
uint8_w *compress_mon(int32_w *, uint8_w *);
void make_mon(uint8_w *, uint8_w *);
void t_bcd(time_t, uint8_w *);
time_t bcd_t(uint8_w *);
void time2bcd(time_t, uint8_w *);
time_t bcd2time(uint8_w *);
int time_cmpq(const void *, const void *);
void rmemo5(char [], int []);
void rmemo6(char [], int []);
int wmemo5(char [], int []);
int **i_matrix(int, int);
WIN_bs get_merge_data(uint8_w *, uint8_w *, WIN_bs *, uint8_w *, WIN_bs *);
WIN_ch get_sysch_list(uint8_w *, WIN_bs, WIN_ch []);
WIN_ch get_chlist_chfile(FILE *, WIN_ch []);
WIN_bs get_select_data(uint8_w *, WIN_ch [], WIN_ch, uint8_w *, WIN_bs);
int WIN_time_hani(char [], int [], int []);
int read_channel_file(FILE *, struct channel_tbl [], int);
void str2double(char *, int, int, double *);

/* MT device */
#if HAVE_SYS_MTIO_H
int mt_pos(int, int, int);
int read_exb1(char [], int, uint8_w *, size_t);
#endif

/* winlib_log.c */
int find_oldest(char *, char *);

#endif  /* !_WIN_LIB_H_*/
