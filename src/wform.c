/* $Id: wform.c,v 1.5.2.2 2016/01/05 07:26:59 uehira Exp $ */
/* wform.c - a program to make a win format file */
/* wform [ch] [sr] */

/*-
     2015/12/24  sample size 5 mode supported. (Uehira)
-*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "winlib.h"

/* #define SR 4096 */
#define SR (HEADER_5B)

static const char  rcsid[] =
   "$Id: wform.c,v 1.5.2.2 2016/01/05 07:26:59 uehira Exp $";

static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
  {
  static uint8_w  outbuf[4+4*SR],tt[6],cbuf;
  static int32_w  inbuf[SR];
  WIN_sr          sr;
  WIN_ch          ch;
  unsigned int    t[6], chtmp;
  WIN_bs          size;
  int             i;
  int             c, ss_mode = SSIZE5_MODE, ssf_flag = 0;

  while ((c = getopt(argc, argv, "s:h")) != -1) {
    switch (c) {
    case 's':
      if (strcmp(optarg, "4") == 0)
	ss_mode = 0;
      else if (strcmp(optarg, "5") == 0) {
	ss_mode = 1;
	ssf_flag = 0;
      } else if (strcmp(optarg, "5f") == 0) {
	ss_mode = 1;
	ssf_flag = 1;
      } else {
	fprintf(stderr, "Invalid option: -s\n");
	usage();
      }
      break;
    case 'h':
    default:
      usage();
      /* NOTREACHED */
    }
  }
  argc -= optind -1;
  argv += optind -1;

  if(argc<4)
    usage();
  sscanf(argv[1],"%2x%2x%2x%2x%2x%2x",&t[0],&t[1],&t[2],&t[3],&t[4],&t[5]);
  for(i=0;i<6;i++) tt[i]=(uint8_w)t[i];
  sscanf(argv[2],"%x",&chtmp);
  ch = (WIN_ch)chtmp;
  sr=(WIN_sr)atoi(argv[3]);
  if(sr<=0 || sr>=SR) exit(1);
  if(fread(inbuf,4,sr,stdin)<sr) exit(1);
  /* size=0; */
  /* size=winform(inbuf,outbuf,sr,ch)+10; */
  size=mk_windata(inbuf,outbuf,sr,ch,ss_mode,ssf_flag)+10;
  cbuf=size>>24; fwrite(&cbuf,1,1,stdout);
  cbuf=size>>16; fwrite(&cbuf,1,1,stdout);
  cbuf=size>>8; fwrite(&cbuf,1,1,stdout);
  cbuf=size; fwrite(&cbuf,1,1,stdout);
  fwrite(tt,6,1,stdout);
  fwrite(outbuf,size-10,1,stdout);
  exit(0);
  }
  
static void
usage(void)
{
  
  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr,
	  "usage: wform (-s [4|5|5f]) [YYMMDDhhmmss] [CH(in hex)] [SR(<%d)]\n",
	  SR);
  exit(1);
}
