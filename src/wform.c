/* $Id: wform.c,v 1.1 2001/02/07 09:31:42 urabe Exp $ */
/* wform.c - a program to make a win format file */
/* wform [ch] [sr] */

/* winform.c  4/30/91   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */

#include <stdio.h>
#define SR 4096
winform(inbuf,outbuf,sr,sys_ch)
  long *inbuf;      /* input data array for one sec*/
  unsigned char *outbuf;  /* output data array for one sec */
  int sr;         /* n of data (i.e. sampling rate) */
  unsigned short sys_ch;  /* 16 bit long channel ID number */
  {
  int dmin,dmax,aa,bb,br,i,byte_leng;
  long *ptr;
  unsigned char *buf,*bf;

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
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0))
    byte_leng=0;
  else
  if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0))
    byte_leng=1;
  else
  if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0))
    byte_leng=2;
  else
  if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0))
    byte_leng=3;
  else byte_leng=4;

  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  bf=(unsigned char *)inbuf;
  *buf++=(*bf++);
  *buf++=(*bf++);
  *buf++=(*bf++);
  *buf++=(*bf++);

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
        bf=(unsigned char *)&inbuf[i]+2;
        *buf++=(*bf++);
        *buf++=(*bf++);
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        bf=(unsigned char *)&inbuf[i]+1;
        *buf++=(*bf++);
        *buf++=(*bf++);
        *buf++=(*bf++);
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        bf=(unsigned char *)&inbuf[i];
        *buf++=(*bf++);
        *buf++=(*bf++);
        *buf++=(*bf++);
        *buf++=(*bf++);
        }
      break;
    }
  return (int)(buf-outbuf);
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  static unsigned char inbuf[SR*4],outbuf[4+4*SR],tt[6];
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
  fwrite(&size,4,1,stdout);
  fwrite(tt,6,1,stdout);
  fwrite(outbuf,size,1,stdout);
  }
