/* $Id: winlib.c,v 1.1.2.4.2.12 2009/12/26 00:56:59 uehira Exp $ */

/*-
 * winlib.c  (Uehira Kenji)
 *  win system library
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

#include "winlib.h"

/**********************************************************
 * Local function
 ********************************************************** */

static time_t mktime2(struct tm *);

static time_t
mktime2(struct tm *mt) /* high-speed version of mktime() */
{
  static struct tm *m;
  time_t t;
  register int i,j,ye;
  static int dm[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static int dy[] = {
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 1970-81 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 1982-93 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 1994-2005 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 2006-17 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365, /* 2018-2029 */
    365, 365, 366, 365, 365, 365, 366, 365, 365, 365, 366, 365};/* 2030-2041 */
#if defined(__SVR4)
  extern time_t timezone;
#endif

  if (m == NULL)
    m = localtime(&t);
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
#if defined(HAVE_STRUCT_TM_GMTOFF)
  return (j * 86400 + mt->tm_hour * 3600 + mt->tm_min * 60 + mt->tm_sec - m->tm_gmtoff);
#endif
#if defined(__CYGWIN__)
  tzset();
  return (j * 86400 + mt->tm_hour * 3600 + mt->tm_min * 60 + mt->tm_sec + _timezone);
#endif
}
/******- End of Local function -******/

void
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

int
time_cmp(int *t1, int *t2, int i)
{
  int		  cntr;

  cntr = 0;
  if (t1[cntr] < 70 && t2[cntr] > 70)
    return (1);
  if (t1[cntr] > 70 && t2[cntr] < 70)
    return (-1);
  for (; cntr < i; cntr++) {
    if (t1[cntr] > t2[cntr])
      return (1);
    if (t1[cntr] < t2[cntr])
      return (-1);
  }
  return (0);
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
read_onesec_win(FILE *fp, uint8_w **rbuf)
{
  uint8_w        sz[WIN_BSLEN];
  WIN_bs         size;
  static size_t  sizesave;

  /* printf("%d\n", sizesave); */
  if (fread(sz, 1, WIN_BSLEN, fp) != WIN_BSLEN)
    return (0);
  size = mkuint4(sz);

  if (*rbuf == NULL)
    sizesave = 0;
  if (size > sizesave) {
    sizesave = (size << 1);
    *rbuf = (uint8_w *)realloc(*rbuf, sizesave);
    if (*rbuf == NULL) {
      (void)fprintf(stderr, "%s\n", strerror(errno));
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

/* init shared memory */
void
Shm_init(struct Shm *sh, size_t size)
{

  sh->p = 0; 
  sh->pl = (size - sizeof(*sh)) / 10 * 9;
  sh->c = 0;
  sh->r = (-1);
}

/* print version */
void
WIN_version(void)
{

#if defined __LP64__
  (void)printf("%s package Version %s [64-bit]\n", PACKAGE, VERSION);
#else
  (void)printf("%s package Version %s [32-bit]\n", PACKAGE, VERSION);
#endif
}

/*-
 * channel header information (High sampling rate version)
 *  return group size (byte)
 * input  : *ptr
 * output : ch  : channel number
 *          sr  : sampling rate
 *          ss  : sample size (0-->0.5byte)
 -*/
uint32_w
win_chheader_info(const uint8_w *ptr, WIN_ch *ch, WIN_sr *sr, int *ss)
{
  WIN_bs    gsize;

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
  if (*ss)
    gsize = *ss * (*sr - 1) + 8;
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

/* same as win_chheader_info(), but retuen only channel number info. */
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
make_mon(uint8_w *ptr, uint8_w *ptw) /* for one minute(second?) */
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
  if ((mt.tm_year = tm[0]) < 50)
    mt.tm_year += 100;
  mt.tm_mon = tm[1] - 1;
  mt.tm_mday = tm[2];
  mt.tm_hour = tm[3];
  mt.tm_min = tm[4];
  mt.tm_sec = tm[5];
  mt.tm_isdst = 0;
#if defined(__SVR4) || defined(HAVE_STRUCT_TM_GMTOFF) || defined(__CYGWIN__)
  ts = mktime2(&mt);
#else
  if ((ts = mktime(&mt)) == (time_t)-1) {
    (void)fputs("mktime error.\n", stderr);
    exit(1);
  }
#endif

  return (ts);
}

/*** This function will be replaced by bcd_t() */
time_t
bcd2time(uint8_w *bcd)
{
  int  t[6];
  struct tm  time_str;
  time_t     time;

  memset(&time_str, 0, sizeof(time_str));
  bcd_dec(t,bcd);
  if (t[0] >= 70)
    time_str.tm_year = t[0];
  else
    time_str.tm_year = 100 + t[0]; /* 2000+t[0]-1900 */
  time_str.tm_mon = t[1] - 1;
  time_str.tm_mday = t[2];
  time_str.tm_hour = t[3];
  time_str.tm_min = t[4];
  time_str.tm_sec = t[5];
  time_str.tm_isdst = 0;

  if ((time = mktime(&time_str)) == (time_t)-1) {
    (void)fputs("mktime error.\n", stderr);
    exit(1);
  }
  return (time);
}

/*** This function will be replaced by t_bcd */
void
time2bcd(time_t time, uint8_w *bcd)
{
  int          t[6];
  struct tm    time_str;

  time_str = *localtime(&time);
  if (time_str.tm_year >= 100)
    t[0] = time_str.tm_year - 100;
  else
    t[0] = time_str.tm_year;
  t[1] = time_str.tm_mon + 1;
  t[2] = time_str.tm_mday;
  t[3] = time_str.tm_hour;
  t[4] = time_str.tm_min;
  t[5] = time_str.tm_sec;
  dec_bcd(bcd,t);
}

int
time_cmpq(const void *_a, const void *_b)   /* for qsort() */
{
  unsigned long  a, b;

  a = *(unsigned long *)_a;
  b = *(unsigned long *)_b;

  if (a < b)
    return (-1);
  else if (a > b)
    return (1);
  else
    return (0);
}

