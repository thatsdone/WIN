/* $Id: w_macros.h,v 1.1.6.2 2011/06/01 12:14:54 uehira Exp $ */

#ifndef _WMACROS_H_
#define _WMACROS_H_

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

#define WIN_STANAME        10   /* (length of station code) */
#define WIN_STACOMP         6   /* (length of component code) */
#define WIN_LABEL          18   /* (length of label) */
#define WIN_STANAME_LEN    11   /* (length of station code)+1 */
#define WIN_STACOMP_LEN     7   /* (length of component code)+1 */
#define WIN_LABEL_LEN      19   /* (length of label)+1 */

#define SR_MON       5   /* sampling rate of MON */
#define TIME_OFFSET ((time_t)0)  /* time(0) offset */

/* MT device (fromtape.c rtape.c) */
#define   TRY_LIMIT 16
#define   TIME1   "9005151102"  /* 10 m / fm before this time */
#define   TIME2   "9005161000"  /* no fms before this time */
                    /* 60 m / fm after this time */
#define   TIME3   "9008031718"  /* 10 m / fm after this time */

/* Max. remain size of shared memory */
#define  MAX_REMAIN_SHM  ((size_t)10000000)   /* 10 MB */

/* default receipt buffer size of socket (in KB) */
#define  DEFAULT_RCVBUF  256

/* default transmit buffer size of socket (in KB) */
#define  DEFAULT_SNDBUF  256

/* structure of binary hypo file 'FinalB' (28 bytes / event) */
#define FinalB_SIZE  28

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
#define EVENTS_USED    "USED_EVENTS"

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

#define WED       "wed"
#define RTAPE     "rtape"

#endif  /* !_WMACROS_H_*/
