/* $Id: winlib.h,v 1.1.2.7.2.44.2.4 2010/12/22 14:39:57 uehira Exp $ */

#ifndef _WIN_LIB_H_
#define _WIN_LIB_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

/* check stat macros */
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef STAT_MACROS_BROKEN
#undef S_ISDIR
#endif
#ifndef S_IFMT
#define S_IFMT   0170000
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#endif

/* check SHM_R & SHM_W */
#include <sys/ipc.h>
#include <sys/shm.h>
#ifndef IPC_R
#define IPC_R           000400  /* read permission */
#endif
#ifndef IPC_W
#define IPC_W           000200  /* write/alter permission */
#endif
#ifndef SHM_R
#define SHM_R       (IPC_R)
#endif
#ifndef SHM_W
#define SHM_W       (IPC_W)
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

#include "filter.h"
#include "pltxy.h"
#include "udpu.h"
#include "ulaw.h"
#include "tcpu.h"
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

/* WIN system macros */
#include "w_macros.h"

/* memory malloc utility macro */
#ifndef MALLOC
#define MALLOC(type, n) (type *)win_xmalloc((size_t)(sizeof(type)*(n)))
#endif
#ifndef CALLOC
#define CALLOC(type, n) (type *)win_xcalloc((size_t)n, sizeof(type))
#endif
#ifndef REALLOC
#define REALLOC(type, ptr, n) \
(type *)win_xrealloc((void *)ptr, (size_t)(sizeof(type)*(n)))
#endif
#ifndef FREE
#define FREE(a)         win_xfree((void *)(a))
#endif

#define  SWAP32(a)  a = ((((a) << 24)) | (((a) << 8) & 0xff0000) |	\
			(((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))
#define  SWAP16(a)  a = ((((a) << 8) & 0xff00) | (((a) >> 8) & 0xff))
#define  MKSWAP16(a)  (uint16_w)((((a) << 8) & 0xff00) | (((a) >> 8) & 0xff))
#define  SWAPF(a)  *(int32_w *)&(a) =\
    (((*(int32_w *)&(a)) << 24) | (((*(int32_w *)&(a)) << 8) & 0xff0000) | \
     (((*(int32_w *)&(a)) >> 8) & 0xff00) | (((*(int32_w *)&(a)) >> 24) & 0xff))

/* WIN system struct declarations */
#include "w_struct.h"

/* WIN system constant */
#include "w_const.h"

#include "ls7000.h"
#include "ls8tel.h"
#include "winpickfile.h"

/** prototypes **/
#include "w_proto.h"
/* MT device */
#if HAVE_SYS_MTIO_H
int mt_pos(int, int, int);
int read_exb1(char [], int, uint8_w *, size_t);
#endif

#endif  /* !_WIN_LIB_H_*/
