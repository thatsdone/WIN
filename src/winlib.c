/* $Id: winlib.c,v 1.12 2019/03/18 06:08:28 urabe Exp $ */

/*-
 * winlib.c  (Uehira Kenji)
 *  win system library
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#if HAVE_SYS_MTIO_H
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

#include "winlib.h"

#define BUF_SIZE 1024

#define DEBUG1 0

/**********************************************************
 * Local function
 ********************************************************** */

static time_t mktime2(struct tm *);
static WIN_bs winform5(int32_w *, uint8_w *, WIN_sr, WIN_ch, int);

static time_t
mktime2(struct tm *mt) /* high-speed version of mktime() */
{
  static struct tm *m;
  time_t t;
  register int i,j,ye;
  static int dm[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static int dy[72] = {
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 1970-81 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 1982-93 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 1994-2005 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 2006-17 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 2018-2029 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365};/* 2030-2041 */
#if defined(__SVR4)
  extern time_t timezone;
#endif

  if (m == NULL) {
    if (time(&t) == (time_t)-1)
      return (-1);
    m = localtime(&t);
  }
  ye = mt->tm_year - 70;
  j = 0;
  for (i = 0; i < ye; i++)
    j += dy[i]; /* days till the previous year */
  for (i = 0; i < mt->tm_mon; i++)
    j += dm[i]; /* days till the previous month */
  if (!(mt->tm_year & 0x3) && mt->tm_mon > 1)
    j++;  /* in a leap year */
  j += mt->tm_mday - 1; /* days */

#if defined(__SVR4)
  return (j * 86400 + mt->tm_hour * 3600 + mt->tm_min * 60 + mt->tm_sec + timezone);
#endif
#if defined(HAVE_STRUCT_TM_TM_GMTOFF)
  return (j * 86400 + mt->tm_hour * 3600 + mt->tm_min * 60 + mt->tm_sec - m->tm_gmtoff);
#endif
#if defined(__CYGWIN__)
  tzset();
  return (j * 86400 + mt->tm_hour * 3600 + mt->tm_min * 60 + mt->tm_sec + _timezone);
#endif
}

/* winform.c  4/30/91, 99.4.19   urabe */
/* 2005.3.9 high sampling rate uehira */
/* 2015.11.24 fixed-sample-size-data in case of 4 bytes uehira */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */
static WIN_bs
winform5(int32_w *inbuf, uint8_w *outbuf, WIN_sr sr, WIN_ch sys_ch, int fixf)
/* int32_w  *inbuf;   : input data array for one sec */
/* uint8_w  *outbuf;  : output data array for one sec */
/* uint32_w sr;       : n of data (i.e. sampling rate) */
/* uint16_w sys_ch;   : 16 bit long channel ID number */
/* int      fixf      : Always 4byte length data */
{
  int		  byte_leng;
  int32_w	  dmin , dmax, aa, bb, br;
  int32_w        *ptr;
  uint8_w        *buf;
  uint32_w	  i;
  int             orflag;

  if (sr >= HEADER_5B)
    exit(1);		/* sampling rate is out of range */

  if (fixf)  /* fix length mode */
    byte_leng = 5;
  else {
    /* obtain min and max */
    orflag = 0;
    dmax = dmin = 0;
    for (i = 1; i < sr; i++) {
      if (check_4byte_diff(inbuf[i], inbuf[i - 1])) {
	orflag = 1;  /* out of range */
	break;
      }
      br = inbuf[i] - inbuf[i - 1];
      if (br > dmax)
	dmax = br;
      else if (br < dmin)
	dmin = br;
    }

    /* determine sample size */
    if (orflag)
      byte_leng = 5;
    else {
      if (((dmin & 0xfffffff8) == 0xfffffff8 || (dmin & 0xfffffff8) == 0) &&
	  ((dmax & 0xfffffff8) == 0xfffffff8 || (dmax & 0xfffffff8) == 0))
	byte_leng = 0;
      else if (((dmin & 0xffffff80) == 0xffffff80 || (dmin & 0xffffff80) == 0) &&
	       ((dmax & 0xffffff80) == 0xffffff80 || (dmax & 0xffffff80) == 0))
	byte_leng = 1;
      else if (((dmin & 0xffff8000) == 0xffff8000 || (dmin & 0xffff8000) == 0) &&
	       ((dmax & 0xffff8000) == 0xffff8000 || (dmax & 0xffff8000) == 0))
	byte_leng = 2;
      else if (((dmin & 0xff800000) == 0xff800000 || (dmin & 0xff800000) == 0) &&
	       ((dmax & 0xff800000) == 0xff800000 || (dmax & 0xff800000) == 0))
	byte_leng = 3;
      else
	byte_leng = 5;
    }  /* if (orflag) */
  }  /* if (fixf) */
  
  /* differentiate */
  if (byte_leng != 5) {
    ptr = inbuf;
    bb = (*ptr++);
    for (i = 1; i < sr; i++) {
      aa = (*ptr);
      *ptr++ = aa - bb;
      bb = aa;
    }
  }

  /* High sampling rate */
  if (HEADER_4B <= sr)
    byte_leng += 8;

  /* make a 4 or 5 byte long header */
  buf = outbuf;
  *buf++ = (sys_ch >> 8) & 0xff;
  *buf++ = sys_ch & 0xff;
  if (sr < HEADER_4B) {		/* 4 byte header */
    *buf++ = (byte_leng << 4) | (sr >> 8);
    *buf++ = sr & 0xff;
  } else {			/* 5 byte header */
    *buf++ = (byte_leng << 4) | (sr >> 16);
    *buf++ = (sr >> 8) & 0xff;
    *buf++ = sr & 0xff;
  }

  /* first sample is always 4 byte long */
  *buf++ = inbuf[0] >> 24;
  *buf++ = inbuf[0] >> 16;
  *buf++ = inbuf[0] >> 8;
  *buf++ = inbuf[0];
  /* second and after */
  byte_leng %= 8;
  switch (byte_leng) {
  case 0:
    for (i = 1; i < sr - 1; i += 2)
      *buf++ = (inbuf[i] << 4) | (inbuf[i + 1] & 0xf);
    if (i == sr - 1)
      *buf++ = (inbuf[i] << 4);
    break;
  case 1:
    for (i = 1; i < sr; i++)
      *buf++ = inbuf[i];
    break;
  case 2:
    for (i = 1; i < sr; i++) {
      *buf++ = inbuf[i] >> 8;
      *buf++ = inbuf[i];
    }
    break;
  case 3:
    for (i = 1; i < sr; i++) {
      *buf++ = inbuf[i] >> 16;
      *buf++ = inbuf[i] >> 8;
      *buf++ = inbuf[i];
    }
    break;
  case 5:
    for (i = 1; i < sr; i++) {
      *buf++ = inbuf[i] >> 24;
      *buf++ = inbuf[i] >> 16;
      *buf++ = inbuf[i] >> 8;
      *buf++ = inbuf[i];
    }
    break;
  }

  return ((WIN_bs) (buf - outbuf));
}
/******- End of Local function -******/

/**********************************************************
 * Global functions
 ********************************************************** */
/* function(s) related with MT device */
#if HAVE_SYS_MTIO_H
int
mt_pos(int fmc, int blc, int fd)
{
  struct mtop exb_param;
  int re;

  re = 0;
  if (fmc) {
    if (fmc > 0) {
      exb_param.mt_op = MTFSF;
      exb_param.mt_count = fmc;
    } else {
      exb_param.mt_op = MTBSF;
      exb_param.mt_count = (-fmc);
    }
    if ((re = ioctl(fd, MTIOCTOP, (char *)&exb_param)) == -1) {
      perror("error in space fms");
      printf("processing continues ... ");
      fflush(stdout);
      return (re);
    }
  }
  if (blc) {
    if (blc > 0) {
      exb_param.mt_op = MTFSR;
      exb_param.mt_count = blc;
    } else {
      exb_param.mt_op = MTBSR;
      exb_param.mt_count = (-blc);
    }
    if ((re = ioctl(fd, MTIOCTOP, (char *)&exb_param)) == -1) {
      perror("error in space records");
      printf("processing continues ... ");
      fflush(stdout);
      return (re);
    }
  }
  return (re);
}

int
read_exb1(char dev_name[], int fd, uint8_w *dbuf, size_t maxsize)
{
  int       cnt, blocking, dec[6];
  ssize_t   re;
  uint32_w  size;
  uint8_w   *ptr;

  blocking = cnt = 0;
  while (1) {
    while ((re = read(fd, dbuf, maxsize)) == 0) {	/* file mark */
      close(fd);
      if ((fd = open(dev_name, O_RDONLY)) == -1) {
	perror("exabyte unit cannot open : ");
	exit(1);
      }
    }
    if (re > 0) {
      blocking = 1;
      size = mkuint4(dbuf);
      /* if(size<0) continue; */   /* nonsense! */
      if (bcd_dec(dec, dbuf + 4) == 0)
	continue;
#if DEBUG
      printf("(%zd/%u)", re, size);
#endif
      ptr = dbuf;
      while (size > re) {
	re = read(fd, ptr += re, size -= re);
	blocking++;
#if DEBUG
	printf("(%zd/%u)", re, size);
#endif
      }
      if (re > 0)
	break;
    }
    /* error */
    perror("exabyte");
    cnt++;
    if (cnt == TRY_LIMIT / 2)
      mt_pos(-2, 0, fd);	/* overrun ? */
    else if (cnt == TRY_LIMIT)
      return (-1);
  }   /* for(;;) */

  return (blocking);
}
#endif  /* #if HAVE_SYS_MTIO_H */

time_t
get_time(int rt[])
{
  struct tm      *nt;
  time_t	  ltime;

  (void)time(&ltime);
  nt = localtime(&ltime);
  rt[0] = nt->tm_year % 100;
  rt[1] = nt->tm_mon + 1;
  rt[2] = nt->tm_mday;
  rt[3] = nt->tm_hour;
  rt[4] = nt->tm_min;
  rt[5] = nt->tm_sec;
  return (ltime);
}

/*-
 * mkuint4
 *  make big-endian 4 bytes unsigned int
 */
uint32_w
mkuint4(const uint8_w *ptr)
{
  uint32_w	  a;

  a = ((ptr[0] << 24) & 0xff000000) + ((ptr[1] << 16) & 0xff0000) +
    ((ptr[2] << 8) & 0xff00) + (ptr[3] & 0xff);
  return (a);
}

/*-
 * mkuint2
 *  make big-endian 2 bytes unsigned int
 */
uint16_w
mkuint2(const uint8_w *ptr)
{
  uint16_w	  a;

  a = ((ptr[0] << 8) & 0xff00) + (ptr[1] & 0xff);

  return (a);
}

/*-
 * bcd_dec()
 *  convert BCD to DEC
 */
int
bcd_dec(int dest[], uint8_w *sour)
{
  int		  i;

  i = b2d[sour[0]];
  if (i >= 0 && i <= 99)
    dest[0] = i;
  else
    return (0);

  i = b2d[sour[1]];
  if
    (i >= 1 && i <= 12)
    dest[1] = i;
  else
    return (0);

  i = b2d[sour[2]];
  if (i >= 1 && i <= 31)
    dest[2] = i;
  else
    return (0);

  i = b2d[sour[3]];
  if (i >= 0 && i <= 23)
    dest[3] = i;
  else
    return (0);

  i = b2d[sour[4]];
  if (i >= 0 && i <= 59)
    dest[4] = i;
  else
    return (0);

  i = b2d[sour[5]];
  if (i >= 0 && i <= 60)
    dest[5] = i;
  else
    return (0);

  return (1);
}

int
dec_bcd(uint8_w *dest, int *sour)
{
  int		  cntr;

  for (cntr = 0; cntr < 6; cntr++) {
    /*
     * dest[cntr] = (((sour[cntr] / 10) << 4) & 0xf0) | (sour[cntr] % 10 &
     * 0xf);
     */
    if (sour[cntr] > 99)
      return (0);
    dest[cntr] = d2b[sour[cntr]];
  }

  return (1);
}

/*-
 * adj_time_m
 */
void
adj_time_m(int tm[])
{

  if (tm[4] == 60) {
    tm[4] = 0;
    if (++tm[3] == 24) {
      tm[3] = 0;
      tm[2]++;
      switch (tm[1]) {
      case 2:
	if (tm[0] % 4 == 0) {
	  if (tm[2] == 30) {
	    tm[2] = 1;
	    tm[1]++;
	  }
	  break;
	} else {
	  if (tm[2] == 29) {
	    tm[2] = 1;
	    tm[1]++;
	  }
	  break;
	}
      case 4:
      case 6:
      case 9:
      case 11:
	if (tm[2] == 31) {
	  tm[2] = 1;
	  tm[1]++;
	}
	break;
      default:
	if (tm[2] == 32) {
	  tm[2] = 1;
	  tm[1]++;
	}
	break;
      }
      if (tm[1] == 13) {
	tm[1] = 1;
	if (++tm[0] == 100)
	  tm[0] = 0;
      }
    }
  } else if (tm[4] == -1) {
    tm[4] = 59;
    if (--tm[3] == -1) {
      tm[3] = 23;
      if (--tm[2] == 0) {
	switch (--tm[1]) {
	case 2:
	  if (tm[0] % 4 == 0)
	    tm[2] = 29;
	  else
	    tm[2] = 28;
	  break;
	case 4:
	case 6:
	case 9:
	case 11:
	  tm[2] = 30;
	  break;
	default:
	  tm[2] = 31;
	  break;
	}
	if (tm[1] == 0) {
	  tm[1] = 12;
	  if (--tm[0] == -1)
	    tm[0] = 99;
	}
      }
    }
  }
}

/*-
 * adj_time
 */
void
adj_time(int tm[])
{

  if (tm[5] == 60) {
    tm[5] = 0;
    if (++tm[4] == 60) {
      tm[4] = 0;
      if (++tm[3] == 24) {
	tm[3] = 0;
	tm[2]++;
	switch (tm[1]) {
	case 2:
	  if (tm[0] % 4 == 0) {	/* leap year */
	    if (tm[2] == 30) {
	      tm[2] = 1;
	      tm[1]++;
	    }
	    break;
	  } else {
	    if (tm[2] == 29) {
	      tm[2] = 1;
	      tm[1]++;
	    }
	    break;
	  }
	case 4:
	case 6:
	case 9:
	case 11:
	  if (tm[2] == 31) {
	    tm[2] = 1;
	    tm[1]++;
	  }
	  break;
	default:
	  if (tm[2] == 32) {
	    tm[2] = 1;
	    tm[1]++;
	  }
	  break;
	}
	if (tm[1] == 13) {
	  tm[1] = 1;
	  if (++tm[0] == 100)
	    tm[0] = 0;
	}
      }
    }
  } else if (tm[5] == -1) {
    tm[5] = 59;
    if (--tm[4] == -1) {
      tm[4] = 59;
      if (--tm[3] == -1) {
	tm[3] = 23;
	if (--tm[2] == 0) {
	  switch (--tm[1]) {
	  case 2:
	    if (tm[0] % 4 == 0)
	      tm[2] = 29;
	    else
	      tm[2] = 28;
	    break;
	  case 4:
	  case 6:
	  case 9:
	  case 11:
	    tm[2] = 30;
	    break;
	  default:
	    tm[2] = 31;
	    break;
	  }
	  if (tm[1] == 0) {
	    tm[1] = 12;
	    if (--tm[0] == -1)
	      tm[0] = 99;
	  }
	}
      }
    }
  }
}

void
adj_sec(int *tm, double *se, int *tmc, double *sec)
{
  int		  i       , j;

  for (i = 0; i < 5; i++)
    tmc[i] = tm[i];
  if ((*sec = (*se)) < 0.0) {
    tmc[5] = 0;
    i = (int)(-(*sec));
    if ((double)i == (-(*sec)))
      i--;
    i++;
    for (j = 0; j < i; j++) {
      tmc[5]--;
      adj_time(tmc);
    }
    *sec += (double)(tmc[5] + i);
  } else
    tmc[5] = (int)(*sec);
  tmc[6] = (int)(*sec * 1000.0) % 1000;
}

int
time_cmp(int *t1, int *t2, int i)
{
  int		  cntr;

  cntr = 0;
  if (t1[cntr] < WIN_YEAR && t2[cntr] > WIN_YEAR)
    return (1);
  if (t1[cntr] > WIN_YEAR && t2[cntr] < WIN_YEAR)
    return (-1);
  for (; cntr < i; cntr++) {
    if (t1[cntr] > t2[cntr])
      return (1);
    if (t1[cntr] < t2[cntr])
      return (-1);
  }
  return (0);
}

/* wrapper function for winform() and winform5() */
WIN_bs
mk_windata(int32_w *inbuf, uint8_w *outbuf, WIN_sr sr, WIN_ch sys_ch, int mode, int fix_flag)
/* int32_w  *inbuf;   : input data array for one sec */
/* uint8_w  *outbuf;  : output data array for one sec */
/* WIN_sr   sr;       : n of data (i.e. sampling rate) */
/* WIN_ch   sys_ch;   : 16 bit long channel ID number */
/* int      mode      : 0 : use winform(). 1 : use winform5() */
/* int      fix_flag  : 1 : always fixed length data (Only affect mode=1.). */
{

  if (mode == 0)
    return (winform(inbuf, outbuf, sr, sys_ch));
  else
    return (winform5(inbuf, outbuf, sr, sys_ch, fix_flag));
}


/* winform.c  4/30/91, 99.4.19   urabe */
/* 2005.3.9 high sampling rate uehira */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */
WIN_bs
winform(int32_w *inbuf, uint8_w *outbuf, WIN_sr sr, WIN_ch sys_ch)
/* int32_w  *inbuf;   : input data array for one sec */
/* uint8_w  *outbuf;  : output data array for one sec */
/* uint32_w sr;       : n of data (i.e. sampling rate) */
/* uint16_w sys_ch;   : 16 bit long channel ID number */
{
  int		  byte_leng;
  int32_w	  dmin , dmax, aa, bb, br;
  int32_w        *ptr;
  uint8_w        *buf;
  uint32_w	  i;

  if (sr >= HEADER_5B)
    exit(1);		/* sampling rate is out of range */

  /* differentiate and obtain min and max */
  ptr = inbuf;
  bb = (*ptr++);
  dmax = dmin = 0;
  for (i = 1; i < sr; i++) {
    aa = (*ptr);
    /* check amp diff is out of range or not. */
    if (check_4byte_diff(aa, bb)) {
      (void)fprintf(stderr, "winform(): Out of range of diff. data!\n");
      exit(1);
    }
    *ptr++ = br = aa - bb;
    bb = aa;
    if (br > dmax)
      dmax = br;
    else if (br < dmin)
      dmin = br;
  }

  /* determine sample size */
  if (((dmin & 0xfffffff8) == 0xfffffff8 || (dmin & 0xfffffff8) == 0) &&
      ((dmax & 0xfffffff8) == 0xfffffff8 || (dmax & 0xfffffff8) == 0))
    byte_leng = 0;
  else if (((dmin & 0xffffff80) == 0xffffff80 || (dmin & 0xffffff80) == 0) &&
	   ((dmax & 0xffffff80) == 0xffffff80 || (dmax & 0xffffff80) == 0))
    byte_leng = 1;
  else if (((dmin & 0xffff8000) == 0xffff8000 || (dmin & 0xffff8000) == 0) &&
	   ((dmax & 0xffff8000) == 0xffff8000 || (dmax & 0xffff8000) == 0))
    byte_leng = 2;
  else if (((dmin & 0xff800000) == 0xff800000 || (dmin & 0xff800000) == 0) &&
	   ((dmax & 0xff800000) == 0xff800000 || (dmax & 0xff800000) == 0))
    byte_leng = 3;
  else
    byte_leng = 4;

  if (HEADER_4B <= sr)
    byte_leng += 8;

  /* make a 4 or 5 byte long header */
  buf = outbuf;
  *buf++ = (sys_ch >> 8) & 0xff;
  *buf++ = sys_ch & 0xff;
  if (sr < HEADER_4B) {		/* 4 byte header */
    *buf++ = (byte_leng << 4) | (sr >> 8);
    *buf++ = sr & 0xff;
  } else {			/* 5 byte header */
    *buf++ = (byte_leng << 4) | (sr >> 16);
    *buf++ = (sr >> 8) & 0xff;
    *buf++ = sr & 0xff;
  }

  /* first sample is always 4 byte long */
  *buf++ = inbuf[0] >> 24;
  *buf++ = inbuf[0] >> 16;
  *buf++ = inbuf[0] >> 8;
  *buf++ = inbuf[0];
  /* second and after */
  byte_leng %= 8;
  switch (byte_leng) {
  case 0:
    for (i = 1; i < sr - 1; i += 2)
      *buf++ = (inbuf[i] << 4) | (inbuf[i + 1] & 0xf);
    if (i == sr - 1)
      *buf++ = (inbuf[i] << 4);
    break;
  case 1:
    for (i = 1; i < sr; i++)
      *buf++ = inbuf[i];
    break;
  case 2:
    for (i = 1; i < sr; i++) {
      *buf++ = inbuf[i] >> 8;
      *buf++ = inbuf[i];
    }
    break;
  case 3:
    for (i = 1; i < sr; i++) {
      *buf++ = inbuf[i] >> 16;
      *buf++ = inbuf[i] >> 8;
      *buf++ = inbuf[i];
    }
    break;
  case 4:
    for (i = 1; i < sr; i++) {
      *buf++ = inbuf[i] >> 24;
      *buf++ = inbuf[i] >> 16;
      *buf++ = inbuf[i] >> 8;
      *buf++ = inbuf[i];
    }
    break;
  }

  return ((WIN_bs) (buf - outbuf));
}

/* returns group size in bytes */
/* High sampling rate version */
uint32_w
win2fix(uint8_w *ptr, int32_w *abuf, WIN_ch *sys_ch, WIN_sr *sr)
/* uint8_w   *ptr;     : input */
/* int32_w   *abuf;    : output */
/* WIN_ch    *sys_ch;  : sys_ch */
/* WIN_sr    *sr;      : sr */
{
  int             b_size;
  uint32_w	  g_size;
  uint32_w	  i;
  WIN_sr	  s_rate;
  uint8_w        *dp;
  int16_w	  shreg;
  int32_w	  inreg;

  g_size = win_chheader_info(ptr, sys_ch, sr, &b_size);
  s_rate = *sr;
  if ((ptr[2] & 0x80) == 0x0)	/* channel header = 4 byte */
    dp = ptr + 4;
  else			        /* channel header = 5 byte */
    dp = ptr + 5;

  /* read group */
  abuf[0] = ((dp[0] << 24) & 0xff000000) + ((dp[1] << 16) & 0xff0000)
    + ((dp[2] << 8) & 0xff00) + (dp[3] & 0xff);
  if (s_rate == 1)
    return (g_size);		/* normal return */

  dp += 4;
  switch (b_size) {
  case 0:
    for (i = 1; i < s_rate; i += 2) {
      abuf[i] = abuf[i - 1] + ((*(int8_w *) dp) >> 4);
      if (i == s_rate - 1)
	break;
      abuf[i + 1] = abuf[i] + (((int8_w) (*(dp++) << 4)) >> 4);
    }
    break;
  case 1:
    for (i = 1; i < s_rate; i++)
      abuf[i] = abuf[i - 1] + (*(int8_w *) (dp++));
    break;
  case 2:
    for (i = 1; i < s_rate; i++) {
      shreg = ((dp[0] << 8) & 0xff00) + (dp[1] & 0xff);
      dp += 2;
      abuf[i] = abuf[i - 1] + shreg;
    }
    break;
  case 3:
    for (i = 1; i < s_rate; i++) {
      inreg = ((dp[0] << 24) & 0xff000000) + ((dp[1] << 16) & 0xff0000)
	+ ((dp[2] << 8) & 0xff00);
      dp += 3;
      abuf[i] = abuf[i - 1] + (inreg >> 8);
    }
    break;
  case 4:
    for (i = 1; i < s_rate; i++) {
      inreg = ((dp[0] << 24) & 0xff000000) + ((dp[1] << 16) & 0xff0000)
	+ ((dp[2] << 8) & 0xff00) + (dp[3] & 0xff);
      dp += 4;
      abuf[i] = abuf[i - 1] + inreg;
    }
    break;
  case 5:
    for (i = 1; i < s_rate; i++) {
      abuf[i] = ((dp[0] << 24) & 0xff000000) + ((dp[1] << 16) & 0xff0000)
	+ ((dp[2] << 8) & 0xff00) + (dp[3] & 0xff);
      dp += 4;
    }
    break;
  default:
    return (0);			/* bad header */
  }

  return (g_size);		/* normal return */
}

int
strncmp2(char *s1, char *s2, int i)
{

  if ((*s1 >= '0' && *s1 <= '5') && (*s2 <= '9' && *s2 >= '6'))
    return (1);
  else if ((*s1 <= '9' && *s1 >= '7') && (*s2 >= '0' && *s2 <= '6'))
    return (-1);
  else
    return (strncmp(s1, s2, i));
}

int
strcmp2(char *s1, char *s2)
{

  if ((*s1 >= '0' && *s1 <= '5') && (*s2 <= '9' && *s2 >= '6'))
    return (1);
  else if ((*s1 <= '9' && *s1 >= '7') && (*s2 >= '0' && *s2 <= '6'))
    return (-1);
  else
    return (strcmp(s1, s2));
}

WIN_bs
read_onesec_win(FILE *fp, uint8_w **rbuf, size_t *rbuf_size)
{
  uint8_w        sz[WIN_BSLEN];
  WIN_bs         size;

  if (fread(sz, 1, WIN_BSLEN, fp) != WIN_BSLEN)
    return (0);
  size = mkuint4(sz);

  if (*rbuf == NULL)
    *rbuf_size = 0;
  /* (void)fprintf(stderr, "%u %zu\n", size, *rbuf_size); */
  if (size > *rbuf_size) {
    *rbuf_size = (size << 1);
    *rbuf = REALLOC(uint8_w, *rbuf, *rbuf_size);
    /* (void)fprintf(stderr, "ZZ %u %zu\n", size, *rbuf_size); */
    if (*rbuf == NULL) {
      (void)fprintf(stderr, "read_onesec_win() : %s \n", strerror(errno));
      exit(1);
    }
  }

  (void)memcpy(*rbuf, sz, WIN_BSLEN);
  if (fread(*rbuf + WIN_BSLEN, 1, size - WIN_BSLEN, fp) != size - WIN_BSLEN)
    return (0);

#if DEBUG > 2
  fprintf(stderr,
	  "%02x%02x%02x%02x%02x%02x %d\n",
	  (*rbuf)[4], (*rbuf)[5], (*rbuf)[6],(*rbuf)[7], (*rbuf)[8],
	  (*rbuf)[9], size);
#endif

  return (size);
}

WIN_bs
read_onesec_win2(FILE *fp, uint8_w **in_buf, uint8_w **out_buf, size_t *size)
{
  WIN_bs  re, reout;
  uint8_w  tmpa[4];

  if (fread(tmpa, 1, WIN_BSLEN, fp) != WIN_BSLEN)
    return (0);
  re = mkuint4(tmpa);
  if (*in_buf == NULL) {
    *in_buf = MALLOC(uint8_w, (*size = (size_t)(re << 1)));
    *out_buf = MALLOC(uint8_w, (*size = (size_t)(re << 1)));
  } else if (re > *size) {
    *in_buf = REALLOC(uint8_w, *in_buf, (*size = (size_t)(re << 1)));
    *out_buf = REALLOC(uint8_w, *out_buf, (*size = (size_t)(re << 1)));
  }
  if ((*in_buf == NULL) || (*out_buf == NULL)) {
    perror("read_onesec_win2()");
    exit(1);
  }
  
  /* copy data to *in_buf() */
  (void)memcpy(*in_buf, tmpa, WIN_BSLEN);
  reout = fread(*in_buf + WIN_BSLEN, 1, re - WIN_BSLEN, fp);
  
  if (reout != re - WIN_BSLEN)
    return (0);
  else
    return (re);  /* previous version : return (reout) */
}

/* init shared memory */
void
Shm_init(struct Shm *sh, size_t size)
{
  size_t  remain;

  remain = (size - sizeof(*sh)) / 10;
  if (remain > MAX_REMAIN_SHM)
    remain = MAX_REMAIN_SHM;

  sh->p = 0; 
  /* sh->pl = (size - sizeof(*sh)) / 10 * 9; */
  sh->pl = size - sizeof(*sh) - remain;
  sh->c = 0;
  sh->r = (-1);
#if DEBUG
  (void)fprintf(stderr, "size = %zu, sh->pl = %zu(%zu), remain = %zu\n",
		size, sh->pl, (size - sizeof(*sh)) / 10 * 9, remain);
#endif
}

/* attach shared memory for read (Read-only) */
struct Shm *
Shm_read_offline(key_t shmkey)
{
  struct Shm  *shm;
  int  shmid, aflag;

  if ((shmid = shmget(shmkey, 0, 0)) == -1) {
    (void)fprintf(stderr, "shmget : %s\n", strerror(errno));
    exit(1);
  }

  aflag = SHM_RDONLY;
  /* aflag = 0;  original */
  if ((shm = (struct Shm *)shmat(shmid, NULL, aflag)) == (struct Shm *)-1) {
    (void)fprintf(stderr, "shmat : %s\n", strerror(errno));
    exit(1);
  }

#if DEBUG
  (void)fprintf(stderr,
 		"Shm_read_offline : key=%ld id=%d (%p)\n",
		shmkey, shmid, shm);
#endif

  return (shm);
}

struct Shm *
Shm_create_offline(key_t shmkey, size_t shmsize)
{
  struct Shm  *shm;
  int  shmid, oflag;

  oflag = IPC_CREAT;
  oflag |= SHM_R |  SHM_W;  /* user permission */
  oflag |= (SHM_R>>3);      /* group permission */
  oflag |= (SHM_R>>6);      /* other permission */
  /* oflag |= (SHM_R>>3) | (SHM_W>>3);   group permission */
  /* oflag |= (SHM_R>>6) | (SHM_W>>6);   other permission */

  if ((shmid = shmget(shmkey, shmsize, oflag)) == -1) {
    (void)fprintf(stderr, "shmget : %s\n", strerror(errno));
    exit(1);
  }

  if ((shm = (struct Shm *)shmat(shmid, NULL, 0)) == (struct Shm *)-1) {
    (void)fprintf(stderr, "shmat : %s\n", strerror(errno));
    exit(1);
  }

#if DEBUG
  (void)fprintf(stderr,
		"Shm_create_offline : key=%ld id=%d size=%zu (%p)",
		shmkey, shmid, shmsize, shm);
#endif

  return (shm);
}

/* print version */
void
WIN_version(void)
{

/* #if defined(__LP64__) || defined(_LP64) */
#if (SIZEOF_LONG == 8) && (SIZEOF_INT_P == 8)
  (void)fprintf(stderr, "%s package Version %s [64-bit]\n", PACKAGE, VERSION);
#else
  (void)fprintf(stderr, "%s package Version %s [32-bit]\n", PACKAGE, VERSION);
#endif

#if SSIZE5_MODE == 1
  (void)fprintf(stderr, "Default output 4 byte sample size : 5\n");
#else
  (void)fprintf(stderr, "Default output 4 byte sample size : 4\n");
#endif
}

/*-
 * channel header information (High sampling rate version)
 *  return group size (byte)
 * input  : *ptr
 * output : ch  : channel number
 *          sr  : sampling rate
 *          ss  : sample size (0-->0.5byte)
 *                            (1-->  1byte)
 *                            (2-->  2byte)
 *                            (3-->  3byte)
 *                            (4-->  4byte)
 *                            (5-->  4byte) 4 bytes NO-diff. values.
 -*/
uint32_w
win_chheader_info(const uint8_w *ptr, WIN_ch *ch, WIN_sr *sr, int *ss)
{
  WIN_bs    gsize;
  int       sample_size;

  /* channel number */
  *ch = (((WIN_ch)ptr[0]) << 8) + (WIN_ch)ptr[1];

  /* samping rate */
  if ((ptr[2] & 0x80) == 0x0) /* channel header = 4 byte */
    *sr = (WIN_sr)ptr[3] + (((WIN_sr)(ptr[2] & 0x0f)) << 8);
  else                        /* channel header = 5 byte */
    *sr = (WIN_sr)ptr[4] + (((WIN_sr)ptr[3]) << 8)
      + (((WIN_sr)(ptr[2] & 0x0f)) << 16);

  /* sample size */
  *ss = (ptr[2] >> 4) & 0x7;
  
  /* goupe size */
  sample_size = (*ss == 5) ? 4 : *ss;
  if (sample_size)
    gsize = sample_size * (*sr - 1) + 8;
  else
    gsize = ((*sr) >> 1) + 8;
  if (ptr[2] & 0x80)
    gsize++;

  return (gsize);
}
/** ORIGINAL VERSION **/
/* WIN_bs */
/* win_chheader_info(const uint8_w *ptr, WIN_ch *ch, WIN_sr *sr, int *ss) */
/* { */
/*   uint32_w  gh; */
/*   WIN_bs    gsize; */

/*   gh = mkuint4(ptr); */
/*   *ch = (WIN_ch)((gh >> 16) & 0xffff); */
/*   *sr = gh & 0xfff; */
/*   *ss = (gh >> 12) & 0xf; */
/*   if (*ss) */
/*     gsize = *ss * (*sr - 1) + 8; */
/*   else */
/*     gsize = ((*sr) >> 1) + 8; */

/*   return (gsize); */
/* } */

/* same as win_chheader_info(), but NOT return sample size info. */
uint32_w
win_get_chhdr(const uint8_w *ptr, WIN_ch *chnum, WIN_sr *sr)
{
  WIN_bs	  gsize;
  int             ss;

  gsize = win_chheader_info(ptr, chnum, sr, &ss);

  return (gsize);
}
/** ORIGINAL VERSION **/
/* /\* High sampling rate version *\/ */
/* WIN_bs */
/* win_get_chhdr(uint8_w *ptr, WIN_ch *chnum, WIN_sr *sr) */
/* { */
/*   WIN_bs  gsize; */

/*   /\* channel number *\/ */
/*   *chnum = (((WIN_ch)ptr[0]) << 8) + (WIN_ch)ptr[1]; */

/*   /\* sampling rate *\/ */
/*   if ((ptr[2] & 0x80) == 0x0) /\* channel header = 4 byte *\/ */
/*     *sr = (WIN_sr)ptr[3] + (((WIN_sr)(ptr[2] & 0x0f)) << 8); */
/*   else                        /\* channel header = 5 byte *\/ */
/*     *sr = (WIN_sr)ptr[4] + (((WIN_sr)ptr[3]) << 8) */
/*       + (((WIN_sr)(ptr[2] & 0x0f)) << 16); */

/*   /\* size *\/ */
/*   if ((ptr[2] >> 4) & 0x7) */
/*     gsize = ((ptr[2] >> 4) & 0x7) * (*sr - 1) + 8; */
/*   else */
/*     gsize = (*sr >> 1) + 8; */
/*   if (ptr[2] & 0x80) */
/*     gsize++; */

/*   return (gsize); */
/* } */

/* same as win_chheader_info(), but return only channel number info. */
uint32_w
get_sysch(const uint8_w *buf, WIN_ch *ch)
{
  /* uint8_w  gh[4]; */
  WIN_sr   sr;
  uint32_w gsize;
  int  i;
  
  /* for(i=0;i<4;++i) gh[i]=buf[i]; */
  /*   /\* channel number *\/ */
  /*   *ch=(((WIN_ch)gh[0])<<8)+(WIN_ch)gh[1]; */
  /*   /\* sampling rate *\/ */
  /*   sr=(((WIN_sr)(gh[2]&0x0f))<<8)+(WIN_sr)gh[3]; */
  /*   /\* sample size *\/ */
  /*   if((gh[2]>>4)&0x7) gsize=((gh[2]>>4)&0x7)*(sr-1)+8; */
  /*   else gsize=(sr>>1)+8; */

  gsize = win_chheader_info(buf, ch, &sr, &i);

  return(gsize);
}

/*-
 * channel header information for MON data
 *  return group size (byte)
 * input  : *ptr
 * output : ch  : channel number
 -*/
uint32_w
get_sysch_mon(const uint8_w *ptr, WIN_ch *ch)
{
  uint32_w  gs;
  int   i, j;

  /* channel number */
  *ch = (((WIN_ch)ptr[0]) << 8) + (WIN_ch)ptr[1];

  /* groupe size */
  gs = 2;
  for (i = 0; i < SR_MON; i++) {
    j = (ptr[gs] & 0x03) * 2;
    gs += j + 1;
    if (j == 0)
      gs++;
  }

  return(gs);
}

/*** make mon data ***/
void
get_mon(WIN_sr gm_sr, int32_w *gm_raw, int32_w (*gm_mon)[2])
  {
  int gm_i,gm_j,gm_subr;

/*   switch(gm_sr) */
/*     { */
/*     case 100: */
/*       gm_subr=100/SR_MON; */
/*       break; */
/*     case 20: */
/*       gm_subr=20/SR_MON; */
/*       break; */
/*     case 120: */
/*       gm_subr=120/SR_MON; */
/*       break; */
/*     default: */
/*       gm_subr=gm_sr/SR_MON; */
/*       break; */
/*     } */
  gm_subr = gm_sr / SR_MON;

  for (gm_i = 0; gm_i < SR_MON; gm_i++) {
    gm_mon[gm_i][0] = gm_mon[gm_i][1] = (*gm_raw);
    for (gm_j = 0; gm_j < gm_subr; gm_j++) {
      if (*gm_raw < gm_mon[gm_i][0])
	gm_mon[gm_i][0] = (*gm_raw);
      else if (*gm_raw > gm_mon[gm_i][1])
	gm_mon[gm_i][1] = (*gm_raw);
      gm_raw++;
    }
  }
}

uint8_w
*compress_mon(int32_w *peaks, uint8_w *ptr)
  {

  /* data compression */
  if (((peaks[0] & 0xffffffc0) == 0xffffffc0 || (peaks[0] & 0xffffffc0) == 0) &&
    ((peaks[1] & 0xffffffc0) == 0xffffffc0 || (peaks[1] & 0xffffffc0) == 0)) {
    *ptr++ = ((peaks[0] & 0x70) << 1) | ((peaks[1] & 0x70) >> 2);
    *ptr++ = ((peaks[0] & 0xf) << 4) | (peaks[1] & 0xf);
  } else if (((peaks[0] & 0xfffffc00) == 0xfffffc00 || (peaks[0] & 0xfffffc00) == 0) &&
    ((peaks[1] & 0xfffffc00) == 0xfffffc00 || (peaks[1] & 0xfffffc00) == 0)) {
    *ptr++ = ((peaks[0] & 0x700) >> 3) | ((peaks[1] & 0x700) >> 6) | 1;
    *ptr++ = peaks[0];
    *ptr++ = peaks[1];
  } else if (((peaks[0] & 0xfffc0000) == 0xfffc0000 || (peaks[0] & 0xfffc0000) == 0) &&
    ((peaks[1] & 0xfffc0000) == 0xfffc0000 || (peaks[1] & 0xfffc0000) == 0)) {
    *ptr++ = ((peaks[0] & 0x70000) >> 11) | ((peaks[1] & 0x70000) >> 14) | 2;
    *ptr++ = peaks[0];
    *ptr++ = peaks[0] >> 8;
    *ptr++ = peaks[1];
    *ptr++ = peaks[1] >> 8;
  } else if (((peaks[0] & 0xfc000000) == 0xfc000000 || (peaks[0] & 0xfc000000) == 0) &&
    ((peaks[1] & 0xfc000000) == 0xfc000000 || (peaks[1] & 0xfc000000) == 0)) {
    *ptr++ = ((peaks[0] & 0x7000000) >> 19) | ((peaks[1] & 0x7000000) >> 22) | 3;
    *ptr++ = peaks[0];
    *ptr++ = peaks[0] >> 8;
    *ptr++ = peaks[0] >> 16;
    *ptr++ = peaks[1];
    *ptr++ = peaks[1] >> 8;
    *ptr++ = peaks[1] >> 16;
  } else {
    *ptr++ = 0;
    *ptr++ = 0;
  }
  return (ptr);
}

void
make_mon(uint8_w *ptr, uint8_w *ptw, int bits_shift) /* for one minute(second?) */
{
  uint8_w        *ptr_lim, *ptw_start;
  int		  i;
  uint32_w	  re;
  WIN_ch	  ch;
  WIN_sr	  sr;
  /* unsigned long uni; */
  size_t	  uni;		/* 64bit ok */
  uint32_w	  uni4;
  int32_w	  buf_raw[HEADER_4B], buf_mon[SR_MON][2];

  /* make mon data */
  ptr_lim = ptr + mkuint4(ptr);
  ptw_start = ptw;
  ptr += 4;
  ptw += 4;			/* size (4) */
  for (i = 0; i < 6; i++)
    *ptw++ = (*ptr++);		/* YMDhms (6) */
  do {				/* loop for ch's */
    if ((re = win2fix(ptr, buf_raw, &ch, &sr)) == 0)
      break;
    if (sr >= HEADER_4B)
      break;
    ptr += re;
    if(bits_shift) for(i=0;i<sr;i++) buf_raw[i]=buf_raw[i]>>bits_shift;
    get_mon(sr, buf_raw, buf_mon);	/* get mon data from raw */
    *ptw++ = ch >> 8;
    *ptw++ = ch;
    for (i = 0; i < SR_MON; i++)
      ptw = compress_mon(buf_mon[i], ptw);
  } while (ptr < ptr_lim);
  uni = ptw - ptw_start;
  uni4 = (uint32_w) uni;
  if (uni4 != uni) {
    fprintf(stderr, "32bit limit exceeded\n");
    exit(1);
  }
  ptw_start[0] = uni4 >> 24;	/* size (H) */
  ptw_start[1] = uni4 >> 16;
  ptw_start[2] = uni4 >> 8;
  ptw_start[3] = uni4;		/* size (L) */
}

void
t_bcd(time_t t, uint8_w *ptr)
{
  struct tm      *nt;

  nt = localtime(&t);
  ptr[0] = d2b[nt->tm_year % 100];
  ptr[1] = d2b[nt->tm_mon + 1];
  ptr[2] = d2b[nt->tm_mday];
  ptr[3] = d2b[nt->tm_hour];
  ptr[4] = d2b[nt->tm_min];
  ptr[5] = d2b[nt->tm_sec];
}

time_t
bcd_t(uint8_w *ptr)
{				/* 64bit ok */
  int		  tm[6];
  time_t	  ts;
  struct tm	  mt;

  if (!bcd_dec(tm, ptr))
    return (0);			/* out of range */
  memset(&mt, 0, sizeof(mt));
  if ((mt.tm_year = tm[0]) < WIN_YEAR)
    mt.tm_year += 100;
  mt.tm_mon = tm[1] - 1;
  mt.tm_mday = tm[2];
  mt.tm_hour = tm[3];
  mt.tm_min = tm[4];
  mt.tm_sec = tm[5];
  mt.tm_isdst = 0;
#if defined(__SVR4) || defined(HAVE_STRUCT_TM_TM_GMTOFF) || defined(__CYGWIN__)
  ts = mktime2(&mt);
#else
  ts = mktime(&mt);
#endif

  if (ts == (time_t)-1) {
    (void)fputs("mktime or mktime2 error.\n", stderr);
    exit(1);
  } else
    return (ts);
}

/*** This function was replaced by bcd_t() [2010/1/12] ***/
/* time_t */
/* bcd2time(uint8_w *bcd) */
/* { */
/*   int  t[6]; */
/*   struct tm  time_str; */
/*   time_t     time; */

/*   memset(&time_str, 0, sizeof(time_str)); */
/*   bcd_dec(t,bcd); */
/*   if (t[0] >= WIN_YEAR) */
/*     time_str.tm_year = t[0]; */
/*   else */
/*     time_str.tm_year = 100 + t[0]; /\* 2000+t[0]-1900 *\/ */
/*   time_str.tm_mon = t[1] - 1; */
/*   time_str.tm_mday = t[2]; */
/*   time_str.tm_hour = t[3]; */
/*   time_str.tm_min = t[4]; */
/*   time_str.tm_sec = t[5]; */
/*   time_str.tm_isdst = 0; */

/*   if ((time = mktime(&time_str)) == (time_t)-1) { */
/*     (void)fputs("mktime error.\n", stderr); */
/*     exit(1); */
/*   } */
/*   return (time); */
/* } */

/*** This function was replaced by t_bcd() [2010/1/12] ***/
/* void */
/* time2bcd(time_t time, uint8_w *bcd) */
/* { */
/*   int          t[6]; */
/*   struct tm    time_str; */

/*   time_str = *localtime(&time); */
/*   if (time_str.tm_year >= 100) */
/*     t[0] = time_str.tm_year - 100; */
/*   else */
/*     t[0] = time_str.tm_year; */
/*   t[1] = time_str.tm_mon + 1; */
/*   t[2] = time_str.tm_mday; */
/*   t[3] = time_str.tm_hour; */
/*   t[4] = time_str.tm_min; */
/*   t[5] = time_str.tm_sec; */
/*   dec_bcd(bcd,t); */
/* } */

int
time_cmpq(const void *_a, const void *_b)   /* for qsort() */
{
  time_t  a, b;

  a = *(time_t *)_a;
  b = *(time_t *)_b;

  if (a < b)
    return (-1);
  else if (a > b)
    return (1);
  else
    return (0);
}

int
ch_cmpq(const void *_a, const void *_b)   /* for qsort() */
{
  WIN_ch  a, b;

  a = *(WIN_ch *)_a;
  b = *(WIN_ch *)_b;

  if (a < b)
    return (-1);
  else if (a > b)
    return (1);
  else
    return (0);
}

void
rmemo5(char f[], int c[])
{
  FILE           *fp;
  int		  bad_flag;

  do {
    while ((fp = fopen(f, "r")) == NULL) {
      (void)fprintf(stderr, "file '%s' not found\007\n", f);
      (void)sleep(1);
    }
    if (fscanf(fp, "%02d%02d%02d%02d.%02d",
	       &c[0], &c[1], &c[2], &c[3], &c[4]) >= 5)
      bad_flag = 0;		/* OK */
    else {
      bad_flag = 1;
      (void)fprintf(stderr, "'%s' illegal. Waiting ...\n", f);
      (void)sleep(1);
    }
    (void)fclose(fp);
  } while (bad_flag);
}

void
rmemo6(char f[], int c[])
{
  FILE           *fp;
  int		  bad_flag;

  do {
    while ((fp = fopen(f, "r")) == NULL) {
      (void)fprintf(stderr, "file '%s' not found\007\n", f);
      (void)sleep(1);
    }
    if (fscanf(fp, "%02d%02d%02d.%02d%02d%02d",
	       &c[0], &c[1], &c[2], &c[3], &c[4], &c[5]) >= 6)
      bad_flag = 0;		/* OK */
    else {
      bad_flag = 1;
      (void)fprintf(stderr, "'%s' illegal. Waiting ...\n", f);
      (void)sleep(1);
    }
    (void)fclose(fp);
  } while (bad_flag);
}

int
wmemo5(char name[], int tm[])
{
  FILE           *fp;

  if ((fp = fopen(name, "w+")) == NULL) {
    (void)fprintf(stderr, "Done file cannot open: %s\n", name);
    return (-2);
  }
  (void)fprintf(fp, "%02d%02d%02d%02d.%02d\n",
		tm[0], tm[1], tm[2], tm[3], tm[4]);
  (void)fclose(fp);

  return (0);
}

/*** make matrix: m[nrow][ncol] ***/
int
**i_matrix(int nrow, int ncol)
{
  int           **m;
  int		  i       , j;

  if (NULL == (m = MALLOC(int *, nrow))) {
    (void)fprintf(stderr, "malloc error\n");
    exit(1);
  }
  if (NULL == (m[0] = MALLOC(int, nrow * ncol))) {
    (void)fprintf(stderr, "malloc error\n");
    exit(1);
  }
  for (i = 1; i < nrow; ++i)
    m[i] = m[i - 1] + ncol;

  /* initialize */
  for (i = 0; i < nrow; ++i)
    for (j = 0; j < ncol; ++j)
      m[i][j] = 0;

  return (m);
}

WIN_bs
get_merge_data(uint8_w *mergebuf,
	       uint8_w *mainbuf, WIN_bs *main_num,
	       uint8_w *subbuf, WIN_bs *sub_num)
{
  WIN_ch	  main_chnum, i;
  static WIN_ch	  main_ch[WIN_CHMAX], sub_ch;
  uint8_w        *ptr, *ptr_lim, *ptw;
  WIN_bs	  gsize , size;

  main_chnum = get_sysch_list(mainbuf, *main_num, main_ch);
  ptr_lim = subbuf + (*sub_num);
  ptr = subbuf + WIN_TIME_LEN;	/* skip time stamp */
  ptw = mergebuf;
  size = 0;
  do {
    gsize = get_sysch(ptr, &sub_ch);
    for (i = 0; i < main_chnum; ++i)
      if (main_ch[i] == sub_ch)
	break;
    if (i == main_chnum) {
      main_ch[main_chnum++] = sub_ch;
      size += gsize;
      while ((gsize--) > 0)
	*ptw++ = (*ptr++);
    } else
      ptr += gsize;
  } while (ptr < ptr_lim);

  return (size);
}

/* get channel list from buffer */
WIN_ch
get_sysch_list(uint8_w *buf, WIN_bs bufnum, WIN_ch sysch[])
{
  WIN_ch	  num   , chtmp;
  uint8_w        *ptr, *ptr_lim;
  WIN_bs	  gsize;
  int		  i;

  num = 0;
  ptr_lim = buf + bufnum;
  ptr = buf + WIN_TIME_LEN;
  do {
    gsize = get_sysch(ptr, &chtmp);
    for (i = 0; i < num; ++i)
      if (sysch[i] == chtmp)
	break;
    if (i == num)
      sysch[num++] = chtmp;
    ptr += gsize;
  } while (ptr < ptr_lim);

  return (num);
}

/* get channel list from channel file */
WIN_ch
get_chlist_chfile(FILE *fp, WIN_ch sysch[])
{
  WIN_ch chnum;
  char tbuf[BUF_SIZE];
  int  i;

  chnum = 0;
  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')
      continue;			/* skip comment line */
    if (sscanf(tbuf, "%x", &i) < 1)
      continue;			/* skip blank line */
    sysch[chnum++] = (WIN_ch) i;
  }
  return (chnum);
}

