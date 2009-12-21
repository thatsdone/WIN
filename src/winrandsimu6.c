/* $Id: winrandsimu6.c,v 1.1.4.2 2009/12/21 10:00:14 uehira Exp $ */

/*  WIN random simulater, real-time version
     write 100Hz 3000ch data to stdout as WIN-text or shared memory
     
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
#include <sys/ipc.h>
#include <sys/shm.h>

#include "subst_func.h"

#define MAX_AMP 128
#define EVENT   60
#define NCH     3000
#define STACH   0xF000
#define SAMP    100
#define DELAY   50
#define TM2_STA 1247
#define TM2_CH  162
#define TM2_DEL 3
#define TM3_STA 2395
#define TM3_CH  153
#define TM3_DEL 12

#define SR    200
#define MAXCH 3600

#define USE_RND_DELAY

key_t shmkey_out;
struct Shm {
  unsigned long p;        /* write point */
  unsigned long pl;       /* write limit */
  unsigned long r;        /* Latest */
  unsigned long c;        /* counter */
  unsigned char d[1];     /* data buffer */
} *shm_out;
int shmid_out, shm_size=0;
unsigned char *ptw;

int winform(long *,unsigned char *,int,unsigned short);

double sinc(double x){
  if(fabs(x)<1.0e-8) return(1.);
  else return(sin(M_PI*x)/(M_PI*x));
}

int prt_now(int type){
  time_t tt;

  tt=time(NULL);
  if(type) fprintf(stderr,"NOW: %s",ctime(&tt));
  else fprintf(stdout,"NOW: %s",ctime(&tt));
  return(0);
}

int main(int argc, char **argv){
  int samp, nch, sch;
  int i, j, k, l, p, chid, dpoint[3];
  int *ev, *de;
  double r;
  time_t tnew, told;
#ifdef DEBUG
  int count;
#endif

#ifdef DEBUG
  prt_now(1);
#endif
  if(argc<3){
    fprintf(stderr,"Usage: %s shm_key shm_size\n",*argv);
    exit(0);
  }

  samp=SAMP;
  shmkey_out=atoi(*++argv);
  shm_size=atoi(*++argv);
  shm_size=shm_size*1000;
  nch=NCH; sch=STACH;

  if((ev=(int *)malloc((size_t)(nch*sizeof(int))))==NULL){
    fprintf(stderr,"Malloc Error.\n");
    free(ev);
    exit(0);
  }
  if((de=(int *)malloc((size_t)(nch*sizeof(int))))==NULL){
    fprintf(stderr,"Malloc Error.\n");
    free(de);
    exit(0);
  }

  if(shm_size>0){
    if((shmid_out=shmget(shmkey_out, shm_size, IPC_CREAT | 0666))<0)
      fprintf(stderr, "error shmget_out\n");
    if((shm_out=(struct Shm *)shmat(shmid_out,(void *)0,0))==(struct Shm *)-1)
      fprintf(stderr, "error shmget_out\n");
    /* initialize output buffer */
    shm_out->p = shm_out->c = 0;
    shm_out->pl = (shm_size - sizeof(*shm_out)) / 10 * 9;
    shm_out->r = -1;
    ptw = shm_out->d;
  }

#ifdef __FreeBSD__
  srandomdev();
#else
  srandom(time(NULL));
#endif

#ifdef DEBUG
  count=1;
#endif
  k=EVENT; l=0;
  for(j=0;j<nch;j++) ev[j]=0;
  told=tnew=time(NULL);
  while(1){
    if(tnew!=told){
      if(k>EVENT){
	for(j=0;j<nch;j++){
	  ev[j]+=floor(((double)random()/(double)RAND_MAX)*EVENT*samp);
	  de[j]=floor((double)random()/(double)RAND_MAX*samp/3.+10.);
	  /* fprintf(stderr,"%d %d %d\n",j,ev[j],de[j]); */
	}
	k=0;
      }
#ifdef USE_RND_DELAY
      for(j=0;j<3;j++)
	dpoint[j]=floor(((double)random()/(double)RAND_MAX)*nch);
#endif

      /*
      out_data(tnew,0,sch,0,nch,samp,ev,de,l,dpoint);
      */

      out_data(tnew,0,sch,0,TM2_STA,samp,ev,de,l,dpoint);
      out_data(tnew,TM2_DEL,sch,TM2_STA,TM2_CH,samp,ev,de,l,dpoint);
      out_data(tnew,0,sch,(TM2_STA+TM2_CH),(TM3_STA-(TM2_STA+TM2_CH)),samp,ev,de,l,dpoint);
      out_data(tnew,TM3_DEL,sch,TM3_STA,TM3_CH,samp,ev,de,l,dpoint);
      out_data(tnew,0,sch,(TM3_STA+TM3_CH),(nch-(TM3_STA+TM3_CH)),samp,ev,de,l,dpoint);

      k ++; l ++;
      told=tnew;

#ifdef DEBUG
      if(count>=30) break;
      count ++;
#endif
    }
    tnew=time(NULL);
  }

#ifdef DEBUG
  prt_now(1);
  fprintf(stderr,"Count = %d\n",count);
#endif

  free(de); free(ev);
  exit(0);
}

