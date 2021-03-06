/* $Id: final2finalb.c,v 1.4 2011/06/01 11:09:20 uehira Exp $ */
/******************************************************************/
/*    final2finalb.c              9/21/94 urabe                   */
/*    97.10.3 FreeBSD  99.4.19 byte-order-free                    */
/******************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>

#include "winlib.h"

static const char rcsid[] =
   "$Id: final2finalb.c,v 1.4 2011/06/01 11:09:20 uehira Exp $";

/* prototypes */
int main(int, char *[]);

int
main(int argc, char *argv[])
  {
  int tm[6],tmc[7];
  double se,sec,mag;
  char tbuf[256],owner[20],diag[20];
  /* struct { */
  /*   char time[8]; /\* Y,M,D,h,m,s,s10,mag10 *\/ */
  /*   float alat,along,dep; */
  /*   char diag[4],owner[4]; */
  /*   } d; */
  struct FinalB  d;  /* 28 bytes / event */

  while(fgets(tbuf,sizeof(tbuf),stdin)!=NULL)
    {
    sscanf(tbuf,"%d%d%d%d%d%lf%f%f%f%lf%20s%20s",
      &tm[0],&tm[1],&tm[2],&tm[3],&tm[4],&se,
      &d.alat,&d.along,&d.dep,&mag,owner,diag);
    adj_sec(tm,&se,tmc,&sec);
    d.time[0]=(int8_w)tmc[0];
    d.time[1]=(int8_w)tmc[1];
    d.time[2]=(int8_w)tmc[2];
    d.time[3]=(int8_w)tmc[3];
    d.time[4]=(int8_w)tmc[4];
    d.time[5]=(int8_w)((int)sec);
    d.time[6]=(int8_w)(((int)(sec*10.0))%10);
    d.time[7]=(int8_w)((int)(mag*10.0+0.5));
    *d.diag=(*d.owner)=0;
    if(*diag) strncpy(d.diag,diag,4);
    if(*owner) strncpy(d.owner,owner,4);
    /* i=1;if(*(char *)&i) */
/*       { */
/*       SWAPF(d.alat); */
/*       SWAPF(d.along); */
/*       SWAPF(d.dep);   */
/*       } */
/*     fwrite(&d,sizeof(d),1,stdout); */
    (void)FinalB_write(d, stdout);
    }

  exit(0);
  }
