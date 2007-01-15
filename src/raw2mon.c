/* $Id: raw2mon.c,v 1.5 2007/01/15 23:08:00 urabe Exp $ */
/*
  program "raw2mon.c"
  6/3/93,6/17/94,8/17/95 urabe
  98.6.30 FreeBSD
  99.4.19 byte-order-free
  2000.4.24 skip ch with >MAX_SR
  2002.4.30 MAXSIZE 500K->1M, SIZE_WBUF 50K->300K
  2007.1.15 MAXSIZE 1M->5M, SIZE_WBUF 300K->1M
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <string.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/fcntl.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>

#include "subst_func.h"

#define   MAXSIZE   5000000
#define   SIZE_WBUF 1000000
#define   SR_MON    5
#define   MAX_SR    1024

  unsigned char wbuf[SIZE_WBUF],buf[MAXSIZE];
  long buf_raw[MAX_SR],buf_mon[SR_MON][2];

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

get_mon(gm_sr,gm_raw,gm_mon)
  int gm_sr,*gm_raw,(*gm_mon)[2];
  {
  int gm_i,gm_j,gm_subr;
  switch(gm_sr)
    {
    case 100:
      gm_subr=100/SR_MON;
      break;
    case 20:
      gm_subr=20/SR_MON;
      break;
    case 120:
      gm_subr=120/SR_MON;
      break;
    default:
      gm_subr=gm_sr/SR_MON;
      break;
    }
  for(gm_i=0;gm_i<SR_MON;gm_i++)
    {
    gm_mon[gm_i][0]=gm_mon[gm_i][1]=(*gm_raw);
    for(gm_j=0;gm_j<gm_subr;gm_j++)
      {
      if(*gm_raw<gm_mon[gm_i][0]) gm_mon[gm_i][0]=(*gm_raw);
      else if(*gm_raw>gm_mon[gm_i][1]) gm_mon[gm_i][1]=(*gm_raw);
      gm_raw++;
      }
    }
  }

unsigned char *compress_mon(peaks,ptr)
  int *peaks;
  unsigned char *ptr;
  {
  /* data compression */
  if(((peaks[0]&0xffffffc0)==0xffffffc0 || (peaks[0]&0xffffffc0)==0) &&
    ((peaks[1]&0xffffffc0)==0xffffffc0 ||(peaks[1]&0xffffffc0)==0))
    {
    *ptr++=((peaks[0]&0x70)<<1)|((peaks[1]&0x70)>>2);
    *ptr++=((peaks[0]&0xf)<<4)|(peaks[1]&0xf);
    }
  else
  if(((peaks[0]&0xfffffc00)==0xfffffc00 || (peaks[0]&0xfffffc00)==0) &&
    ((peaks[1]&0xfffffc00)==0xfffffc00 ||(peaks[1]&0xfffffc00)==0))
    {
    *ptr++=((peaks[0]&0x700)>>3)|((peaks[1]&0x700)>>6)|1;
    *ptr++=peaks[0];
    *ptr++=peaks[1];
    }
  else
  if(((peaks[0]&0xfffc0000)==0xfffc0000 ||(peaks[0]&0xfffc0000)==0) &&
    ((peaks[1]&0xfffc0000)==0xfffc0000 ||(peaks[1]&0xfffc0000)==0))
    {
    *ptr++=((peaks[0]&0x70000)>>11)|((peaks[1]&0x70000)>>14)|2;
    *ptr++=peaks[0];
    *ptr++=peaks[0]>>8;
    *ptr++=peaks[1];
    *ptr++=peaks[1]>>8;
    }
  else
  if(((peaks[0]&0xfc000000)==0xfc000000 || (peaks[0]&0xfc000000)==0) &&
    ((peaks[1]&0xfc000000)==0xfc000000 ||(peaks[1]&0xfc000000)==0))
    {
    *ptr++=((peaks[0]&0x7000000)>>19)|((peaks[1]&0x7000000)>>22)|3;
    *ptr++=peaks[0];
    *ptr++=peaks[0]>>8;
    *ptr++=peaks[0]>>16;
    *ptr++=peaks[1];
    *ptr++=peaks[1]>>8;
    *ptr++=peaks[1]>>16;
    }
  else 
    {
    *ptr++=0;
    *ptr++=0;
    }
  return ptr;
  }

win2fix(ptr,abuf,sys_ch,sr) /* returns group size in bytes */
  unsigned char *ptr; /* input */
  register long *abuf;/* output */
  long *sys_ch;       /* sys_ch */
  long *sr;           /* sr */
  {
  int b_size,g_size;
  register int i,s_rate;
  register unsigned char *dp;
  unsigned int gh;
  short shreg;
  int inreg;

  dp=ptr;
  gh=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  *sr=s_rate=gh&0xfff;
  if(s_rate>MAX_SR) return 0;
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16)&0xffff;

  /* read group */
  abuf[0]=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  if(s_rate==1) return g_size;  /* normal return */
  switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        abuf[i]=abuf[i-1]+((*(char *)dp)>>4);
        abuf[i+1]=abuf[i]+(((char)(*(dp++)<<4))>>4);
        }
      break;
    case 1:
      for(i=1;i<s_rate;i++)
        abuf[i]=abuf[i-1]+(*(char *)(dp++));
      break;
    case 2:
      for(i=1;i<s_rate;i++)
        {
        shreg=((dp[0]<<8)&0xff00)+(dp[1]&0xff);
        dp+=2;
        abuf[i]=abuf[i-1]+shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00);
        dp+=3;
        abuf[i]=abuf[i-1]+(inreg>>8);
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
        dp+=4;
        abuf[i]=abuf[i-1]+inreg;
        }
      break;
    default:
      return 0; /* bad header */
    }
  return g_size;  /* normal return */
  }

make_mon(ptr,ptw) /* for one minute */
  unsigned char *ptr,*ptw;
  {
  unsigned char *ptr_lim,*ptw_start,*ptw_save;
  int i,j,ch,sr,re;
  unsigned long uni;

  /* make mon data */
  ptr_lim=ptr+mklong(ptr);
  ptw_start=ptw;
  ptr+=4;
  ptw+=4;               /* size (4) */
  for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */
  do    /* loop for ch's */
    {
    if((re=win2fix(ptr,buf_raw,&ch,&sr))==0) break;
    ptr+=re;
    get_mon(sr,buf_raw,buf_mon); /* get mon data from raw */
    *ptw++=ch>>8;
    *ptw++=ch;
    for(i=0;i<SR_MON;i++) ptw=compress_mon(buf_mon[i],ptw);
    } while(ptr<ptr_lim);
  uni=ptw-ptw_start;
  ptw_start[0]=uni>>24; /* size (H) */
  ptw_start[1]=uni>>16;
  ptw_start[2]=uni>>8;
  ptw_start[3]=uni;     /* size (L) */
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  while(fread(buf,1,4,stdin)==4)
    {
    fread(buf+4,1,mklong(buf)-4,stdin);
    make_mon(buf,wbuf);
    fwrite(wbuf,1,mklong(wbuf),stdout);
    }
  }