WIN_bs
get_select_data(uint8_w *selectbuf,
		WIN_ch chlist[], WIN_ch ch_num,
		uint8_w *buf,  WIN_bs buf_num)
{
  WIN_bs  size,gsize;
  static WIN_ch  buf_ch[WIN_CHMAX],sel_ch;
  WIN_ch  buf_chnum;
  uint8_w  *ptr,*ptw;
  int  i;

  buf_chnum=get_sysch_list(buf,buf_num,buf_ch);
  ptr=buf+WIN_TIME_LEN;  /* skip time stamp */
  ptw=selectbuf;
  size=0;
  do{
    gsize=get_sysch(ptr,&sel_ch);
    for(i=0;i<ch_num;++i)
      if(chlist[i]==sel_ch) break;
    if(i<ch_num){
      size+=gsize;
      while((gsize--)>0) *ptw++=(*ptr++);
    }
    else ptr+=gsize;
  } while(ptr<buf+buf_num);
  
  return(size);
}


int
WIN_time_hani(char fname[], int start[], int end[])
{
  FILE  *fp;
  WIN_bs  a,size;
  uint8_w  *tbuf;
  int  status,init_flag,i;
  int  dtime[WIN_TIME_LEN];

  if((fp=fopen(fname,"r"))==NULL) return(-1);
  status=0;
  init_flag=1;
  while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN){
    size=(WIN_bs)mkuint4((uint8_w *)&a)-WIN_BLOCKSIZE_LEN;
    if((tbuf=MALLOC(uint8_w,size))==NULL){
      status=1;
      break;
    }
    if(fread(tbuf,1,size,fp)!=size){
      status=1;
      FREE(tbuf);
      break;
    }
    if(!bcd_dec(dtime,tbuf)){
      FREE(tbuf);
      continue;
    }
    if(init_flag){
      for(i=0;i<WIN_TIME_LEN;++i) start[i]=end[i]=dtime[i];
      init_flag=0;
    }
    else{
      if(time_cmp(start,dtime,6)>0) 
	for(i=0;i<WIN_TIME_LEN;++i) start[i]=dtime[i];
      if(time_cmp(end,dtime,6)<0)
	for(i=0;i<WIN_TIME_LEN;++i) end[i]=dtime[i];
    }
    FREE(tbuf);
  } /* while(fread(&a,1,WIN_BLOCKSIZE_LEN,fp)==WIN_BLOCKSIZE_LEN) */
  fclose(fp);

  return(status);
}

