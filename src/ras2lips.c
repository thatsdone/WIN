/* $Id: ras2lips.c,v 1.3 2002/01/13 06:57:51 uehira Exp $ */
/********************************************************/
/*  ras2lips.c   97.10.31-97.11.27             urabe    */
/*               98.3.4      LITTLE ENDIAN    uehira    */
/*               99.4.19     byte-order-free            */
/*               2000.4.17   wabort                     */
/********************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/file.h>
#include <signal.h>
#include <syslog.h>

#include "subst_func.h"

#define A4_FRAME_XB     392       /* framebuffer's width (byte) */
#define A4_FRAME_YP     4516      /* framebuffer's height (pixel) */

#define SWAPL(a) a=(((a)<<24)|((a)<<8)&0xff0000|((a)>>8)&0xff00|((a)>>24)&0xff)

read_header(fp,height,width)
  FILE  *fp;
  int *height,*width;    /* width in bytes */
  {
  int i,length,type,maplength;
  static int buf;

/* header */
  if(fread(&buf,4,1,fp)<1) return(1);  /* magic */
  i=1;
  if(*(char *)&i) SWAPL(buf);
  if(buf!=0x59a66a95) return(0);
  if(fread(&buf,4,1,fp)<1) return(1);  /* width */
  if(*(char *)&i) SWAPL(buf);
  *width=buf;
  if(fread(&buf,4,1,fp)<1) return(1);  /* height */
  if(*(char *)&i) SWAPL(buf);
  *height=buf;
  if(fread(&buf,4,1,fp)<1) return(1);  /* depth */
  if(*(char *)&i) SWAPL(buf);
  if(buf!=1) return(1);
  if(fread(&buf,4,1,fp)<1) return(1);  /* length */
  if(*(char *)&i) SWAPL(buf);
  length=buf;
  if(fread(&buf,4,1,fp)<1) return(1);  /* type */
  if(*(char *)&i) SWAPL(buf);
  type=buf;
  if(type>1) return(1);    /* only RT_OLD and RT_STANDARD */
  if(fread(&buf,4,1,fp)<1) return(1);  /* maptype */
  if(fread(&buf,4,1,fp)<1) return(1);  /* maplength */
  if(*(char *)&i) SWAPL(buf);
  maplength=buf;
  for(i=0;i<maplength;i++) if(fgetc(fp)==EOF) return(1);  /* color map */
/* size */
  *width=((*width-1)/16+1)*16/8;
  return(0);
  }

compress(buf,size)
  char *buf;
  int size;
  {
  int i;
  char *ptr,*ptw,*ptr_lim,c;
  ptw=ptr=buf;
  ptr_lim=buf+size;
  while(1)
    {
    *ptw++=c=(*ptr++);
    if(ptr==ptr_lim) return ptw-buf;
    if(c==(*ptr++))
      {
      *ptw++=c;
      if(ptr==ptr_lim) return ptw-buf;
      for(i=0;i<255;i++) if(c!=(*ptr++) || ptr==ptr_lim) break;
      *ptw++=i;
      if(i==255) ptr++;
      }
    ptr--;
    }
  }

#define DPI 410
one_page(fp,inv)
  FILE *fp;
  int inv;
  {
  int i,j,c1,c2,width,height,size;
  char *buf;
  if(read_header(fp,&height,&width)) fprintf(stderr,"header error.\n");
  else
    {
    buf=(char *)malloc(height*width);
    size=fread(buf,1,height*width,fp);
    if(inv)
      {
      for(i=0,j=size-1;i<j;i++,j--)
        {
        c1=buf[i];
        c2=buf[j];
        buf[j]=(c1<<7)&0x80|(c1<<5)&0x40|(c1<<3)&0x20|(c1<<1)&0x10|
               (c1>>7)&0x01|(c1>>5)&0x02|(c1>>3)&0x04|(c1>>1)&0x08;
        buf[i]=(c2<<7)&0x80|(c2<<5)&0x40|(c2<<3)&0x20|(c2<<1)&0x10|
               (c2>>7)&0x01|(c2>>5)&0x02|(c2>>3)&0x04|(c2>>1)&0x08;
        }
      }
    size=compress(buf,size);
    printf("\033[%d;%d;%d;9;%d.r",size,width,DPI,height);
    fwrite(buf,1,size,stdout);
    free(buf);
    }
  printf("\014");    /* form-feed */
  }

wabort() {exit(9);}

main(argc, argv)
  int argc;
  char *argv[];
  {
  FILE *fp;
  int i;
  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);
  signal(SIGPIPE,(void *)wabort);
  printf("\033%%@");              /* begin text mode */
  printf("\033P41;600;0J\033\\"); /* start job LIPS4, 600dpi */
  printf("\033<");                /* soft reset */
  printf("\033[0p");              /* portrait mode */
  if(argc>2) printf("\033[2;0#x"); /* two sides */
  else printf("\033[0#x"); /* one side */
  if(argc==1) one_page(stdin,0);
  else for(i=1;i<argc;i++) if(fp=fopen(argv[i],"r")) one_page(fp,(i-1)&1);
  printf("\033[0#x"); /* one side */
  printf("\033P0J\033\\");    /* end job */
  }
