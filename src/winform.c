/* $Id: winform.c,v 1.1 2000/04/30 10:05:24 urabe Exp $ */
/* winform.c  4/30/91,99.4.19   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */

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