int
read_channel_file(FILE *fp, struct channel_tbl tbl[], int arrynum)
{
  char  tbuf[BUF_SIZE], format[BUF_SIZE];
  int   i, ich;

  i = 0;
  if (snprintf(format, sizeof(format),
	       "%%x%%d%%d%%%ds%%%ds%%d%%255s%%lf%%255s%%lf%%lf%%lf%%lf%%lf%%lf%%lf%%lf%%lf",
	       WIN_STANAME_LEN - 1, WIN_STACOMP_LEN - 1) >= sizeof(format)) {
    (void)fprintf(stderr, "buffer overtun!\n");
    exit(1);
  }
  /* printf("%s\n", format); */

  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')   /* skip comment line */
      continue;
    if (sscanf(tbuf, format,
	       &ich, &tbl[i].flag, &tbl[i].delay, tbl[i].name, tbl[i].comp,
	       &tbl[i].scale, tbl[i].bit, &tbl[i].sens, tbl[i].unit,
	       &tbl[i].t0, &tbl[i].h, &tbl[i].gain, &tbl[i].adc, &tbl[i].lat,
	       &tbl[i].lng, &tbl[i].higt, &tbl[i].stcp, &tbl[i].stcs) < 1)
      continue;   /* skip blank line */
    tbl[i++].sysch = (WIN_ch)ich;
    if (i == arrynum)
      break;
  }

  return (i);
}

