/* $Id: cormeisei.c,v 1.2.2.2 2001/11/19 01:59:47 uehira Exp $ */
/* "cormeisei.c"    June'97 Ide changed from*/
/* "raw_raw.c"      3/4/96 urabe */
/*                  revised on 5/20/96 */
/*                  modified from raw_mon.c */
/*                  negate channel 5/24/96 */
/*                  usleep -> sleep */
/*                  97.7.25-  urabe */
/*                  97.9.5-   ide */
/*                  97.9.11   urabe dt[400],dout[400] -> dt[500],dout[500] */
/*                  97.9.17   win2fix ; dath[100]->[101] */
/*                  98.4.17   usleep, FreeBSD  urabe */
/*                  98.6.8    make CH_TOTAL & MAX_SEC_SIZE ext. definable */
/*                  98.10.22  applied patch on ach[30] by Ide */
/*                  99.2.4    moved signal(HUP) to read_chfile() by urabe */
/*                  99.4.19   byte-order-free by urabe */
/*                  2000.4.17 deleted definition of usleep() */
/*                  2000.4.24 strerror() */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else  /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif  /* !TIME_WITH_SYS_TIME */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "subst_func.h"

#define DEBUG     0
#define DEBUG1    0
#ifndef CH_TOTAL
#define CH_TOTAL  200  /* max N of meisei chs */
#endif
#ifndef MAX_SEC_SIZE
#define MAX_SEC_SIZE  500000  /* max size of one sec data in bytes */
#endif

#define XPM       30
#define YPM       30
#define IAH       26
#define IBH       27
#define TDLYH     0.27
#define IAL       26
#define IBL       27
#define TDLYL     1.35

short ch_tableh[65536], ch_tablel[65536];
char *progname,logfile[256],chfile[256];
int n_chh, ch_orderh[CH_TOTAL], n_chl, ch_orderl[CH_TOTAL];

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

get_time(rt)
int *rt;
{
  struct tm *nt;
  unsigned long ltime;
  time(&ltime);
  nt=localtime(&ltime);
  rt[0]=nt->tm_year%100;
  rt[1]=nt->tm_mon+1;
  rt[2]=nt->tm_mday;
  rt[3]=nt->tm_hour;
  rt[4]=nt->tm_min;
  rt[5]=nt->tm_sec;
}

write_log(logfil,ptr)
char *logfil;
char *ptr;
{
  FILE *fp;
  int tm[6];
  if(*logfil) fp=fopen(logfil,"a");
  else fp=stdout;
  get_time(tm);
  fprintf(fp,"%02d%02d%02d.%02d%02d%02d %s %s\n",
    tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
  if(*logfil) fclose(fp);
}

ctrlc()
{
  write_log(logfile,"end");
  exit(0);
}

err_sys(ptr)
char *ptr;
{
  perror(ptr);
  write_log(logfile,ptr);
  if(strerror(errno)) write_log(logfile,strerror(errno));
  ctrlc();
}

read_chfile()
{
FILE *fp;
int i,jh,jl,k,sp;
char tbuf[256];
  if((fp=fopen(chfile,"r"))!=NULL) {
#if DEBUG
    fprintf(stderr,"ch_file=%s\n",chfile);
#endif
    n_chh=0;
    n_chl=0;
    for(i=0;i<65536;i++) ch_tableh[i]=(-1);
    for(i=0;i<65536;i++) ch_tablel[i]=(-1);
    i=jh=jl=0;
    while(fgets(tbuf,256,fp)) {
      if(*tbuf=='#') continue;
      sscanf(tbuf,"%x %d",&k,&sp);
      k&=0xffff;
      if(ch_tableh[k]<0 && sp == 100){
#if DEBUG
      fprintf(stderr,"100Hz %04X",k);
#endif
        ch_tableh[k]=jh;
        ch_orderh[jh]=k;
        jh++;
      }
      if(ch_tablel[k]<0 && sp == 20){
#if DEBUG
      fprintf(stderr,"20Hz %04X",k);
#endif
        ch_tablel[k]=jl;
        ch_orderl[jl]=k;
        jl++;
      }
    }
#if DEBUG
    fprintf(stderr,"\n",k);
#endif
    n_chh=jh;
    n_chl=jl;
    sprintf(tbuf,"100Hz %d channels  20Hz %d channels",n_chh,n_chl);
    write_log(logfile,tbuf);
  } else {
#if DEBUG
    fprintf(stderr,"ch_file '%s' not open\n",chfile);
#endif
    sprintf(tbuf,"channel list file '%s' not open",chfile);
    write_log(logfile,tbuf);
    write_log(logfile,"end");
    exit(1);
  }
  signal(SIGHUP,(void *)read_chfile);
}

/* winform.c  4/30/91   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */

winform(inbuf,outbuf,sr,sys_ch)
  long *inbuf;      /* input data array for one sec*/
  unsigned char *outbuf;  /* output data array for one sec */
  int sr;         /* n of data (i.e. sampling rate) */
  unsigned short sys_ch;  /* 16 bit long channel ID number */
  {
  int dmin,dmax,aa,bb,br,i,byte_leng;
  long *ptr;
  unsigned char *buf;

  /* differentiate and obtain min and max */
  ptr=inbuf;
  bb=(*ptr++);
  dmax=dmin=0;
  for(i=1;i<sr;i++)
    {
    aa=(*ptr);
    *ptr++=br=aa-bb;
    bb=aa;
    if(br>dmax) dmax=br;
    else if(br<dmin) dmin=br;
    }

  /* determine sample size */
  if(((dmin&0xfffffff8)==0xfffffff8 || (dmin&0xfffffff8)==0) &&
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0)) byte_leng=0;
  else if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0)) byte_leng=1;
  else if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0)) byte_leng=2;
  else if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0)) byte_leng=3;
  else byte_leng=4;
  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  *buf++=inbuf[0]>>24;
  *buf++=inbuf[0]>>16;
  *buf++=inbuf[0]>>8;
  *buf++=inbuf[0];
  /* second and after */
  switch(byte_leng)
    {
    case 0:
      for(i=1;i<sr-1;i+=2)
        *buf++=(inbuf[i]<<4)|(inbuf[i+1]&0xf);
      if(i==sr-1) *buf++=(inbuf[i]<<4);
      break;
    case 1:
      for(i=1;i<sr;i++)
        *buf++=inbuf[i];
      break;
    case 2:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>24;
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    }
  return (int)(buf-outbuf);
  }

