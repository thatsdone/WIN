/* $Id: winlib.c,v 1.1.2.4 2008/05/19 04:14:20 uehira Exp $ */

/*-
 * winlib.c  (Uehira Kenji)
 *  win system library
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else  /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif  /* !TIME_WITH_SYS_TIME */

#include "winlib.h"

void
get_time(int rt[])
{
  struct tm  *nt;
  time_t  ltime;

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
 * mklong()
 *  make unsigned long
*/
unsigned long
mklong(unsigned char *ptr)
{
  unsigned long a;

  a = ((ptr[0] << 24) & 0xff000000) + ((ptr[1] << 16) & 0xff0000) +
    ((ptr[2] << 8) & 0xff00) + (ptr[3] & 0xff);

  return (a);
}

/*-
 * mkshort
 *  make unsigned short
 */
unsigned short
mkshort(unsigned char *ptr)
{
  unsigned  short a;      

  a = ((ptr[0] << 8) & 0xff00) + (ptr[1] & 0xff);

  return (a);
}

/*-
 * bcd_dec()
 *  convert BCD to DEC
 */
int
bcd_dec(int dest[], unsigned char *sour)
{
  int i;
  
  i=b2d[sour[0]];
  if(i>=0 && i<=99) dest[0]=i; else return(0);
  i=b2d[sour[1]];
  if(i>=1 && i<=12) dest[1]=i; else return(0);
  i=b2d[sour[2]];
  if(i>=1 && i<=31) dest[2]=i; else return(0);
  i=b2d[sour[3]];
  if(i>=0 && i<=23) dest[3]=i; else return(0);
  i=b2d[sour[4]];
  if(i>=0 && i<=59) dest[4]=i; else return(0);
  i=b2d[sour[5]];
  if(i>=0 && i<=60) dest[5]=i; else return(0);
  return(1);
}

int
dec_bcd(unsigned char *dest, unsigned int *sour)
{
  int  cntr;

  for (cntr = 0; cntr < 6; cntr++) {
    /* dest[cntr] =
       (((sour[cntr] / 10) << 4) & 0xf0) | (sour[cntr] % 10 & 0xf); */
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
  if(tm[4]==60){
    tm[4]=0;
    if(++tm[3]==24){
      tm[3]=0;
      tm[2]++;
      switch(tm[1]){
        case 2:
          if(tm[0]%4==0){
            if(tm[2]==30){
              tm[2]=1;
              tm[1]++;
            }
            break;
          }
          else{
            if(tm[2]==29){
              tm[2]=1;
              tm[1]++;
            }
            break;
          }
        case 4:
        case 6:
        case 9:
        case 11:
          if(tm[2]==31){
            tm[2]=1;
            tm[1]++;
          }
          break;
        default:
          if(tm[2]==32){
            tm[2]=1;
            tm[1]++;
          }
          break;
      }
      if(tm[1]==13){
        tm[1]=1;
        if(++tm[0]==100) tm[0]=0;
      }
    }
  }
  else if(tm[4]==-1){
    tm[4]=59;
    if(--tm[3]==-1){
      tm[3]=23;
      if(--tm[2]==0){
        switch(--tm[1]){
          case 2:
            if(tm[0]%4==0) tm[2]=29;
            else tm[2]=28;
            break;
          case 4:
          case 6:
          case 9:
          case 11:
            tm[2]=30;
            break;
          default:
            tm[2]=31;
            break;
        }
        if(tm[1]==0){
          tm[1]=12;
          if(--tm[0]==-1) tm[0]=99;
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
  if(tm[5]==60){
    tm[5]=0;
    if(++tm[4]==60){
      tm[4]=0;
      if(++tm[3]==24){
        tm[3]=0;
        tm[2]++;
        switch(tm[1]){
	  case 2:
	    if(tm[0]%4==0){  /* leap year */
	      if(tm[2]==30){
                tm[2]=1;
                tm[1]++;
              }
              break;
            }
            else{
              if(tm[2]==29){
                tm[2]=1;
                tm[1]++;
              }
              break;
            }
          case 4:
          case 6:
          case 9:
          case 11:
            if(tm[2]==31){
              tm[2]=1;
              tm[1]++;
            }
            break;
          default:
            if(tm[2]==32){
              tm[2]=1;
              tm[1]++;
            }
            break;
        }
        if(tm[1]==13){
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
        }
      }
    }
  }
  else if(tm[5]==-1){
    tm[5]=59;
    if(--tm[4]==-1){
      tm[4]=59;
      if(--tm[3]==-1){
        tm[3]=23;
        if(--tm[2]==0){
          switch(--tm[1]){
            case 2:
              if(tm[0]%4==0) tm[2]=29;
              else tm[2]=28;
              break;
            case 4:
            case 6:
            case 9:
            case 11:
              tm[2]=30;
              break;
            default:
              tm[2]=31;
              break;
          }
          if(tm[1]==0){
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
          }
        }
      }
    }
  }
}

int
time_cmp(int *t1, int *t2, int i)
{
  int cntr;

  cntr=0;
  if(t1[cntr]<70 && t2[cntr]>70) return(1);
  if(t1[cntr]>70 && t2[cntr]<70) return(-1);
  for(;cntr<i;cntr++){
    if(t1[cntr]>t2[cntr]) return(1);
    if(t1[cntr]<t2[cntr]) return(-1);
  } 
  return(0);
}
 
/* winform.c  4/30/91, 99.4.19   urabe */
/*            2005.3.9 high sampling rate uehira */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */
int
winform(long *inbuf, unsigned char *outbuf, int sr, unsigned short sys_ch)
     /* long *inbuf;      /* input data array for one sec */
     /* unsigned char *outbuf;  /* output data array for one sec */
     /* int sr;         /* n of data (i.e. sampling rate) */
     /* unsigned short sys_ch;  /* 16 bit long channel ID number */
{
  int             dmin, dmax, aa, bb, br, i, byte_leng;
  long           *ptr;
  unsigned char  *buf;

  if (sr >= HEADER_5B)
    return (-1);   /* sampling rate is out of range */

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
      ((dmax & 0xfffffff8) == 0xfffffff8 || (dmax & 0xfffffff8)==0))
    byte_leng = 0;
  else if (((dmin & 0xffffff80) == 0xffffff80 || (dmin & 0xffffff80) == 0) &&
	   ((dmax & 0xffffff80) == 0xffffff80 || (dmax & 0xffffff80)==0))
    byte_leng = 1;
  else if (((dmin & 0xffff8000) == 0xffff8000 || (dmin & 0xffff8000) == 0) &&
	   ((dmax & 0xffff8000) == 0xffff8000 || (dmax & 0xffff8000)== 0))
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
  if (sr < HEADER_4B) {       /* 4 byte header */
    *buf++ = (byte_leng << 4) | (sr >> 8);
    *buf++ = sr & 0xff;
  } else {                    /* 5 byte header */
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

  return ((int)(buf - outbuf));
}

/* returns group size in bytes */
/* High sampling rate version */
int
win2fix(unsigned char *ptr, long *abuf, long *sys_ch, long *sr)
     /*       unsigned char *ptr; /* input */
     /*       long *abuf;         /* output */
     /*       long *sys_ch;       /* sys_ch */
     /*       long *sr;           /* sr */
{
  int            b_size, g_size;
  int            i;
  long           s_rate;
  unsigned char  *dp;
  unsigned long  gh;
  short          shreg;
  int            inreg;

  /* channel number */
  *sys_ch = (long)((((WIN_ch)ptr[0]) << 8) + (WIN_ch)ptr[1]);

  /* sampling rate */
  if ((ptr[2] & 0x80) == 0x0) {  /* channel header = 4 byte */
    *sr = s_rate = (long)((WIN_sr)ptr[3] + (((WIN_sr)(ptr[2] & 0x0f)) << 8));
    dp = ptr + 4;
  } else {                       /* channel header = 5 byte */
    *sr = s_rate = (long)((WIN_sr)ptr[4] + (((WIN_sr)ptr[3]) << 8)
			  + (((WIN_sr)(ptr[2] & 0x0f)) << 16));
    dp = ptr + 5;
  }

  /* size */
  b_size = (ptr[2] >> 4) & 0x7;
  if (b_size)
    g_size = b_size * (s_rate - 1) + 8;
  else
    g_size = (s_rate >> 1) + 8;
  if (ptr[2] & 0x80)
    g_size++;

  /* read group */
  abuf[0] = ((dp[0] << 24) & 0xff000000) + ((dp[1] << 16) & 0xff0000)
    + ((dp[2] << 8) & 0xff00) + (dp[3] & 0xff);
  if (s_rate == 1)
    return (g_size);  /* normal return */

  dp += 4;
  switch (b_size) {
  case 0:
    for (i = 1; i < s_rate; i += 2) {
      abuf[i] = abuf[i - 1] + ((*(char *)dp) >> 4);
      if (i == s_rate - 1)
	break;
      abuf[i + 1] = abuf[i] + (((char)(*(dp++) << 4)) >> 4);
    }
    break;
  case 1:
    for (i = 1; i < s_rate; i++)
      abuf[i] = abuf[i - 1] + (*(char *)(dp++));
    break;
  case 2:
    for (i = 1; i < s_rate; i++) {
      shreg = ((dp[0] << 8) & 0xff00) + (dp[1] & 0xff);
      dp += 2;
      abuf[i] = abuf[i - 1] + shreg;
    }
    break;
  case 3:
    for(i = 1; i < s_rate; i++) {
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
    return (0); /* bad header */
  }

  return (g_size);  /* normal return */
}

int
strncmp2(char *s1, char *s2, int i)
{

  if ((*s1 >= '0' && *s1 <= '5') && (*s2 <= '9' && *s2 >= '6'))
    return (1);
  else if ((*s1 <= '9' && *s1 >= '7') && (*s2 >= '0' && *s2 <= '6'))
    return (-1);
  else
    return (strncmp(s1,s2,i));
}

int
strcmp2(char *s1, char *s2)
{

  if ((*s1 >= '0' && *s1 <= '5') && (*s2 <= '9' && *s2 >= '6'))
    return (1);
  else if ((*s1 <= '9' && *s1 >= '7') && (*s2 >= '0' && *s2 <= '6'))
    return (-1);
  else
    return (strcmp(s1,s2));
}