void
str2double(char *t, int n, int m, double *d)
{
  char  *tb;

  /* May be need length of 't'.*/
  /* if (strlen(t) < n + m) {....} */
  if ((tb = MALLOC(char, m + 1)) == NULL) {
    (void)fprintf(stderr, "str2double : malloc error\n");
    exit(1);
  }
  strncpy(tb, t + n, m);
  tb[m] = '\0';
  if (tb[0] == '*')
    *d = 100.0;
  else
    *d = atof(tb);
  FREE(tb);
}

time_t
shift_sec(uint8_w *tm_bcd, int sec)
{
  struct tm  *nt, mt;
  time_t     ltime;

  memset(&mt, 0, sizeof(mt));
  if ((mt.tm_year = b2d[tm_bcd[0]]) < WIN_YEAR)
    mt.tm_year += 100;
  mt.tm_mon = b2d[tm_bcd[1]] - 1;
  mt.tm_mday = b2d[tm_bcd[2]];
  mt.tm_hour = b2d[tm_bcd[3]];
  mt.tm_min = b2d[tm_bcd[4]];
  mt.tm_sec = b2d[tm_bcd[5]];
  mt.tm_isdst = 0;
  ltime=mktime(&mt);
  if (ltime == -1) {
    fprintf(stderr,"mktime error!\n");
    exit(1);
  }

  if (sec)
    ltime += sec;
  else
    return (ltime);
    
  nt = localtime(&ltime);
  tm_bcd[0] = d2b[nt->tm_year % 100];
  tm_bcd[1] = d2b[nt->tm_mon + 1];
  tm_bcd[2] = d2b[nt->tm_mday];
  tm_bcd[3] = d2b[nt->tm_hour];
  tm_bcd[4] = d2b[nt->tm_min];
  tm_bcd[5] = d2b[nt->tm_sec];
  return (ltime);
}

