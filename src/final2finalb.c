/* $Id: final2finalb.c,v 1.3.4.2.2.1 2010/06/18 09:50:11 uehira Exp $ */
/******************************************************************/
/*    final2finalb.c              9/21/94 urabe                   */
/*    97.10.3 FreeBSD  99.4.19 byte-order-free                    */
/******************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>

#include "winlib.h"

adj_sec(tm,se,tmc,sec)
  int *tm,*tmc;
  double *se,*sec;
  {
  int i,j;
  for(i=0;i<5;i++) tmc[i]=tm[i];
  if((*sec=(*se))<0.0)
    {
    tmc[5]=0;
    i=(int)(-(*sec));
    if((double)i==(-(*sec))) i--;
    i++;
    for(j=0;j<i;j++) {tmc[5]--;adj_time(tmc);}
    *sec+=(double)(tmc[5]+i);
    }
  else tmc[5]=(int)(*sec);
  tmc[6]=(int)(*sec*1000.0)%1000;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  int i,tm[6],tmc[7];
  double se,sec,mag;
  char tbuf[256],owner[20],diag[20];
  /* struct { */
  /*   char time[8]; /\* Y,M,D,h,m,s,s10,mag10 *\/ */
  /*   float alat,along,dep; */
  /*   char diag[4],owner[4]; */
  /*   } d; */
  struct FinalB  d;  /* 28 bytes / event */

  while(fgets(tbuf,256,stdin)!=NULL)
    {
    sscanf(tbuf,"%d%d%d%d%d%lf%f%f%f%lf%s%s",
      &tm[0],&tm[1],&tm[2],&tm[3],&tm[4],&se,
      &d.alat,&d.along,&d.dep,&mag,owner,diag);
    adj_sec(tm,&se,tmc,&sec);
    d.time[0]=(unsigned char)tmc[0];
    d.time[1]=(unsigned char)tmc[1];
    d.time[2]=(unsigned char)tmc[2];
    d.time[3]=(unsigned char)tmc[3];
    d.time[4]=(unsigned char)tmc[4];
    d.time[5]=(unsigned char)((int)sec);
    d.time[6]=(unsigned char)(((int)(sec*10.0))%10);
    d.time[7]=(int)(mag*10.0+0.5);
    *d.diag=(*d.owner)=0;
    if(*diag) strncpy(d.diag,diag,4);
    if(*owner) strncpy(d.owner,owner,4);
    i=1;if(*(char *)&i)
      {
      SWAPF(d.alat);
      SWAPF(d.along);
      SWAPF(d.dep);  
      }
    fwrite(&d,sizeof(d),1,stdout);
    }
  }
