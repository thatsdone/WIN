/* $Id: finalb2final.c,v 1.2.2.1 2001/11/02 11:43:36 uehira Exp $ */
/******************************************************************/
/*    finalb2final.c              6/10/94 urabe                   */
/*    97.10.3 FreeBSD    99.4.19 byte-order-free                  */
/******************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>

#include "subst_func.h"

#define SWAPF(a) *(long *)&(a)=(((*(long *)&(a))<<24)|\
  ((*(long *)&(a))<<8)&0xff0000|((*(long *)&(a))>>8)&0xff00|\
  ((*(long *)&(a))>>24)&0xff)

main(argc,argv)
  int argc;
  char *argv[];
  {
  char fname[100];
  int i,j,no,start,year;
  unsigned char b[16];
  struct {
    char time[8]; /* Y,M,D,h,m,s,s10,mag10 */
    float alat,along,dep;
    char diag[4],owner[4];
    } d;      /* 28 bytes / event */

  while(fread(&d,sizeof(d),1,stdin)>0)
    {
    i=1;if(*(char *)&i)
      {
      SWAPF(d.alat);  
      SWAPF(d.along);
      SWAPF(d.dep);
      }
    printf("%02d %02d %02d %02d %02d %02d.%d %.5f %.5f %5.1f %4.1f %.4s %.4s\n",
      d.time[0],d.time[1],d.time[2],d.time[3],d.time[4],
      d.time[5],d.time[6],d.alat,d.along,d.dep,
      ((float)d.time[7])*0.1,d.owner,d.diag);
    }
  }