int out_data(time_t tnow,int del,int sch,int sta,int nch,int samp,int *ev,int *de,int l,int *dpoint){
  int i,j,p,tm[6];
  double r;
  static unsigned char outbuf[MAXCH][4*4*SR],tt[6],cbuf;
  static long inbuf[SR];
  int ii, size, chsize[MAXCH], t[6];
  char buf[18];
  struct tm *tn;
  time_t ttime;

  ttime=(time_t)(tnow-del);
  tn=localtime(&ttime);
  tm[0]=(tn->tm_year>100)?tn->tm_year-100:tn->tm_year;
  tm[1]=tn->tm_mon+1;
  tm[2]=tn->tm_mday; tm[3]=tn->tm_hour;
  tm[4]=tn->tm_min; tm[5]=tn->tm_sec;
  if(shm_size>0){
    sprintf(buf,"%2d %2d %2d %2d %2d %2d",tm[0],tm[1],tm[2],tm[3],tm[4],tm[5]);
    sscanf(buf,"%2x %2x %2x %2x %2x %2x",&t[0],&t[1],&t[2],&t[3],&t[4],&t[5]);
    for(ii=0;ii<6;ii++) tt[ii]=t[ii];
    size=0;
  }else{
    fprintf(stdout,"%02d %02d %02d %02d %02d %02d %d\n",
	    tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],nch);
  }
  for(j=sta;j<(sta+nch);j++){
    if(shm_size<=0) fprintf(stdout,"%04X %d",sch+j,samp);
    for(i=0;i<samp;i++){
      if((samp*l+i)>ev[j]&&(samp*l+i)<(ev[j]+de[j])){
	r=pow(20.,((double)random()/(double)RAND_MAX));
	r*=3.0*sinc((samp*l+i-ev[j])/de[j]);
      }else if((samp*l+i)>(ev[j]+de[j])&&(samp*l+i)<(ev[j]+3*de[j])){
	r=pow(50.,((double)random()/(double)RAND_MAX));
	r*=5.0*sinc((samp*l+i-(ev[j]+de[j]))/(2*de[j]));
      }else{
	r=pow(10.,((double)random()/(double)RAND_MAX));
      }	  
      p=floor(((double)random()/((double)RAND_MAX)-0.5)*r*MAX_AMP);
      if(shm_size>0){
	inbuf[i]=p;
      }else{
	fprintf(stdout," %d",p);
      }
    }
    if(shm_size>0){
      chsize[j]=winform(inbuf,outbuf[j],samp,sch+j);
      size+=chsize[j];
    }else{
      fprintf(stdout,"\n");
      fflush(stdout);
    }
#ifdef USE_RND_DELAY
    if(j==dpoint[0]||j==dpoint[1]||j==dpoint[2]) usleep(DELAY*1000);
#endif
  }
  if(shm_size>0){
    size+=10;
    cbuf = size >> 24; memcpy(ptw, &cbuf, 1); ptw++;
    cbuf = size >> 16; memcpy(ptw, &cbuf, 1); ptw++;
    cbuf = size >> 8; memcpy(ptw, &cbuf, 1); ptw++;
    cbuf = size; memcpy(ptw, &cbuf, 1); ptw++;
    memcpy(ptw, tt, 6); ptw += 6;
    for (j=sta;j<(sta+nch);j++) {
      memcpy(ptw, outbuf[j], chsize[j]);
      ptw += chsize[j];
    }
    if(ptw>(shm_out->d + shm_out->pl)) ptw=shm_out->d;
    shm_out->r = shm_out->p;
    shm_out->p = ptw - shm_out->d;
    shm_out->c++;
  }

  return(0);
}

