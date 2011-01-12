/* $Id: ras2util.c,v 1.1.2.4 2011/01/12 15:44:30 uehira Exp $ */
/********************************************************/
/*  ras2util.c   97.10.31-97.11.27             urabe    */
/*               98.3.4      LITTLE ENDIAN    uehira    */
/*               99.4.19     byte-order-free            */
/*               2000.4.17   wabort                     */
/*          2010.9.21   join ras2lips.c and ras2rpdl.c. */
/*                      64bit check. (Uehira)           */
/********************************************************/

#if !defined(LIPS) && !defined(RPDL)
#error define LIPS or RPDL.
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "winlib.h"

#define A4_FRAME_XB     392       /* framebuffer's width (byte) */
#define A4_FRAME_YP     4516      /* framebuffer's height (pixel) */

static const char rcsid[] =
  "$Id: ras2util.c,v 1.1.2.4 2011/01/12 15:44:30 uehira Exp $";

/* prototypes */
static int read_header(FILE  *, int32_w *, int32_w *);
static size_t compress(int8_w *, size_t);
static void one_page(FILE *, int);
static void wabort(void);
int main(int, char *[]);

static int
read_header(FILE  *fp, int32_w *height, int32_w *width)
/* height, width :  width in bytes */
  {
  int32_w  i,length,type,maplength;
  static int32_w buf;

/* header */
  if(fread(&buf,4,1,fp)<1) return(1);  /* magic */
  i=1;
  if(*(char *)&i) SWAP32(buf);
  if(buf!=0x59a66a95) return(0);
  if(fread(&buf,4,1,fp)<1) return(1);  /* width */
  if(*(char *)&i) SWAP32(buf);
  *width=buf;
  if(fread(&buf,4,1,fp)<1) return(1);  /* height */
  if(*(char *)&i) SWAP32(buf);
  *height=buf;
  if(fread(&buf,4,1,fp)<1) return(1);  /* depth */
  if(*(char *)&i) SWAP32(buf);
  if(buf!=1) return(1);
  if(fread(&buf,4,1,fp)<1) return(1);  /* length */
  if(*(char *)&i) SWAP32(buf);
  length=buf;
  if(fread(&buf,4,1,fp)<1) return(1);  /* type */
  if(*(char *)&i) SWAP32(buf);
  type=buf;
  if(type>1) return(1);    /* only RT_OLD and RT_STANDARD */
  if(fread(&buf,4,1,fp)<1) return(1);  /* maptype */
  if(fread(&buf,4,1,fp)<1) return(1);  /* maplength */
  if(*(char *)&i) SWAP32(buf);
  maplength=buf;
  for(i=0;i<maplength;i++) if(fgetc(fp)==EOF) return(1);  /* color map */
/* size */
  *width=((*width-1)/16+1)*16/8;
  return(0);
  }

static size_t
compress(int8_w *buf, size_t size)
  {
  int i;
  int8_w *ptr,*ptw,*ptr_lim,c;

  ptw=ptr=buf;
  ptr_lim=buf+size;
  for(;;)
    {
    *ptw++=c=(*ptr++);
    if(ptr==ptr_lim) return (ptw-buf);
    if(c==(*ptr++))
      {
      *ptw++=c;
      if(ptr==ptr_lim) return (ptw-buf);
      for(i=0;i<255;i++) if(c!=(*ptr++) || ptr==ptr_lim) break;
      *ptw++=i;
      if(i==255) ptr++;
      }
    ptr--;
    }
  }

#define DPI 410
#define SCALE 0.98
static void
one_page(FILE *fp, int inv)
  {
  int32_w c1,c2,width,height;
  size_t  i,j;
  size_t  size;
  int8_w *buf;

  if(read_header(fp,&height,&width)) fprintf(stderr,"header error.\n");
  else
    {
    if ((buf=MALLOC(int8_w, height*width)) == NULL)
      fprintf(stderr, "Allocate error\n");
    else
      {
	size=fread(buf,1,height*width,fp);
	if(inv)
	  {
	    for(i=0,j=size-1;i<j;i++,j--)
	      {
		c1=buf[i];
		c2=buf[j];
		buf[j]=((c1<<7)&0x80)|((c1<<5)&0x40)|((c1<<3)&0x20)|((c1<<1)&0x10)|
		  ((c1>>7)&0x01)|((c1>>5)&0x02)|((c1>>3)&0x04)|((c1>>1)&0x08);
		buf[i]=((c2<<7)&0x80)|((c2<<5)&0x40)|((c2<<3)&0x20)|((c2<<1)&0x10)|
		  ((c2>>7)&0x01)|((c2>>5)&0x02)|((c2>>3)&0x04)|((c2>>1)&0x08);
	      }
	  }
	size=compress(buf,size);
#ifdef LIPS
	printf("\033[%zu;%d;%d;9;%d.r",size,width,DPI,height);
#endif
#ifdef RPDL
	printf("\033\022G3,%d,%d,%4.2f,4,,,%zu@",width*8,height,SCALE,size);
#endif
	fwrite(buf,1,size,stdout);
	FREE(buf);
      }
    }
  printf("\014");    /* form-feed */
  }
#undef DPI
#undef SCALE

static void
wabort(void) {exit(9);}

int
main(int argc, char *argv[])
  {
  FILE *fp;
  int i;

  signal(SIGINT,(void *)wabort);
  signal(SIGTERM,(void *)wabort);
  signal(SIGPIPE,(void *)wabort);
#ifdef LIPS
  printf("\033%%@");              /* begin text mode */
  printf("\033P41;600;0J\033\\"); /* start job LIPS4, 600dpi */
  printf("\033<");                /* soft reset */
  printf("\033[0p");              /* portrait mode */
  if(argc>2) printf("\033[2;0#x"); /* two sides */
  else printf("\033[0#x"); /* one side */
#endif
#ifdef RPDL
  printf("\033\022!@R00\033 "); /* enter RPDL */
  printf("\0334");   /* end graphic mode */
  printf("\033\022YA04,%d,1 ",1); /* resolution - 1:400/2:240/3:600 */
  printf("\033\022YW,%d,1 ",1); /* graphics unit - 1:400/2:240/3:600 */
  printf("\033\022Y2,%d,1 ",1); /* direction - 1:portrait/2:landscape */
  if(argc>2) printf("\033\022YA06,%d,1 ",2); /* two sides - 1:off/2:on */
  else printf("\033\022YA06,%d,1 ",1); /* two sides - 1:off/2:on */
#endif
  if(argc==1) one_page(stdin,0);
  else for(i=1;i<argc;i++) if((fp=fopen(argv[i],"r"))!=NULL) one_page(fp,(i-1)&1);
#ifdef LIPS
  printf("\033[0#x"); /* one side */
  printf("\033P0J\033\\");    /* end job */
#endif
#ifdef RPDL
  printf("\033\022YA06,%d,1 ",1); /* two sides - 1:off/2:on */
  printf("\033\022!@RPS\033 ");   /* enter RPS */
#endif

  exit(0);
  }
