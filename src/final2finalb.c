/******************************************************************/
/*    final2finalb.c              9/21/94 urabe                   */
/*    97.10.3 FreeBSD                                             */
/******************************************************************/
#include  <stdio.h>

/*****************************/
/* 3/22/95 for little-endian */
/* 8/6/96 Uehira */
#ifndef         LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234    /* LSB first: i386, vax */
#endif
#ifndef         BIG_ENDIAN
#define BIG_ENDIAN      4321    /* MSB first: 68000, ibm, net */
#endif
#ifndef  BYTE_ORDER
#define  BYTE_ORDER      BIG_ENDIAN
#endif        

#define SWAPU  union { long l; float f; short s; char c[4];} swap
#define SWAPL(a) swap.l=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPF(a) swap.f=(a); ((char *)&(a))[0]=swap.c[3];\
    ((char *)&(a))[1]=swap.c[2]; ((char *)&(a))[2]=swap.c[1];\
    ((char *)&(a))[3]=swap.c[0]
#define SWAPS(a) swap.s=(a); ((char *)&(a))[0]=swap.c[1];\
    ((char *)&(a))[1]=swap.c[0]
/*****************************/

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

adj_time(tm)
  int *tm;
  {
  if(tm[5]==60)
    {
    tm[5]=0;
    if(++tm[4]==60)
      {
      tm[4]=0;
      if(++tm[3]==24)
        {
        tm[3]=0;
        tm[2]++;
        switch(tm[1])
          {
          case 2:
            if(tm[0]%4==0)
              {if(tm[2]==30) {tm[2]=1;tm[1]++;}break;}
            else
              {if(tm[2]==29) {tm[2]=1;tm[1]++;}break;}
          case 4:
          case 6:
          case 9:
          case 11:  if(tm[2]==31) {tm[2]=1;tm[1]++;}break;
          default:  if(tm[2]==32) {tm[2]=1;tm[1]++;}break;
          }
        if(tm[1]==13)
          {
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
          }
        }
      }
    }
  else if(tm[5]==-1)
    {
    tm[5]=59;
    if(--tm[4]==-1)
      {
      tm[4]=59;
      if(--tm[3]==-1)
        {
        tm[3]=23;
        if(--tm[2]==0)
          {
          switch(--tm[1])
            {
            case 2:
              if(tm[0]%4==0) tm[2]=29;else tm[2]=28;break;
            case 4:
            case 6:
            case 9:
            case 11:  tm[2]=30;break;
            default:  tm[2]=31;break;
            }
          if(tm[1]==0)
            {
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
            }
          }
        }
      }
    }
  return 0;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  int tm[6],tmc[7];
  double se,sec,mag;
  char tbuf[256],owner[20],diag[20];
  struct {
    char time[8]; /* Y,M,D,h,m,s,s10,mag10 */
    float alat,along,dep;
    char diag[4],owner[4];
    } d;      /* 28 bytes / event */
#if BYTE_ORDER == LITTLE_ENDIAN
  SWAPU;     
#endif

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
#if BYTE_ORDER == LITTLE_ENDIAN
    SWAPF(d.alat);
    SWAPF(d.along);
    SWAPF(d.dep);  
#endif
    fwrite(&d,sizeof(d),1,stdout);
    }
  }
