/* $Id: win_system.h,v 1.7.4.3.2.6 2010/02/02 10:57:23 uehira Exp $ */

#ifndef _WIN_SYSTEM_H_
#define _WIN_SYSTEM_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "winlib.h"

/* typedef unsigned long  WIN_blocksize; */
/* typedef unsigned short WIN_ch; */
/* typedef unsigned long  WIN_sr; */

/* #define WIN_BLOCKSIZE_LEN  (sizeof(WIN_blocksize))  /\* byte *\/ */
/* #define WIN_TIME_LEN       6  /\* byte *\/ */
/* #define WIN_CH_LEN         (sizeof(WIN_ch))  /\* byte *\/ */
/* #define WIN_CHHEADER_LEN   4  /\* byte *\/ */
/* #define WIN_CHHEADERH_LEN  5  /\* byte *\/ */

/* #define WIN_CH_MAX_NUM  WIN_CHMAX   /\* 2^16 *\/ */
/* #define WIN_STANAME_LEN    11   /\* (length of station code)+1 *\/ */
/* #define WIN_STACOMP_LEN     7   /\* (length of component code)+1 *\/ */

/* High sampling rate format */
/* #define  HEADER_4B    4096     /\* SR<2^12  (   1 Hz --    4095 Hz) *\/ */
/* #define  HEADER_5B    1048576  /\* SR<2^20  (4096 Hz -- 1048575 Hz) *\/ */

/* 'events' process makes the following files */
/* #define EVENTS_OLDEST  "OLDEST" */
/* #define EVENTS_LATEST  "LATEST" */
/* #define EVENTS_BUSY    "BUSY" */
/* #define EVENTS_COUNT   "COUNT" */
/* #define EVENTS_FREESPACE "FREESPACE" */

/* #define TRG_CHFILE_SUFIX ".ch" */

/* moved to winlib.h */
/* /\* 'wdisk' process makes the following files *\/ */
/* #define WDISK_OLDEST  "OLDEST" */
/* #define WDISK_LATEST  "LATEST" */
/* #define WDISK_BUSY    "BUSY" */
/* #define WDISK_COUNT   "COUNT" */
/* #define WDISK_MAX     "MAX" */

/* moved to winlib.h (2010/1/4) */
/* /\* 'wdiskt' process makes the following files *\/ */
/* #define WDISKT_OLDEST  "OLDEST" */
/* #define WDISKT_LATEST  "LATEST" */
/* #define WDISKT_BUSY    "BUSY" */
/* #define WDISKT_COUNT   "COUNT" */
/* #define WDISKT_MAX     "MAX" */

/* moved to winlib.h (2010/2/1) */
/* 'insert_raw' process makes the following file */
/* #define INSERT_RAW_USED  "USED_RAW" */

/* moved to winlib.h (2010/2/1) */
/* 'insert_trg' process makes the following file */
/* #define INSERT_TRG_USED  "USED_TRG" */

/* #define WINADD  "winadd" */
/* #define WADD    "wadd" */

/* channel table */
/* struct channel_tbl { */
/*   WIN_ch  sysch;                  /\* channel number [2byte hex] *\/ */
/*   int     flag;                   /\* record flag *\/ */
/*   int     delay;                  /\* line delay [ms] *\/ */
/*   char    name[WIN_STANAME_LEN];  /\* station name *\/ */
/*   char    comp[WIN_STACOMP_LEN];  /\* component name *\/ */
/*   int     scale;                  /\* display scale *\/ */
/*   char    bit[256];                    /\* bit value of A/D converter *\/ */
/*   double  sens;                   /\* sensitivity of instruments *\/ */
/*   char    unit[256];              /\* unit of sensitivity *\/ */
/*   double  t0;                     /\* natural period [s] *\/ */
/*   double  h;                      /\* damping factor *\/ */
/*   double  gain;                   /\* gain [dB] *\/ */
/*   double  adc;                    /\* [V/LSB] *\/ */
/*   double  lat;                    /\* latitude of station *\/ */
/*   double  lng;                    /\* longitude of station *\/ */
/*   double  higt;                   /\* height of station *\/ */
/*   double  stcp;                   /\* station correction of P phase *\/ */
/*   double  stcs;                   /\* station correction of P phase *\/ */
/* }; */

/* void  adj_time(int tm[]); */
/* void  adj_time_m(int tm[]); */
/* int  bcd_dec(int dest[], unsigned char *sour); */
/* int  time_cmp(int *t1, int *t2, int i); */
/* void rmemo5(char f[], int c[]); */
/* void rmemo6(char f[], int c[]); */
/* int  wmemo5(char name[], int tm[]); */
/* WIN_ch  get_sysch_list(uint8_w *buf, WIN_bs bufnum, */
/* 		      WIN_ch sysch[]); */
/* WIN_ch  get_chlist_chfile(FILE *fp, WIN_ch sysch[]); */
/* WIN_bs  get_merge_data(uint8_w *mergebuf, */
/* 			      uint8_w *mainbuf, WIN_bs *main_num, */
/* 			      uint8_w *subbuf,  WIN_bs *sub_num); */
/* WIN_bs get_select_data(uint8_w *selectbuf, */
/* 			      WIN_ch chlist[], WIN_ch ch_num, */
/* 			      uint8_w *buf,  WIN_bs buf_num); */
/* int WIN_time_hani(char fname[], int start[], int end[]); */
/* int read_channel_file(FILE *, struct channel_tbl [], int); */
/* int **i_matrix(int, int); */
/* WIN_bs read_onesec_win(FILE *, uint8_w **); */
/* WIN_bs win_get_chhdr(uint8_w *, WIN_ch *, WIN_sr *); */

/* int win2fix(unsigned char *, long *, long *, long *); */
/* int winform(long *, unsigned char *, int, unsigned short); */

#endif  /*_WIN_SYSTEM_H_ */
