/* $Id: winlib.h,v 1.1.2.2 2007/06/19 11:19:40 uehira Exp $ */

#ifndef _WIN_LIB_H_
#define _WIN_LIB_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "filter.h"
#include "win_log.h"

typedef unsigned long  WIN_blocksize;
typedef unsigned short WIN_ch;
typedef unsigned long  WIN_sr;

/* High sampling rate format */
#define  HEADER_4B    4096     /* SR<2^12  (   1 Hz --    4095 Hz) */
#define  HEADER_5B    1048576  /* SR<2^20  (4096 Hz -- 1048575 Hz) */

/** prototypes **/
void get_time(int []);
unsigned long mklong(unsigned char *);
unsigned short mkshort(unsigned char *);
int bcd_dec(int [], unsigned char *);
void dec_bcd(unsigned char *, unsigned int *);
void adj_time_m(int []);
void adj_time(int []);
int time_cmp(int *, int *, int);
int winform(long *, unsigned char *, int, unsigned short);
int win2fix(unsigned char *, long *, long *, long *);

#endif  /* !_WIN_LIB_H_*/