int
read_param_line(FILE *f_param, char textbuf[], int bufsize)
{

  do {
    if (fgets(textbuf, bufsize, f_param) == NULL)
      return (1);
  } while(textbuf[0] == '#');  /* skip comment line */

  return (0);
}


/* check dir exists or not. If doesn't, make it.
 * return : 1: make dir, 0: dir already exists, -1: error */
int
dir_check(char *path)

{
  struct stat sb;

  if (stat(path, &sb) < 0) {
    if (errno == ENOENT) {  /* if no such dir, make dir */
      if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
	return (-1);
      else
	return (1);
    } else
      return (-1);
  }
  else if (!S_ISDIR(sb.st_mode))
    return (-1);  /* path exists, but not directory */  

  return (0);
}

time_t
check_ts(uint8_w *ptr, time_t pre, time_t post)
{
  int  tm[6];
  time_t  ts, rt, diff;
  struct tm  mt;

  if (!bcd_dec(tm, ptr))
    return (0);			/* out of range */
  /* memset((char *)&mt,0,sizeof(mt)); */
  memset(&mt, 0, sizeof(mt));
  if ((mt.tm_year = tm[0]) < WIN_YEAR)
    mt.tm_year += 100;
  mt.tm_mon = tm[1] - 1;
  mt.tm_mday = tm[2];
  mt.tm_hour = tm[3];
  mt.tm_min = tm[4];
  mt.tm_sec = tm[5];
  mt.tm_isdst = 0;
  ts = mktime(&mt);
  /* compare time with real time */
  time(&rt);
  diff = ts - rt;
  if ((pre == 0 || pre < diff) && (post == 0 || diff < post))
    return (ts);
#if DEBUG1
  printf("diff %ld s out of range (%lds - %lds)\n", diff, pre, post);
#endif
  return (0);
}

