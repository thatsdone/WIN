/* $Id: raw2mon.c,v 1.7 2013/08/09 08:50:15 urabe Exp $ */
/*
  program "raw2mon.c"

  6/3/93,6/17/94,8/17/95 urabe
  98.6.30 FreeBSD
  99.4.19 byte-order-free
  2000.4.24 skip ch with >MAX_SR
  2002.4.30 MAXSIZE 500K->1M, SIZE_WBUF 50K->300K
  2007.1.15 MAXSIZE 1M->5M, SIZE_WBUF 300K->1M
  2009.7.30 64bit clean.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>

#include "winlib.h"

#define   MAXSIZE   5000000
#define   SIZE_WBUF 1000000
/* #define   SR_MON    5 */
/* #define   MAX_SR    1024 */

/* prototypes */
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  static uint8_w  wbuf[SIZE_WBUF], buf[MAXSIZE];
  int bits_shift;

  bits_shift=0;
  if(argc>1)
    {
    bits_shift=atoi(argv[1]);
    if(bits_shift<0 || bits_shift>16) {
      (void)fprintf(stderr, "illegal bits_shift=%d\n",bits_shift);
      exit(1);
    }
    else (void)fprintf(stderr, "bits_shift=%d\n",bits_shift);
  }

  while (fread(buf, 1, 4, stdin) == 4) {
    if (mkuint4(buf) > sizeof(buf)) {
      (void)fprintf(stderr, "Buffer overflow. Exit!!\n");
      exit(1);
    }
    if (fread(buf + 4, 1, mkuint4(buf) - 4, stdin) < mkuint4(buf) - 4) {
      (void)fprintf(stderr, "fread error!!\n");
      exit(1);
    }
    make_mon(buf, wbuf, bits_shift);
    if (fwrite(wbuf, 1, mkuint4(wbuf), stdout) < mkuint4(wbuf)) {
      (void)fprintf(stderr, "fwrite error!!\n");
      exit(1);
    }
  }
  exit(0);
}
