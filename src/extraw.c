/* $Id: extraw.c,v 1.4 2011/06/01 11:09:20 uehira Exp $ */
/* "extraw.c"    2000.3.17 urabe */
/* 2000.4.24/2001.11.14 strerror() */
/* 2010.9.10  64bit (Uehira) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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

#include "winlib.h"

#define BELL        0

static const char rcsid[] =
  "$Id: extraw.c,v 1.4 2011/06/01 11:09:20 uehira Exp $";

char *progname,*logfile;
int syslog_mode, exit_status;

/* prototype */
int main(int, char *[]);

int
main(int argc, char *argv[])
  {
  struct Shm  *shin,*shdat,*shctl;
  key_t inkey,datkey,ctlkey;
  /* int shmid_in,shmid_dat,shmid_ctl; */
  uint32_w  uni;  /* 64bit ok*/
  /* char tb[100]; */
  uint8_w  *ptr,*ptw,*ptr_lim,*ptr_save;
  int  i;
  uint32_w  size;
  size_t  size_shdat,size_shctl;
  unsigned long c_save;   /* 64bit ok*/

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];
  syslog_mode = 0;
  exit_status = EXIT_SUCCESS;

  if(argc<6)
    {
    WIN_version();
    fprintf(stderr, "%s\n", rcsid);
    fprintf(stderr,
      " usage : '%s [in_key] [dat_key] [shm_size(KB)] [ctl_key] [shm_size(KB)] \\\n",
      progname);
    fprintf(stderr,
      "                       ([log file]))'\n");
    exit(1);
    }
  inkey=atol(argv[1]);
  datkey=atol(argv[2]);
  size_shdat=(size_t)atol(argv[3])*1000;
  ctlkey=atol(argv[4]);
  size_shctl=(size_t)atol(argv[5])*1000;
  if(argc>6) logfile=argv[6];
  else   logfile=NULL;
    
  /* in shared memory */
  shin = Shm_read(inkey, "in");
  /* if((shmid_in=shmget(inkey,0,0))<0) err_sys("shmget in"); */
  /* if((shin=(struct Shm *)shmat(shmid_in,(void *)0,0))== */
  /*     (struct Shm *)-1) err_sys("shmat in"); */

  /* dat(out) shared memory */
  shdat = Shm_create(datkey, size_shdat, "dat");
  /* if((shmid_dat=shmget(datkey,size_shdat,IPC_CREAT|0666))<0) */
  /*   err_sys("shmget dat"); */
  /* if((shdat=(struct Shm *)shmat(shmid_dat,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat dat"); */

  /* ctl(out) shared memory */
  shctl = Shm_create(ctlkey, size_shctl, "ctl");
  /* if((shmid_ctl=shmget(ctlkey,size_shctl,IPC_CREAT|0666))<0) */
  /*   err_sys("shmget ctl"); */
  /* if((shctl=(struct Shm *)shmat(shmid_ctl,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat ctl"); */

  /* snprintf(tb,sizeof(tb), */
  /* 	  "start in_key=%ld id=%d dat_key=%ld id=%d size=%zu ctl_key=%ld id=%d size=%zu ", */
  /* 	  inkey,shmid_in,datkey,shmid_dat,size_shdat,ctlkey,shmid_ctl,size_shctl); */
  /* write_log(tb); */

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

reset:
  /* initialize buffer */
  Shm_init(shdat, size_shdat);
  /*   shdat->p=shdat->c=0; */
  /*   shdat->pl=(size_shdat-sizeof(*shdat))/10*9; */
  /*   shdat->r=(-1); */
  Shm_init(shctl, size_shctl);
  /*   shctl->p=shctl->c=0; */
  /*   shctl->pl=(size_shctl-sizeof(*shctl))/10*9; */
  /*   shctl->r=(-1); */

  ptr=shin->d;
  while(shin->r==(-1)) sleep(1);
  ptr=shin->d+shin->r;

  for(;;)
    {
    ptr_lim=ptr+(size=mkuint4(ptr_save=ptr));
    c_save=shin->c;
    ptr+=4; /* skip size */
    ptr+=4; /* skip tow */
#if DEBUG
    printf("%02X:",ptr[0]);
    for(i=1;i<7;i++) printf("%02X",ptr[i]);
    printf(" : %u ",size);
#endif
    i=ptr[0];
    if(i==0xA1 || i==0xA2 || i==0xA3 || i==0xA4) /* dat */
      {
      ptw=shdat->d+shdat->p;
      ptw+=4;               /* size (4) */
      uni=(uint32_w)(time(NULL)-TIME_OFFSET);
      *ptw++=uni>>24;  /* tow (H) */
      *ptw++=uni>>16;
      *ptw++=uni>>8;
      *ptw++=uni;      /* tow (L) */
      ptr++; /* skip label */
      while(ptr<ptr_lim-1) *ptw++=(*ptr++);

      uni=(uint32_w)(ptw-(shdat->d+shdat->p));
      shdat->d[shdat->p  ]=uni>>24; /* size (H) */
      shdat->d[shdat->p+1]=uni>>16;
      shdat->d[shdat->p+2]=uni>>8;
      shdat->d[shdat->p+3]=uni;     /* size (L) */
#if DEBUG
      printf("dat %u>%zu\n",uni,shdat->p);
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
      uni=(uint32_w)(time(NULL)-TIME_OFFSET);
      *ptw++=uni>>24;  /* tow (H) */
      *ptw++=uni>>16;
      *ptw++=uni>>8;
      *ptw++=uni;      /* tow (L) */
      for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */
      while(ptr<ptr_lim) *ptw++=(*ptr++);

      uni=(uint32_w)(ptw-(shctl->d+shctl->p));
      shctl->d[shctl->p  ]=uni>>24; /* size (H) */
      shctl->d[shctl->p+1]=uni>>16;
      shctl->d[shctl->p+2]=uni>>8;
      shctl->d[shctl->p+3]=uni;     /* size (L) */
#if DEBUG
      printf("ctl %u>%zu\n",uni,shctl->p);
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
