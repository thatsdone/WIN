/******************************************************************/
/*    finalb2seis.c                8/19/92-6/1/93 urabe           */
/*    How to use (for example),                                   */
/*       finalb2seis /dat/seis/eri (ogino) < [finalb file]        */
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
  char fname[100],fname1[100];
  int i,j,no,start,year;
  unsigned char b[16];
  FILE *fp;
  struct {
    char time[8]; /* Y,M,D,h,m,s,s10,mag10 */
    float alat,along,dep;
    char diag[4],owner[4];
    } d;      /* 28 bytes / event */
#if BYTE_ORDER == LITTLE_ENDIAN
  SWAPU;
#endif

  if(argc<2)
    {
    fprintf(stderr,"usage of finalb2seis :\n");
    fprintf(stderr," (ex.) finalb2seis /dat/seis/eri (ogino) < [finalb file]\n");
    exit(1);
    }
  j=0;
  start=1;
  while(fread(&d,sizeof(d),1,stdin)>0)
    {
    if(argc>2 && strncmp(argv[2],d.owner,4)) continue;
    if(start)
      {
      sprintf(fname,"%s-%d.b",argv[1],d.time[0]);
      strcpy(fname1,fname);
      fp=fopen(fname,"w+");
      b[0]=b[1]=b[2]=b[3]=0;
      fwrite(b,1,4,fp);
      j=start=0;
      no=1;
      }
    else if(d.time[0]>year)   /* a new year */
      {
      b[0]=b[1]=0xff;
      fwrite(b,1,2,fp);
      fclose(fp);
      sprintf(fname,"%s-%d.b",argv[1],d.time[0]);
      strcpy(fname1,fname);
      fp=fopen(fname,"w+");
      b[0]=b[1]=b[2]=b[3]=0;
      fwrite(b,1,4,fp);
      j=0;
      no=1;
      }
    year=d.time[0];
    if(j==4095)
      {
      no++;
      b[0]=(-no);
      b[1]=0xff;
      fwrite(b,1,2,fp);
      fclose(fp);
      sprintf(fname+strlen(fname1),"%d",no);
      fp=fopen(fname,"w+");
      b[0]=0;
      b[1]=0;
      b[2]=0;
      b[3]=0;
      fwrite(b,1,4,fp);
      j=0;
      }
    b[0]=d.time[0]; /* year */
    b[1]=d.time[1]; /* mon */
    b[2]=d.time[2]; /* day */
    b[3]=d.time[3]; /* hour */
    b[4]=d.time[4]; /* min */
    i=d.time[5]*10+d.time[6];
    b[5]=i%256;
    b[6]=i/256;
#if BYTE_ORDER == LITTLE_ENDIAN
    SWAPF(d.alat);
    SWAPF(d.along);
    SWAPF(d.dep);  
#endif
    b[7]=(int)d.along;
    b[8]=(int)((d.along-(float)b[7])*60.0);
    b[9]=(int)((d.along-(float)b[7]-((float)b[8])/60.0)*3600.0+0.5);
    b[10]=(int)d.alat;
    b[11]=(int)((d.alat-(float)b[10])*60.0);
    b[12]=(int)((d.alat-(float)b[10]-((float)b[11])/60.0)*3600.0+0.5);
    if(d.dep<0.0) d.dep=0.0;    /**************/
    i=(int)(d.dep*10.0+0.5);
    b[13]=i%256;
    b[14]=i/256;
    if(d.time[7]==99) b[15]=0;    /**************/
    else if(d.time[7]>0) b[15]=d.time[7];
    else b[15]=0x80-d.time[7];    /* 5/25/93 */
    fwrite(b,1,16,fp);
    j++;
    }
  if(j)
    {
    b[0]=b[1]=0xff;
    fwrite(b,1,2,fp);
    fclose(fp);
    }
  }
