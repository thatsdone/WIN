/* $Id: extraw.c,v 1.3.4.3.2.1 2008/11/11 15:19:47 uehira Exp $ */
/* "extraw.c"    2000.3.17 urabe */
/* 2000.4.24/2001.11.14 strerror() */

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

char *progname,*logfile;
int syslog_mode = 0; exit_status;

main(argc,argv)
  int argc;
  char *argv[];
  {
  struct Shm  *shin,*shdat,*shctl;
  key_t inkey,datkey,ctlkey;
  int shmid_in,shmid_dat,shmid_ctl;
  unsigned long uni;
  char tb[100];
  unsigned char *ptr,*ptw,tm[6],*ptr_lim,*ptr_save;
  int i,j,k,size,n,size_shdat,size_shctl,tow;
  unsigned long c_save;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  exit_status = EXIT_SUCCESS;

  if(argc<6)
    {
    fprintf(stderr,
      " usage : '%s [in_key] [dat_key] [shm_size(KB)] [ctl_key] [shm_size(KB)] \\\n",
      progname);
    fprintf(stderr,
      "                       ([log file]))'\n");
    exit(1);
    }
  inkey=atoi(argv[1]);
  datkey=atoi(argv[2]);
  size_shdat=atoi(argv[3])*1000;
  ctlkey=atoi(argv[4]);
  size_shctl=atoi(argv[5])*1000;
  if(argc>6) logfile=argv[6];
  else   logfile=NULL;
    
  /* in shared memory */
  if((shmid_in=shmget(inkey,0,0))<0) err_sys("shmget in");
  if((shin=(struct Shm *)shmat(shmid_in,(char *)0,0))==
      (struct Shm *)-1) err_sys("shmat in");

  /* dat(out) shared memory */
  if((shmid_dat=shmget(datkey,size_shdat,IPC_CREAT|0666))<0)
    err_sys("shmget dat");
  if((shdat=(struct Shm *)shmat(shmid_dat,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat dat");

  /* ctl(out) shared memory */
  if((shmid_ctl=shmget(ctlkey,size_shctl,IPC_CREAT|0666))<0)
    err_sys("shmget ctl");
  if((shctl=(struct Shm *)shmat(shmid_ctl,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat ctl");

  sprintf(tb,"start in_key=%d id=%d dat_key=%d id=%d size=%d ctl_key=%d id=%d size=%d ",
    inkey,shmid_in,datkey,shmid_dat,size_shdat,ctlkey,shmid_ctl,size_shctl);
  write_log(tb);

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

reset:
  /* initialize buffer */
  shdat->p=shdat->c=0;
  shdat->pl=(size_shdat-sizeof(*shdat))/10*9;
  shdat->r=(-1);
  shctl->p=shctl->c=0;
  shctl->pl=(size_shctl-sizeof(*shctl))/10*9;
  shctl->r=(-1);
  ptr=shin->d;
  while(shin->r==(-1)) sleep(1);
  ptr=shin->d+shin->r;
  tow=(-1);

  while(1)
    {
    ptr_lim=ptr+(size=mkuint4(ptr_save=ptr));
    c_save=shin->c;
    ptr+=4; /* skip size */
    ptr+=4; /* skip tow */
#if DEBUG
    printf("%02X:",ptr[0]);
    for(i=1;i<7;i++) printf("%02X",ptr[i]);
    printf(" : %d ",size);
#endif
    i=ptr[0];
    if(i==0xA1 || i==0xA2 || i==0xA3 || i==0xA4) /* dat */
      {
      ptw=shdat->d+shdat->p;
      ptw+=4;               /* size (4) */
      uni=time(0);
      *ptw++=uni>>24;  /* tow (H) */
      *ptw++=uni>>16;
      *ptw++=uni>>8;
      *ptw++=uni;      /* tow (L) */
      ptr++; /* skip label */
      while(ptr<ptr_lim-1) *ptw++=(*ptr++);

      uni=ptw-(shdat->d+shdat->p);
      shdat->d[shdat->p  ]=uni>>24; /* size (H) */
      shdat->d[shdat->p+1]=uni>>16;
      shdat->d[shdat->p+2]=uni>>8;
      shdat->d[shdat->p+3]=uni;     /* size (L) */
#if DEBUG
      printf("dat %d>%d\n",uni,shdat->p);
#endif

      shdat->r=shdat->p;
      if(ptw>shdat->d+shdat->pl) ptw=shdat->d;
      shdat->p=ptw-shdat->d;
      shdat->c++;
      }
    else /* ctl */
      {
      ptw=shctl->d+shctl->p;
      ptw+=4;               /* size (4) */
      uni=time(0);
      *ptw++=uni>>24;  /* tow (H) */
      *ptw++=uni>>16;
      *ptw++=uni>>8;
      *ptw++=uni;      /* tow (L) */
      for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */
      while(ptr<ptr_lim) *ptw++=(*ptr++);

      uni=ptw-(shctl->d+shctl->p);
      shctl->d[shctl->p  ]=uni>>24; /* size (H) */
      shctl->d[shctl->p+1]=uni>>16;
      shctl->d[shctl->p+2]=uni>>8;
      shctl->d[shctl->p+3]=uni;     /* size (L) */
#if DEBUG
      printf("ctl %d>%d\n",uni,shctl->p);
#endif

      shctl->r=shctl->p;
      if(ptw>shctl->d+shctl->pl) ptw=shctl->d;
      shctl->p=ptw-shctl->d;
      shctl->c++;
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    if((ptr=ptr_lim)>shin->d+shin->pl) ptr=shin->d;
    while(ptr==shin->d+shin->p) sleep(1);
    if(shin->c<c_save || mkuint4(ptr_save)!=size)
      {
      write_log("reset");
      goto reset;
      }
    }
  }