size_t
FinalB_read(struct FinalB *d, FILE *fp)
{
  size_t  b;
  int  i;

  b = 0;
  b += fread(d->time, sizeof(int8_w), 8, fp) * sizeof(int8_w);
  b += fread(&d->alat, sizeof(float), 1, fp) * sizeof(float);
  b += fread(&d->along, sizeof(float), 1, fp) * sizeof(float);
  b += fread(&d->dep, sizeof(float), 1, fp) * sizeof(float);
  b += fread(d->diag, sizeof(char), 4, fp) * sizeof(char);
  b += fread(d->owner, sizeof(char),4, fp) * sizeof(char);

  /* endian check */
  i = 1;
  if (*(char *)&i) {
    SWAPF(d->alat);
    SWAPF(d->along);
    SWAPF(d->dep);
  }

  return (b);
}

size_t
FinalB_write(struct FinalB d, FILE *fp)
{
  size_t  b;
  int  i;

  /* endian check */
  i = 1;
  if (*(char *)&i) {
    SWAPF(d.alat);
    SWAPF(d.along);
    SWAPF(d.dep);
  }

  b = 0;
  b += fwrite(d.time, sizeof(int8_w), 8, fp) * sizeof(int8_w);
  b += fwrite(&d.alat, sizeof(float), 1, fp) * sizeof(float);
  b += fwrite(&d.along, sizeof(float), 1, fp) * sizeof(float);
  b += fwrite(&d.dep, sizeof(float), 1, fp) * sizeof(float);
  b += fwrite(d.diag, sizeof(char), 4, fp) * sizeof(char);
  b += fwrite(d.owner, sizeof(char), 4, fp) * sizeof(char);

  return (b);
}

