/* $Id: raw_ddp.c,v 1.1 2019/03/18 06:08:28 urabe Exp $ */

/* "raw_ddp.c"    2019.3.18 urabe */
/*                  modified from raw_raw.c */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
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

#include "daemon_mode.h"
#include "winlib.h"

#define DEBUG       0
#define BELL        0

static const char rcsid[] =
  "$Id: raw_ddp.c,v 1.1 2019/03/18 06:08:28 urabe Exp $";

static int daemon_mode;

char *progname,*logfile;
int  syslog_mode, exit_status;

/* prototypes */
static void usage(void);
int main(int, char *[]);

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if(daemon_mode)
    fprintf(stderr,
" usage : '%s (-B) (-n [pkts]) (-t [sec]) [in_key] [out_key] [shm_size(KB)] ([log file])'\n",progname);
  else
    fprintf(stderr,
" usage : '%s (-BD) (-n [pkts]) (-t [sec]) [in_key] [out_key] [shm_size(KB)] ([log file]))'\n",progname);
}

int
main(int argc, char *argv[])
  {
  struct Shm  *shr,*shm;
  key_t rawkey,monkey;
  /* int shmid_raw,shmid_mon; */
  uint32_w uni;
  char tb[256];
  uint8_w *ptr,*ptw,*ptr_lim,*ptr_save;
  int i,j,k,tow,c,eobsize_in,eobsize_out,eobsize_in_count,size2,
    len,tbl_p,nh,ns;
  size_t  size_shm, pl_out;
  uint32_w  size;
  unsigned long c_save;  /* 64bit ok */
#define NH 20   /* max n of packet history */
#define NS 40   /* max of time window (s) */
  struct {
    union {
      uint16_w sc[2];
      uint32_w s;
    } ss;
    uint8_w *p;
    uint32_w t;
  } tbl[NH];
  union {
    uint16_w sc[2];
    uint32_w s;
  } ss;
  /* extern int optind; */
  /* extern char *optarg; */

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;
  if(strcmp(progname,"raw_ddpd")==0) daemon_mode=1;
  nh=10;
  ns=2;

  eobsize_out=eobsize_in=0;
  while((c=getopt(argc,argv,"gBDn:t:"))!=-1)
    {
    switch(c)
      {
      case 'B':   /* eobsize */
        eobsize_out=1;
        break;
      case 'D':   /* daemon mode */
	daemon_mode = 1;
        break;
      case 'n':   /* n of packet history */
        nh=atoi(optarg);
        break;
      case 't':   /* time window (s) */
        ns=atoi(optarg);
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
	usage();
        exit(1);
      }
    }
  optind--;
  if(argc<4+optind)
    {
    usage();
    exit(1);
    }

  rawkey=atol(argv[1+optind]);
  monkey=atol(argv[2+optind]);
  size_shm=(size_t)atol(argv[3+optind])*1000;
  logfile=NULL;
  if(argc>4+optind) logfile=argv[4+optind];
  else
    {
      logfile=NULL;
      if (daemon_mode)
	syslog_mode = 1;
    }

  /* daemon mode */
  if (daemon_mode) {
    daemon_init(progname, LOG_USER, syslog_mode);
    umask(022);
  }

  if(nh>NH) nh=NH;
  if(ns>NS) ns=NS;

  /* in shared memory */
  shr = Shm_read(rawkey, "in");
  /* if((shmid_raw=shmget(rawkey,0,0))<0) err_sys("shmget in"); */
  /* if((shr=(struct Shm *)shmat(shmid_raw,(void *)0,0))== */
  /*     (struct Shm *)-1) err_sys("shmat in"); */

  /* out shared memory */
  shm = Shm_create(monkey, size_shm, "out");
  /* if((shmid_mon=shmget(monkey,size_shm,IPC_CREAT|0666))<0) */
  /*   err_sys("shmget out"); */
  /* if((shm=(struct Shm *)shmat(shmid_mon,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat out"); */

  /* initialize buffer */
  Shm_init(shm, size_shm);
  pl_out = shm->pl;
  /*   shm->p=shm->c=0; */
  /*   shm->pl=pl_out=(size_shm-sizeof(*shm))/10*9; */
  /*   shm->r=(-1); */

  /* sprintf(tb,"start in_key=%d id=%d out_key=%d id=%d size=%d", */
  /*   rawkey,shmid_raw,monkey,shmid_mon,size_shm); */
  /* write_log(tb); */

  snprintf(tb,sizeof(tb),"n_packet_hist=%d time_window=%d s",nh,ns);
  write_log(tb);

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

reset:
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;
  tow=(-1);

  size=mkuint4(ptr);
  if(mkuint4(ptr+size-4)==size) eobsize_in=1;
  else eobsize_in=0;
  eobsize_in_count=eobsize_in;
  snprintf(tb,sizeof(tb),
	   "eobsize_in=%d, eobsize_out=%d",eobsize_in,eobsize_out);
  write_log(tb);

  for(i=0;i<NH;i++) tbl[i].ss.s=0;
  tbl_p=NH-1;

  for(;;)
    {
    size=mkuint4(ptr_save=ptr);
    if(size==mkuint4(ptr+size-4)) {
      if (++eobsize_in_count == 0) eobsize_in_count = 1;
    }
    else eobsize_in_count=0;
    if(eobsize_in && eobsize_in_count==0) goto reset;
    if(!eobsize_in && eobsize_in_count>3) goto reset;
    if(eobsize_in) size2=size-4;
    else size2=size;

    ptr_lim=ptr+size2;
    c_save=shr->c;
    ptr+=4;
#if DEBUG
    for(i=0;i<6;i++) printf("%02X",ptr[i]);
    printf(" : %d R\n",size);
#endif
  /* make output data */
    uni=(uint32_w)(time(NULL)-TIME_OFFSET);
    i=uni-mkuint4(ptr);
    if(i>=0 && i<1440)   /* with tow */
      {
      if(tow!=1)
        {
	snprintf(tb,sizeof(tb),"with TOW (diff=%ds)",i);
        write_log(tb);
        if(tow==0)
          {
          write_log("reset");
          goto reset;
          }
        tow=1;
        }
      ptr+=4;
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

    /* check data */
    len=ptr_lim-ptr;
#if DEBUG
    printf("in: ");
    for(k=0;k<20;k++) printf("%02X",ptr[k]);
    printf(" len=%d\n",len);
#endif
    ss.sc[0]=len;
    ss.sc[1]=crc16(0,ptr,len);
#if DEBUG
    printf("in: size=%d crc=%d t=%d\n",ss.sc[0],ss.sc[1],uni);
#endif
    j=tbl_p;
    for(i=0;i<nh;i++) {
      if(tbl[j].ss.s>0) {
#if DEBUG
        printf("chk_hist: i=%d j=%d size=%d crc=%d t=%d\n",
          i,j,tbl[j].ss.sc[0],tbl[j].ss.sc[1],tbl[j].t);
#endif
        if(uni-tbl[j].t>ns) { /* too old */
#if DEBUG
          printf(" -> too old (%d s)\n",uni-tbl[j].t);
#endif
          i=nh;
          break;
        }
        if(ss.s==tbl[j].ss.s && memcmp(tbl[j].p,ptr,len)==0) { /* same */
#if DEBUG
          printf(" -> same packet in %d s (skipped)\n",uni-tbl[j].t);
#endif
          break;
        }
      }
      if(--j<0) j=NH-1;
    }

    if(i==nh) { /* output */
      ptw=shm->d+shm->p;
      ptw+=4;            /* fill size (4) later */
      if(tow) {
        *ptw++=uni>>24;  /* tow (H) */
        *ptw++=uni>>16;
        *ptw++=uni>>8;
        *ptw++=uni;      /* tow (L) */
      }

      if(++tbl_p==NH) tbl_p=0;
      tbl[tbl_p].ss.s=ss.s;
      tbl[tbl_p].p=ptw;
      tbl[tbl_p].t=uni;

#if DEBUG
      printf("out len=%d\n",ptr_lim-ptr);
#endif
      do {
        *ptw++=(*ptr++);
      } while(ptr<ptr_lim);

      uni=(uint32_w)(ptw-(shm->d+shm->p));
      if(eobsize_out) uni+=4;
      shm->d[shm->p  ]=uni>>24; /* size (H) */
      shm->d[shm->p+1]=uni>>16;
      shm->d[shm->p+2]=uni>>8;
      shm->d[shm->p+3]=uni;     /* size (L) */
      if(eobsize_out)
        {
        ptw[0]=uni>>24;
        ptw[1]=uni>>16;
        ptw[2]=uni>>8;
        ptw[3]=uni;
        ptw+=4;
        }
#if DEBUG
      for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+4+i]);
      printf(" : %u M\n",uni);
#endif

      shm->r=shm->p;
      if(eobsize_out && ptw>shm->d+pl_out) {shm->pl=ptw-shm->d-4;ptw=shm->d;}
      if(!eobsize_out && ptw>shm->d+shm->pl) ptw=shm->d;
      shm->p=ptw-shm->d;
      shm->c++;
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    if((ptr=ptr_save+size)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) usleep(10000);
    i=shr->c-c_save;
    if(!(i<1000000 && i>=0) || mkuint4(ptr_save)!=size)
      {
      write_log("reset");
      goto reset;
      }
    }
  }
