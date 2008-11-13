/* $Id: wform.c,v 1.4.4.2.2.2 2008/11/13 09:36:07 uehira Exp $ */
/* wform.c - a program to make a win format file */
/* wform [ch] [sr] */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "winlib.h"

#define SR 4096

int
main(argc,argv)
  int argc;
  char *argv[];
  {
  static uint8_w  outbuf[4+4*SR],tt[6],cbuf;
  static int32_w  inbuf[SR];
  WIN_sr          sr;
  WIN_ch          ch;
  unsigned int    t[6], chtmp;
  WIN_bs          size;
  int             i;

  if(argc<4)
    {
    fprintf(stderr,"usage: wform [YYMMDDhhmmss] [CH(in hex)] [SR(<4096)]\n");
    exit(1);
    }
  sscanf(argv[1],"%2x%2x%2x%2x%2x%2x",&t[0],&t[1],&t[2],&t[3],&t[4],&t[5]);
  for(i=0;i<6;i++) tt[i]=(uint8_w)t[i];
  sscanf(argv[2],"%x",&chtmp);
  ch = (WIN_ch)chtmp;
  sr=(WIN_sr)atoi(argv[3]);
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
  exit(0);
  }
