/*
  program "raw2mon.c"
  6/3/93,6/17/94,8/17/95 urabe
  98.6.30 FreeBSD
*/

#include  <stdio.h>
#include  <string.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/fcntl.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>

#define   MAXSIZE   500000
#define   SIZE_WBUF 50000
#define   SR_MON    5
#define   MAX_SR    1024

/*****************************/
/* 8/21/96 for little-endian (uehira) */
#ifndef         LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234    /* LSB first: i386, vax */
#endif
#ifndef         BIG_ENDIAN
#define BIG_ENDIAN      4321    /* MSB first: 68000, ibm, net */
#endif
#ifndef  BYTE_ORDER
#define  BYTE_ORDER      BIG_ENDIAN
#endif

#define SWAPU  union { long l; float f; short s; char c[4];} swap
#define SWAPL(a) swap.l=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPF(a) swap.f=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPS(a) swap.s=(a); ((char *)&(a))[0]=swap.c[1];\
    ((char *)&(a))[1]=swap.c[0]
/*****************************/

  unsigned char wbuf[SIZE_WBUF],buf[MAXSIZE];
  long buf_raw[MAX_SR],buf_mon[SR_MON][2];

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned char *ptr1;
  unsigned long a;
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPU;
#endif
  ptr1=(unsigned char *)&a;
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1++=(*ptr++);
  *ptr1  =(*ptr);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(a);
#endif
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
  register unsigned char *dp,*pts;
  unsigned int gh;
  short shreg;
  int inreg;
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPU;
#endif

  dp=ptr;
  pts=(unsigned char *)&gh;
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(gh);
#endif
  *sr=s_rate=gh&0xfff;
  if(s_rate>MAX_SR) return 0;
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16);

  /* read group */
  pts=(unsigned char *)&abuf[0];
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts++=(*dp++);
  *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
  SWAPL(abuf[0]);
#endif
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
        pts=(unsigned char *)&shreg;
        *pts++=(*dp++);
        *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
        SWAPS(shreg);
#endif
        abuf[i]=abuf[i-1]+shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        pts=(unsigned char *)&inreg;
        *pts++=(*dp++);
        *pts++=(*dp++);
        *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
        SWAPL(inreg);
#endif
        abuf[i]=abuf[i-1]+(inreg>>8);
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        pts=(unsigned char *)&inreg;
        *pts++=(*dp++);
        *pts++=(*dp++);
        *pts++=(*dp++);
        *pts  =(*dp++);
#if BYTE_ORDER ==  LITTLE_ENDIAN
        SWAPL(inreg);
#endif
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
  int i,j,ch,sr;
  union {
    unsigned long i;
    unsigned short s;
    char c[4];
    } un;

  /* make mon data */
  ptr_lim=ptr+mklong(ptr);
  ptw_start=ptw;
  ptr+=4;
  ptw+=4;               /* size (4) */
  for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */

  do    /* loop for ch's */
    {
    ptr+=win2fix(ptr,buf_raw,&ch,&sr);
    get_mon(sr,buf_raw,buf_mon); /* get mon data from raw */
    *ptw++=ch>>8;
    *ptw++=ch;
    for(i=0;i<SR_MON;i++) ptw=compress_mon(buf_mon[i],ptw);
    } while(ptr<ptr_lim);
  un.i=ptw-ptw_start;
#if BYTE_ORDER ==  BIG_ENDIAN
  ptw_start[0]=un.c[0]; /* size (H) */
  ptw_start[1]=un.c[1];
  ptw_start[2]=un.c[2];
  ptw_start[3]=un.c[3]; /* size (L) */
#endif
#if BYTE_ORDER ==  LITTLE_ENDIAN
  ptw_start[0]=un.c[3]; /* size (H) */
  ptw_start[1]=un.c[2];
  ptw_start[2]=un.c[1];
  ptw_start[3]=un.c[0]; /* size (L) */
#endif
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
