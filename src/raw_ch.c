/* $Id: raw_ch.c,v 1.1.2.2 2001/11/19 01:59:47 uehira Exp $ */
/* "raw_ch.c"    99.12.8 urabe */
/*                  modified from raw_raw.c */
/*                  byte-order-free */
/*                  2000.3.21 c_save=shr->c; bug fixed */
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
#include <errno.h>

#include "subst_func.h"

#define DEBUG       0
#define BELL        0

int ch_table[65536];
char *progname,logfile[256],chfile[256];
int n_ch,negate_channel;

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
  int i,j,k,kk;
  char tbuf[1024];
  if(*chfile)
    {
    if((fp=fopen(chfile,"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile);
#endif
      if(negate_channel) for(i=0;i<65536;i++) ch_table[i]=(-1);
      else for(i=0;i<65536;i++) ch_table[i]=i;
      j=0;
      while(fgets(tbuf,1024,fp))
        {
        if(*tbuf=='#' || sscanf(tbuf,"%x%x",&k,&kk)<0) continue;
        k&=0xffff;
        kk&=0xffff;
#if DEBUG
        fprintf(stderr," %04X->%04X",k,kk);
#endif
        ch_table[k]=kk;
        j++;
        i++;
        }
#if DEBUG
      fprintf(stderr,"\n");
#endif
      n_ch=j;
      if(negate_channel) sprintf(tbuf,"%d channels exclusively",n_ch);
      else sprintf(tbuf,"%d channels set",n_ch);
      write_log(logfile,tbuf);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile);
#endif
      sprintf(tbuf,"channel list file '%s' not open",chfile);
      write_log(logfile,tbuf);
      write_log(logfile,"end");
      exit(1);
      }
    }
  else
    {
    for(i=0;i<65536;i++) ch_table[i]=i;
    n_ch=i;
    write_log(logfile,"all channels");
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
  int sr,i,j,k,size,n,size_shm,tow;
  unsigned long c_save;
  unsigned short ch;
  int gs,gh;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<4)
    {
    fprintf(stderr,
      " usage : '%s [in_key] [out_key] [shm_size(KB)] \\\n",
      progname);
    fprintf(stderr,
      "                       (-/[ch_file]/-[ch_file] ([log file]))'\n",
      progname);
    exit(1);
    }
  rawkey=atoi(argv[1]);
  monkey=atoi(argv[2]);
  size_shm=atoi(argv[3])*1000;
  *chfile=(*logfile)=0;
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
      else
        {
        strcpy(chfile,argv[4]);
        negate_channel=0;
        }
      }
    }    
  if(argc>5) strcpy(logfile,argv[5]);
    
  read_chfile();

  /* in shared memory */
  if((shmid_raw=shmget(rawkey,0,0))<0) err_sys("shmget in");
  if((shr=(struct Shm *)shmat(shmid_raw,(char *)0,0))==
      (struct Shm *)-1) err_sys("shmat in");

  /* out shared memory */
  if((shmid_mon=shmget(monkey,size_shm,IPC_CREAT|0666))<0)
    err_sys("shmget out");
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
        write_log(logfile,tb);
        if(tow==0)
          {
          write_log(logfile,"reset");
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
      write_log(logfile,"without TOW");
      if(tow==1)
        {
        write_log(logfile,"reset");
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
      if(ch_table[ch]>=0)
        {
#if DEBUG
        fprintf(stderr,"%04X->%04X(%5d)",ch,ch_table[ch],gs);
#endif
        if(ch_table[ch]!=ch)
          {
          ptr[0]=(ch_table[ch]>>8);        
          ptr[1]=(ch_table[ch]);        
          }
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
      write_log(logfile,"reset");
      goto reset;
      }
    }
  }