/*
   win text format to win format
   2003.06.22- (C) Hiroshi TSURUOKA / All Rights Reserved.
        06.30  bug fix & add fflush
   2004.10.10  writing to share memory option
 */
/* $Id: winrandsimu6.c,v 1.1.4.2 2009/12/21 10:00:14 uehira Exp $ */
/* winform.c  4/30/91,99.4.19   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */

int winform(inbuf, outbuf, sr, sys_ch)
long *inbuf;                    /* input data array for one sec */
unsigned char *outbuf;          /* output data array for one sec */
int sr;                         /* n of data (i.e. sampling rate) */
unsigned short sys_ch;          /* 16 bit long channel ID number */
{
    int dmin, dmax, aa, bb, br, i, byte_leng;
    long *ptr;
    unsigned char *buf;

    /* differentiate and obtain min and max */
    ptr = inbuf;
    bb = (*ptr++);
    dmax = dmin = 0;
    for (i = 1; i < sr; i++) {
        aa = (*ptr);
        *ptr++ = br = aa - bb;
        bb = aa;
        if (br > dmax)
            dmax = br;
        else if (br < dmin)
            dmin = br;
    }

    /* determine sample size */
    if (((dmin & 0xfffffff8) == 0xfffffff8 || (dmin & 0xfffffff8) == 0) &&
        ((dmax & 0xfffffff8) == 0xfffffff8 || (dmax & 0xfffffff8) == 0))
        byte_leng = 0;
    else if (((dmin & 0xffffff80) == 0xffffff80 || (dmin & 0xffffff80) == 0) &&
             ((dmax & 0xffffff80) == 0xffffff80 || (dmax & 0xffffff80) == 0))
        byte_leng = 1;
    else if (((dmin & 0xffff8000) == 0xffff8000 || (dmin & 0xffff8000) == 0) &&
             ((dmax & 0xffff8000) == 0xffff8000 || (dmax & 0xffff8000) == 0))
        byte_leng = 2;
    else if (((dmin & 0xff800000) == 0xff800000 || (dmin & 0xff800000) == 0) &&
             ((dmax & 0xff800000) == 0xff800000 || (dmax & 0xff800000) == 0))
        byte_leng = 3;
    else
        byte_leng = 4;
    /* make a 4 byte long header */
    buf = outbuf;
    *buf++ = (sys_ch >> 8) & 0xff;
    *buf++ = sys_ch & 0xff;
    *buf++ = (byte_leng << 4) | (sr >> 8);
    *buf++ = sr & 0xff;

    /* first sample is always 4 byte long */
    *buf++ = inbuf[0] >> 24;
    *buf++ = inbuf[0] >> 16;
    *buf++ = inbuf[0] >> 8;
    *buf++ = inbuf[0];
    /* second and after */
    switch (byte_leng) {
    case 0:
        for (i = 1; i < sr - 1; i += 2)
            *buf++ = (inbuf[i] << 4) | (inbuf[i + 1] & 0xf);
        if (i == sr - 1)
            *buf++ = (inbuf[i] << 4);
        break;
    case 1:
        for (i = 1; i < sr; i++)
            *buf++ = inbuf[i];
        break;
    case 2:
        for (i = 1; i < sr; i++) {
            *buf++ = inbuf[i] >> 8;
            *buf++ = inbuf[i];
        }
        break;
    case 3:
        for (i = 1; i < sr; i++) {
            *buf++ = inbuf[i] >> 16;
            *buf++ = inbuf[i] >> 8;
            *buf++ = inbuf[i];
        }
        break;
    case 4:
        for (i = 1; i < sr; i++) {
            *buf++ = inbuf[i] >> 24;
            *buf++ = inbuf[i] >> 16;
            *buf++ = inbuf[i] >> 8;
            *buf++ = inbuf[i];
        }
        break;
    }
    return (int) (buf - outbuf);
}
