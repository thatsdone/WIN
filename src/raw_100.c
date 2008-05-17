/* $Id: raw_100.c,v 1.5.4.2 2008/05/17 14:22:01 uehira Exp $ */
/* "raw_100.c"    97.6.23 - 6.30 urabe */
/*                  modified from raw_raw.c */
/*                  97.8.4 bug fixed (output empty block) */
/*                  97.8.5 fgets/sscanf */
/*                  99.2.4 moved signal(HUP) to read_chfile() by urabe */
/*                  99.12.10 byte-order-free */
/*                  2000.3.21 c_save=shr->c; bug fixed */
/*                  2000.4.24/2001.11.14 strerror() */
/*                  2005.2.20 added fclose() in read_chfile() */

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
#include <errno.h>

#include "winlib.h"

#define DEBUG       0
#define BELL        0
#define MAX_SR   4095
#define SR_LOWER   50
#define SR        100

long buf_raw[MAX_SR];
unsigned char ch_table[65536];
char *progname,*logfile,chfile[256];
int n_ch,negate_channel;
int syslog_mode=0, exit_status;

read_chfile()
  {
  FILE *fp;
  int i,j,k;
  char tbuf[1024];
  if(*chfile)
    {
    if((fp=fopen(chfile,"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile);
#endif
      if(negate_channel) for(i=0;i<65536;i++) ch_table[i]=1;
      else for(i=0;i<65536;i++) ch_table[i]=0;
      i=j=0;
      while(fgets(tbuf,1024,fp))
        {
        if(*tbuf=='#' || sscanf(tbuf,"%x",&k)<0) continue;
        k&=0xffff;
#if DEBUG
        fprintf(stderr," %04X",k);
#endif
        if(negate_channel)
          {
          if(ch_table[k]==1)
            {
            ch_table[k]=0;
            j++;
            }
          }
        else
          {
          if(ch_table[k]==0)
            {
            ch_table[k]=1;
            j++;
            }
          }
        i++;
        }
#if DEBUG
      fprintf(stderr,"\n",k);
#endif
      n_ch=j;
      if(negate_channel) sprintf(tbuf,"-%d channels",n_ch);
      else sprintf(tbuf,"%d channels",n_ch);
      write_log(tbuf);
      fclose(fp);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile);
#endif
      sprintf(tbuf,"channel list file '%s' not open",chfile);
      write_log(tbuf);
      write_log("end");
      exit(1);
      }
    }
  else
    {
    for(i=0;i<65536;i++) ch_table[i]=1;
    n_ch=i;
    write_log("all channels");
    }
  signal(SIGHUP,(void *)read_chfile);
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
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
  int sr,i,j,k,size,n,size_shm,ch1,sr1,tow,rest;
  unsigned long c_save;
  unsigned short ch;
  int gs,gh;
  static int buf1[MAX_SR],buf2[SR];
  float t,ds,dt;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<4)
    {
    fprintf(stderr,
      " usage : '%s [in_key] [out_key] [shm_size(KB)] \\\n",
      progname);
    fprintf(stderr,
      "                       (-/[ch_file]/-[ch_file]/+[ch_file] ([log file]))'\n",
      progname);
    exit(1);
    }
  rawkey=atoi(argv[1]);
  monkey=atoi(argv[2]);
  size_shm=atoi(argv[3])*1000;
  *chfile=0;
  logfile=NULL;
  rest=1;
  exit_status = EXIT_SUCCESS;
  if(argc>4)
    {
    if(strcmp("-",argv[4])==0) *chfile=0;
    else
      {
      if(argv[4][0]=='-')
        {
        strcpy(chfile,argv[4]+1);
        negate_channel=1;
        }
      else if(argv[4][0]=='+')
        {
        strcpy(chfile,argv[4]+1);
        negate_channel=0;
        }
      else
        {
        strcpy(chfile,argv[4]);
        rest=negate_channel=0;
        }
      }
    }    
  if(argc>5) logfile=argv[5];
    
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
  write_log(tb);

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

reset:
  /* initialize buffer */
  shm->p=shm->c=0;
  shm->pl=(size_shm-sizeof(*shm))/10*9;
  shm->r=(-1);
  ptr=shr->d;
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;
  tow=(-1);

  while(1)
    {
    ptr_lim=ptr+(size=mklong(ptr_save=ptr));
    c_save=shr->c;
    ptr+=4;
#if DEBUG
    for(i=0;i<6;i++) printf("%02X",ptr[i]);
    printf(" : %d R\n",size);
#endif
  /* make output data */
    ptw=shm->d+shm->p;
    ptw+=4;               /* size (4) */
    uni=time(0);
    i=uni-mklong(ptr);
    if(i>=0 && i<1440)   /* with tow */
      {
      if(tow!=1)
        {
        sprintf(tb,"with TOW (diff=%ds)",i);
        write_log(tb);
        if(tow==0)
          {
          write_log("reset");
          goto reset;
          }
        tow=1;
        }
      ptr+=4;
      *ptw++=uni>>24;  /* tow (H) */
      *ptw++=uni>>16;
      *ptw++=uni>>8;
      *ptw++=uni;      /* tow (L) */
      }
    else if(tow!=0)
      {
      write_log("without TOW");
      if(tow==1)
        {
        write_log("reset");
        goto reset;
        }
      tow=0;
      }
    for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */

    do    /* loop for ch's */
      {
      gh=mklong(ptr);
      ch=(gh>>16)&0xffff;
      sr=gh&0xfff;
      if((gh>>12)&0xf) gs=((gh>>12)&0xf)*(sr-1)+8;
      else gs=(sr>>1)+8;
      if(ch_table[ch])
        {
#if DEBUG
        fprintf(stderr,"%5d",gs);
#endif
        if(sr!=SR && sr>=SR_LOWER) 
          {
          win2fix(ptr,(long *)buf1,(long *)&ch1,(long *)&sr1);
          ds=(float)sr/(float)SR;
          t=0.0;
          buf1[sr]=buf1[sr+1];
          for(i=0;i<SR;i++)
            {
            j=(int)t;
            dt=t-(float)j;
            buf2[i]=buf1[j]+(int)(dt*(float)(buf1[j+1]-buf1[j]));
            t+=ds;
            }
          ptw+=winform((long *)buf2,ptw,SR,ch);
          }
        else
          {
          memcpy(ptw,ptr,gs);
          ptw+=gs;
          }
        }
      else if(rest==1)
        {
        memcpy(ptw,ptr,gs);
        ptw+=gs;
        }
      ptr+=gs;
      } while(ptr<ptr_lim);
    if(tow) i=14;
    else i=10;
    if((uni=ptw-(shm->d+shm->p))>i)
      {
      uni=ptw-(shm->d+shm->p);
      shm->d[shm->p  ]=uni>>24; /* size (H) */
      shm->d[shm->p+1]=uni>>16;
      shm->d[shm->p+2]=uni>>8;
      shm->d[shm->p+3]=uni;     /* size (L) */

#if DEBUG
      for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+4+i]);
      printf(" : %d M\n",uni);
#endif

      shm->r=shm->p;
      if(ptw>shm->d+shm->pl) ptw=shm->d;
      shm->p=ptw-shm->d;
      shm->c++;
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    if((ptr=ptr_lim)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) sleep(1);
    if(shr->c<c_save || mklong(ptr_save)!=size)
      {
      write_log("reset");
      goto reset;
      }
    }
  }
