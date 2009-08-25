/* $Id: raw2mon.c,v 1.4.4.3.2.4 2009/08/25 04:00:15 uehira Exp $ */
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
#include  <string.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/fcntl.h>
#include  <sys/ioctl.h>
#include  <sys/stat.h>

#include "winlib.h"

#define   MAXSIZE   5000000
#define   SIZE_WBUF 1000000
/* #define   SR_MON    5 */
/* #define   MAX_SR    1024 */

static uint8_w wbuf[SIZE_WBUF],buf[MAXSIZE];

/* prototypes */
int main(int, char *[]);

int
main(int argc, char *argv[])
  {

  while(fread(buf,1,4,stdin)==4)
    {
    fread(buf+4,1,mkuint4(buf)-4,stdin);
    make_mon(buf,wbuf);
    fwrite(wbuf,1,mkuint4(wbuf),stdout);
    }
  exit(0);
  }