win2fix(ptr,abuf,sys_ch,sr) /* returns group size in bytes */
  unsigned char *ptr; /* input */
  register long *abuf;/* output */
  long *sys_ch;       /* sys_ch */
  long *sr;           /* sr */
  {
  int b_size,g_size;
  register int i,s_rate;
  register unsigned char *dp;
  unsigned int gh;
  short shreg;
  int inreg;

  dp=ptr;
  gh=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  *sr=s_rate=gh&0xfff;
/*  if(s_rate>MAX_SR) return 0;*/
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16)&0xffff;

  /* read group */
  abuf[0]=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  if(s_rate==1) return g_size;  /* normal return */
  switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        abuf[i]=abuf[i-1]+((*(char *)dp)>>4);
        abuf[i+1]=abuf[i]+(((char)(*(dp++)<<4))>>4);
        }
      break;
    case 1:
      for(i=1;i<s_rate;i++)
        abuf[i]=abuf[i-1]+(*(char *)(dp++));
      break;
    case 2:
      for(i=1;i<s_rate;i++)
        {
        shreg=((dp[0]<<8)&0xff00)+(dp[1]&0xff);
        dp+=2;
        abuf[i]=abuf[i-1]+shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00);
        dp+=3;
        abuf[i]=abuf[i-1]+(inreg>>8);
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
        dp+=4;
        abuf[i]=abuf[i-1]+inreg;
        }
      break;
    default:
      return 0; /* bad header */
    }
  return g_size;  /* normal return */
  }