/*- splite host:port
 *
 * hostname(:port)
 * IPv4(:port)
 * [IPv6](:port)
 * IPv6
 *
 * Input : buf[]
 * Output : buf[], **host, **port
 */
int
split_host_port(char buf[], char **host, char **port)
{
  char  *ptr;

  if (buf[0] == '[') {   /* IPv6 address (and port) */
    *host = buf + 1;
    if ((ptr = strchr(buf, ']')) == NULL) {
      *host = *port = NULL;
      return (1);  /* Invalid entry */
    }
    ptr[0] = '\0';
    *port = strrchr(ptr + 1, ':');
    if (*port != NULL)
      (*port)++;
    return (0);
  }

  *host = buf;
  *port = strrchr(buf, ':');
  ptr = strchr(buf, ':');
  if (*port != NULL) {
    if (*port != ptr)  /* IPv6 address */
      *port = NULL;
    else {
      *port[0] = '\0';
      (*port)++;
    }
  }

  return (0);
}

/*-
 * check (a - b) is out of range or not.
 *  return  0 : OK
 *          1 : out of range
 -*/
int
check_4byte_diff(int32_w a, int32_w b)
{
  int32_w  diff;
  
  if (a < -1) {
    diff = a - WIN_AMP_MIN;
    if (b > diff)
      return (1);
    else
      return (0);
  } else if (a == -1)     /* always OK */
    return (0);
  else {
    diff = a - WIN_AMP_MAX;
    if (b < diff)
      return (1);
    else
      return (0);
  }
}

/* calculate CRC16 */
uint16_w
crc16(uint16_w crc,uint8_w *ptr,int len)
{
#define CRC16POLY 0xa001
  int i,j;
  crc=~crc;
  for(i=0;i<len;i++) {
    crc^=ptr[i];
    for(j=0;j<8;j++) {
      if(crc&1) crc=(crc>>1)^CRC16POLY;
      else crc>>=1;
    }
  }
  return ~crc;
}

