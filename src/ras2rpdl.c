/* $Id: ras2rpdl.c,v 1.3.4.1 2008/05/17 14:22:00 uehira Exp $ */
/********************************************************/
/*  ras2rpdl.c   97.10.31-97.11.27             urabe    */
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

#include "winlib.h"

#define A4_FRAME_XB     392       /* framebuffer's width (byte) */
#define A4_FRAME_YP     4516      /* framebuffer's height (pixel) */

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

#define SCALE 0.98
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
    printf("\033\022G3,%d,%d,%4.2f,4,,,%d@",width*8,height,SCALE,size);
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
  printf("\033\022!@R00\033 "); /* enter RPDL */
  printf("\0334");   /* end graphic mode */
  printf("\033\022YA04,%d,1 ",1); /* resolution - 1:400/2:240/3:600 */
  printf("\033\022YW,%d,1 ",1); /* graphics unit - 1:400/2:240/3:600 */
  printf("\033\022Y2,%d,1 ",1); /* direction - 1:portrait/2:landscape */
  if(argc>2) printf("\033\022YA06,%d,1 ",2); /* two sides - 1:off/2:on */
  else printf("\033\022YA06,%d,1 ",1); /* two sides - 1:off/2:on */
  if(argc==1) one_page(stdin,0);
  else for(i=1;i<argc;i++) if(fp=fopen(argv[i],"r")) one_page(fp,(i-1)&1);
  printf("\033\022YA06,%d,1 ",1); /* two sides - 1:off/2:on */
  printf("\033\022!@RPS\033 ");   /* enter RPS */
  }