main(argc,argv)
int argc;
char *argv[];
{
  FILE *fp;
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *shr,*shm;
  key_t rawkey,monkey;
  int shmid_raw,shmid_mon;
  unsigned long uni;
  char tb[100];
  unsigned char *ptr,*ptw,tm[6],*ptr_lim,*ptr_save;
  static unsigned char dbuf[12][MAX_SEC_SIZE],ch_flagh[12][CH_TOTAL],
    ch_flagl[12][CH_TOTAL];
  int sr,i,j,k,size,n,size_shm,itdl,itdh,ich,ch;
  unsigned long c_save;
  int gs,gh,wf;
  int iah=IAH,ibh=IBH;
  int ial=IAL,ibl=IBL;
  int idb01=0,idb02=0,idb03=0,idb04=0,idb05=0,idb06=0;
  int idb07=0,idb08=0,idb09=0,idb10=0,idb11=0,idb12=0;
  int idx0a,idx0b,idx01,idx02,idx03,idx04,idx05;
  int idx06,idx07,idx08,idx09,idx10,idx11,idx12;
  int iddx01,iddx02,iddx03,iddx04,iddx05,iddx06;
  int iddx07,iddx08,iddx09,iddx10,iddx11,iddx12;
  float tdlyh=TDLYH;
  float tdlyl=TDLYL;
  static long dath[14][CH_TOTAL][101],datl[14][CH_TOTAL][21];
  long dt[500],xp[XPM],*pd1,*pd2;
  double yp[YPM],dout[500],*pdd;

  static double ach[30]=
     {2.4880006e-001, 1.7382422e-001, -3.5318485e-001, 3.6853228e-001,
     -2.9050902e-001, 1.7614073e-001, -6.5745463e-002, -1.7483372e-002, 
      6.5351046e-002, -8.0715525e-002, 7.2767317e-002, -5.2601314e-002, 
      2.9894632e-002, -1.1082363e-002, -1.0129346e-003, 6.4907626e-003, 
     -7.1588663e-003, 5.3158303e-003, -2.8750834e-003, 9.6577354e-004, 
      5.4531441e-005, -3.4382360e-004, 2.8514264e-004, 6.0093594e-005,
      1.2436903e-006, 6.8459746e-010};
  static double bch[30]=
    {-6.8459746e-010, -1.2436903e-006, -6.0093594e-005, -2.8514264e-004,
      3.4382360e-004, -5.4531441e-005, -9.6577354e-004, 2.8750834e-003,
     -5.3158303e-003, 7.1588663e-003, -6.4907626e-003, 1.0129346e-003,
      1.1082363e-002, -2.9894632e-002, 5.2601314e-002, -7.2767317e-002,
      8.0715525e-002, -6.5351046e-002, 1.7483372e-002, 6.5745463e-002,
     -1.7614073e-001, 2.9050902e-001, -3.6853228e-001, 3.5318485e-001,
     -1.7382422e-001, -2.4880006e-001, 1.0000000e+000};
  static double acl[30]=
     {2.4879237e-001, 1.7382806e-001, -3.5318485e-001, 3.6852992e-001,
     -2.9050579e-001, 1.7613769e-001, -6.5743223e-002, -1.7484628e-002,
      6.5351419e-002, -8.0715274e-002, 7.2766738e-002, -5.2600661e-002,
      2.9894073e-002, -1.1081976e-002, -1.0131453e-003, 6.4908365e-003,
     -7.1588578e-003, 5.3157880e-003, -2.8750396e-003, 9.6574320e-004,
      5.4546391e-005, -3.4382778e-004, 2.8514186e-004, 6.0095514e-005,
      1.2438215e-006, 6.8489467e-010};
  static double bcl[30]=
    {-6.8489467e-010, -1.2438215e-006, -6.0095514e-005, -2.8514186e-004,
      3.4382778e-004, -5.4546391e-005, -9.6574320e-004, 2.8750396e-003,
     -5.3157880e-003, 7.1588578e-003, -6.4908365e-003, 1.0131453e-003,
      1.1081976e-002, -2.9894073e-002, 5.2600661e-002, -7.2766738e-002,
      8.0715274e-002, -6.5351419e-002, 1.7484628e-002, 6.5743223e-002,
     -1.7613769e-001, 2.9050579e-001, -3.6852992e-001, 3.5318485e-001,
     -1.7382806e-001, -2.4879237e-001, 1.0000000e+000};

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<5)
    {
    fprintf(stderr,
      " usage : '%s [in_key] [out_key] [shm_size(KB)] [ch_file] ([log file])'\n",
      progname);
    exit(1);
    }
  rawkey=atoi(argv[1]);
  monkey=atoi(argv[2]);
  size_shm=atoi(argv[3])*1000;
  strcpy(chfile,argv[4]);
  if(argc>5) strcpy(logfile,argv[5]);
    
  read_chfile();

  /* in shared memory */
  if((shmid_raw=shmget(rawkey,0,0))<0) err_sys("shmget in");
  if((shr=(struct Shm *)shmat(shmid_raw,(char *)0,0))==
      (struct Shm *)-1) err_sys("shmat in");

  /* out shared memory */
  if((shmid_mon=shmget(monkey,size_shm,IPC_CREAT|0666))<0) err_sys("shmget out");
  if((shm=(struct Shm *)shmat(shmid_mon,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat out");

  sprintf(tb,"start in_key=%d id=%d out_key=%d id=%d size=%d",
    rawkey,shmid_raw,monkey,shmid_mon,size_shm);
  write_log(logfile,tb);

  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);

reset:
  /* initialize buffer */
  shm->p=shm->c=0;
  shm->pl=(size_shm-sizeof(*shm))/10*9;
  shm->r=(-1);

  ptr=shr->d;
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;
  wf=(-5);
  idx0b=0;
  idx0a=1;
  idx01=2;
  idx02=3;
  idx03=4;
  idx04=5;
  idx05=6;
  idx06=7;
  idx07=8;
  idx08=9;
  idx09=10;
  idx10=11;
  idx11=12;
  idx12=13;
  iddx01=0;
  iddx02=1;
  iddx03=2;
  iddx04=3;
  iddx05=4;
  iddx06=5;
  iddx07=6;
  iddx08=7;
  iddx09=8;
  iddx10=9;
  iddx11=10;
  iddx12=11;
  itdh=tdlyh*100;
  itdl=tdlyl*20;
#if DEBUG
  fprintf(stderr,"100Hz itd = %d \n",itdh);
  fprintf(stderr,"20Hz itd = %d \n",itdl);
#endif

  while(1) {
    ptr_lim=ptr+(size=mklong(ptr_save=ptr));
    c_save=shm->c;
    ptr+=4;
#if DEBUG1
    for(i=0;i<6;i++) fprintf(stderr,"%02X",ptr[i]);
    fprintf(stderr," : %d R wf=%d um=%d\n",size,wf,idb01);
#endif
  /* make output data */
    ptw=shm->d+shm->p;
    ptw+=4;               /* size (4) */
    for(i=0;i<idb01;i++) *ptw++=dbuf[iddx01][i]; /* Unmodified */
    if(wf){
      for(ich=0;ich<n_chh;ich++) {
        if(ch_flagh[iddx01][ich]){
          pd1=dt;
          pd2=dath[idx04][ich]+99;
          for(i=0;i<100;i++) *pd1++=(*pd2--);
          pd2=dath[idx03][ich]+99;
          for(i=0;i<100;i++) *pd1++=(*pd2--);
          pd2=dath[idx02][ich]+99;
          for(i=0;i<100;i++) *pd1++=(*pd2--);
          pd2=dath[idx01][ich]+99;
          for(i=0;i<100;i++) *pd1++=(*pd2--);
          pd2=dath[idx0a][ich]+99;
          for(i=0;i<100;i++) *pd1++=(*pd2--);
          for(i=0;i<500;i++){
            dout[i]=0.0;
            for(k=0;k<ibh;k++) if((i-k)>=0) dout[i]+=(double)dt[i-k]*bch[k];
            for(k=0;k<iah;k++) if((i-k)>=1) dout[i]+=dout[i-k-1]*ach[k];
          }
          pd1=dt;
          pdd=dout+399+itdh;
          for(i=0;i<100;i++) *pd1++=(long)(*pdd--);
          ptw+=winform(dt,ptw,100,ch_orderh[ich]);
#if DEBUG
          fprintf(stderr,"HIGH %04X ",ch_orderh[ich]);
#endif
        }
      }
    } else {
      for(ich=0;ich<n_chh;ich++) {
        if(ch_flagh[iddx01][ich]){
          pd1=dt;
          pd2=dath[idx01][ich];
          for(i=0;i<100;i++) *pd1++=(*pd2++);
          ptw+=winform(dt,ptw,100,ch_orderh[ich]);
        }
      }
    }
#if DEBUG
          fprintf(stderr,"\n");
#endif
    if(wf){
      for(ich=0;ich<n_chl;ich++) {
        if(ch_flagl[iddx01][ich]){
          pd1=dt;
          pd2=datl[idx12][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx11][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx10][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx09][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx08][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx07][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx06][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx05][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx04][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx03][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx02][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx01][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx0a][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          pd2=datl[idx0b][ich]+19;
          for(i=0;i<20;i++) *pd1++=(*pd2--);
          for(i=0;i<280;i++){
            dout[i]=0.0;
            for(k=0;k<ibl;k++) if((i-k)>=0) dout[i]+=(double)dt[i-k]*bcl[k];
            for(k=0;k<ial;k++) if((i-k)>=1) dout[i]+=dout[i-k-1]*acl[k];
          }
          pd1=dt;
          pdd=dout+239+itdl;
          for(i=0;i<20;i++) *pd1++=(long)(*pdd--);
          ptw+=winform(dt,ptw,20,ch_orderl[ich]);
#if DEBUG
          fprintf(stderr,"LOW  %04X ",ch_orderl[ich]);
#endif
        }
      }
    } else {
      for(ich=0;ich<n_chl;ich++) {
        if(ch_flagl[iddx01][ich]){
          pd1=dt;
          pd2=datl[idx01][ich];
          for(i=0;i<100;i++) *pd1++=(*pd2++);
          ptw+=winform(dt,ptw,20,ch_orderl[ich]);
        }
      }
    }
#if DEBUG
          fprintf(stderr,"\n");
#endif
	
    i=idx0b;
    idx0b=idx0a;
    idx0a=idx01;
    idx01=idx02;
    idx02=idx03;
    idx03=idx04;
    idx04=idx05;
    idx05=idx06;
    idx06=idx07;
    idx07=idx08;
    idx08=idx09;
    idx09=idx10;
    idx10=idx11;
    idx11=idx12;
    idx12=i;
    if((uni=ptw-(shm->d+shm->p))>10) {
      uni=ptw-(shm->d+shm->p);
      shm->d[shm->p  ]=uni>>24; /* size (H) */
      shm->d[shm->p+1]=uni>>16;
      shm->d[shm->p+2]=uni>>8;
      shm->d[shm->p+3]=uni;     /* size (L) */
#if DEBUG1
      for(i=0;i<6;i++)fprintf(stderr,"%02X",shm->d[shm->p+4+i]);
      fprintf(stderr," : %d M um=%d\n",uni,idb01);
#endif
      shm->r=shm->p;
      if(ptw>shm->d+shm->pl) ptw=shm->d;
      shm->p=ptw-shm->d;
      shm->c++;
    }

    i=iddx01;
    iddx01=iddx02;
    iddx02=iddx03;
    iddx03=iddx04;
    iddx04=iddx05;
    iddx05=iddx06;
    iddx06=iddx07;
    iddx07=iddx08;
    iddx08=iddx09;
    iddx09=iddx10;
    iddx10=iddx11;
    iddx11=iddx12;
    iddx12=i;
    for(i=0;i<n_chh;i++) ch_flagh[iddx12][i]=0;
    for(i=0;i<n_chl;i++) ch_flagl[iddx12][i]=0;
    idb01=idb02;
    idb02=idb03;
    idb03=idb04;
    idb04=idb05;
    idb05=idb06;
    idb06=idb07;
    idb07=idb08;
    idb08=idb09;
    idb09=idb10;
    idb10=idb11;
    idb11=idb12;
    for(i=0;i<6;i++) dbuf[iddx12][i]=(*ptr++); /* YMDhms (6) */
    idb12=6;
#if DEBUG1
    j=0;
#endif
    do {   /* loop for ch's */
      gh=mklong(ptr);
      ch=(gh>>16)&0xffff;
      sr=gh&0xfff;
      if((ich=ch_tableh[ch])>=0 && sr==100){
        gs=win2fix(ptr,dath[idx12][ich],&ch,&sr);
        ch_flagh[iddx12][ich]=1;
#if DEBUG
        fprintf(stderr,"+");
#endif
      } else if((ich=ch_tablel[ch])>=0 && sr==20){
        gs=win2fix(ptr,datl[idx12][ich],&ch,&sr);
        ch_flagl[iddx12][ich]=1;
#if DEBUG
        fprintf(stderr,"+");
#endif
      } else {
        if((gh>>12)&0xf) gs=((gh>>12)&0xf)*(sr-1)+8;
        else gs=(sr>>1)+8;
        for(i=0;i<gs;i++) dbuf[iddx12][idb12+i]=ptr[i];
        idb12+=gs;
      }
#if DEBUG
      fprintf(stderr,"%d ",gs);
#endif
      ptr+=gs;
#if DEBUG1
    j++;
#endif
    } while(ptr<ptr_lim);
#if DEBUG1
    fprintf(stderr,"nch=%d\n",j);
#endif

    for(ich=0;ich<n_chh;ich++){
      if(ch_flagh[iddx12][ich]==0){
        pd1=dath[idx12][ich];
        for(i=0;i<100;i++) *pd1++=0;
      }
    }
    for(ich=0;ich<n_chl;ich++){
      if(ch_flagl[iddx12][ich]==0){
        pd1=datl[idx12][ich];
        for(i=0;i<20;i++) *pd1++=0;
      }
    }
#if DEBUG
    fprintf(stderr,"\n");
#endif

    if(wf<1) wf++;
    if((ptr=ptr_lim)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) usleep(100000);
    if(mklong(ptr_save)!=size) {
      write_log(logfile,"reset");
      goto reset;
    }
  }
}
