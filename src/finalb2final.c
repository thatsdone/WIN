/* $Id: finalb2final.c,v 1.3.2.1 2010/12/28 12:55:41 uehira Exp $ */
/******************************************************************/
/*    finalb2final.c              6/10/94 urabe                   */
/*    97.10.3 FreeBSD    99.4.19 byte-order-free                  */
/******************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>

#include "winlib.h"

static const char rcsid[] =
   "$Id: finalb2final.c,v 1.3.2.1 2010/12/28 12:55:41 uehira Exp $";

/* prototypes */
int main(int, char *[]);

int
main(int argc, char *argv[])
  {
  int i;
  /* struct { */
  /*   char time[8]; /\* Y,M,D,h,m,s,s10,mag10 *\/ */
  /*   float alat,along,dep; */
  /*   char diag[4],owner[4]; */
  /*   } d; */
  struct FinalB  d;      /* 28 bytes / event */

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

  exit(0);
  }
