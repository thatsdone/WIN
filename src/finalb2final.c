/******************************************************************/
/*    finalb2final.c              6/10/94 urabe                   */
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
#if BYTE_ORDER == LITTLE_ENDIAN
  SWAPU;     
#endif

  while(fread(&d,sizeof(d),1,stdin)>0)
    {
#if BYTE_ORDER == LITTLE_ENDIAN
    SWAPF(d.alat);
    SWAPF(d.along);
    SWAPF(d.dep);  
#endif
    printf("%02d %02d %02d %02d %02d %02d.%d %.5f %.5f %5.1f %4.1f %.4s %.4s\n",
      d.time[0],d.time[1],d.time[2],d.time[3],d.time[4],
      d.time[5],d.time[6],d.alat,d.along,d.dep,
      ((float)d.time[7])*0.1,d.owner,d.diag);
    }
  }
