/* $Id: wform.c,v 1.4 2002/05/15 04:02:00 uehira Exp $ */
/* wform.c - a program to make a win format file */
/* wform [ch] [sr] */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "subst_func.h"

#define SR 4096

/* $Id: wform.c,v 1.4 2002/05/15 04:02:00 uehira Exp $ */
/* winform.c  4/30/91,99.4.19   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */

static
winform(inbuf,outbuf,sr,sys_ch)
  long *inbuf;      /* input data array for one sec*/
  unsigned char *outbuf;  /* output data array for one sec */
  int sr;         /* n of data (i.e. sampling rate) */
  unsigned short sys_ch;  /* 16 bit long channel ID number */
  {
  int dmin,dmax,aa,bb,br,i,byte_leng;
  long *ptr;
  unsigned char *buf;

  /* differentiate and obtain min and max */
  ptr=inbuf;
  bb=(*ptr++);
  dmax=dmin=0;
  for(i=1;i<sr;i++)
    {
    aa=(*ptr);
    *ptr++=br=aa-bb;
    bb=aa;
    if(br>dmax) dmax=br;
    else if(br<dmin) dmin=br;
    }

  /* determine sample size */
  if(((dmin&0xfffffff8)==0xfffffff8 || (dmin&0xfffffff8)==0) &&
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0)) byte_leng=0;
  else if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0)) byte_leng=1;
  else if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0)) byte_leng=2;
  else if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0)) byte_leng=3;
  else byte_leng=4;
  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  *buf++=inbuf[0]>>24;
  *buf++=inbuf[0]>>16;
  *buf++=inbuf[0]>>8;
  *buf++=inbuf[0];
  /* second and after */
  switch(byte_leng)
    {
    case 0:
      for(i=1;i<sr-1;i+=2)
        *buf++=(inbuf[i]<<4)|(inbuf[i+1]&0xf);
      if(i==sr-1) *buf++=(inbuf[i]<<4);
      break;
    case 1:
      for(i=1;i<sr;i++)
        *buf++=inbuf[i];
      break;
    case 2:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>24;
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    }
  return (int)(buf-outbuf);
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  static unsigned char inbuf[SR*4],outbuf[4+4*SR],tt[6],cbuf;
  int sr,ch,size,t[6],i;

  if(argc<4)
    {
    fprintf(stderr,"usage: wform [YYMMDDhhmmss] [CH(in hex)] [SR(<4096)]\n");
    exit(1);
    }
  sscanf(argv[1],"%2x%2x%2x%2x%2x%2x",&t[0],&t[1],&t[2],&t[3],&t[4],&t[5]);
  for(i=0;i<6;i++) tt[i]=t[i];
  sscanf(argv[2],"%x",&ch);
  sr=atoi(argv[3]);
  if(sr<=0 || sr>=SR) exit(1);
  if((size=fread(inbuf,4,sr,stdin))<sr) exit(1);
  size=0;
  size=winform(inbuf,outbuf,sr,ch)+10;
  cbuf=size>>24; fwrite(&cbuf,1,1,stdout);
  cbuf=size>>16; fwrite(&cbuf,1,1,stdout);
  cbuf=size>>8; fwrite(&cbuf,1,1,stdout);
  cbuf=size; fwrite(&cbuf,1,1,stdout);
  fwrite(tt,6,1,stdout);
  fwrite(outbuf,size,1,stdout);
  }
