/* $Id: win_system.h,v 1.1 2000/05/26 10:52:14 uehira Exp $ */

#ifndef _WIN_SYSTEM_H_
#define _WIN_SYSTEM_H_

#include <stdlib.h>

typedef unsigned long  WIN_blocksize;
typedef unsigned short WIN_ch;
typedef unsigned long  WIN_sr;

#define WIN_BLOCKSIZE_LEN  (sizeof(WIN_blocksize))  /* byte */
#define WIN_TIME_LEN       6  /* byte */
#define WIN_CH_LEN         (sizeof(WIN_ch))  /* byte */
#define WIN_CHHEADER_LEN   4  /* byte */
#define WIN_CHHEADERH_LEN  5  /* byte */

#define WIN_CH_MAX_NUM  65536   /* 2^16 */

/* 'events' process makes the following files */
#define EVENTS_OLDEST  "OLDEST"
#define EVENTS_LATEST  "LATEST"
#define EVENTS_BUSY    "BUSY"
#define EVENTS_COUNT   "COUNT"
#define EVENTS_FREESPACE "FREESPACE"

#define TRG_CHFILE_SUFIX ".ch"

/* 'wdisk' process makes the following files */
#define WDISK_OLDEST  "OLDEST"
#define WDISK_LATEST  "LATEST"
#define WDISK_BUSY    "BUSY"
#define WDISK_COUNT   "COUNT"
#define WDISK_MAX     "MAX"

/* 'wdiskt' process makes the following files */
#define WDISKT_OLDEST  "OLDEST"
#define WDISKT_LATEST  "LATEST"
#define WDISKT_BUSY    "BUSY"
#define WDISKT_COUNT   "COUNT"
#define WDISKT_MAX     "MAX"

/* 'insert_raw' process makes the following file */
#define INSERT_RAW_USED  "USED_RAW"

/* 'insert_trg' process makes the following file */
#define INSERT_TRG_USED  "USED_TRG"

#define WINADD  "winadd"
#define WADD    "wadd"

#define SWAPL(a) a=(((a)<<24)|((a)<<8)&0xff0000|((a)>>8)&0xff00|((a)>>24)&0xff)

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

unsigned long  mklong(unsigned char *ptr);
void  adj_time(int tm[]);
void  adj_time_m(int tm[]);
int  bcd_dec(int dest[], unsigned char *sour);
int  time_cmp(int *t1, int *t2, int i);
void rmemo5(char f[], int c[]);
void rmemo6(char f[], int c[]);
int  wmemo5(char name[], int tm[]);
WIN_ch  get_sysch_list(unsigned char *buf, WIN_blocksize bufnum,
		      WIN_ch sysch[]);
WIN_ch  get_chlist_chfile(FILE *fp, WIN_ch sysch[]);
WIN_blocksize  get_merge_data(unsigned char *mergebuf,
			      unsigned char *mainbuf, WIN_blocksize *main_num,
			      unsigned char *subbuf,  WIN_blocksize *sub_num);
WIN_blocksize get_select_data(unsigned char *selectbuf,
			      WIN_ch chlist[], WIN_ch ch_num,
			      unsigned char *buf,  WIN_blocksize buf_num);
int WIN_time_hani(char fname[], int start[], int end[]);

#endif  /*_WIN_SYSTEM_H_ */
