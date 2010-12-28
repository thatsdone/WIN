/* $Id: raw_mon.c,v 1.6.2.1 2010/12/28 12:55:42 uehira Exp $ */
/* "raw_mon.c"      7/2/93,6/17/94,6/28/94    urabe */
/*                  3/17/95 write_log(), 4/17/95 MAX_SR safety */
/*                  usleep -> sleep */
/*                  1/13/97 don't abort loop when SR exceeds MAXSR(=1000Hz) */
/*                  97.8.5 fgets/sscanf */
/*                  97.9.23 FreeBSD  urabe, 97.10.3 */
/*                  98.4.13 MAX_SR : 300 -> 4096 */
/*                  98.4.14 usleep from U.M. */
/*                  99.2.4  moved signal(HUP) to read_chfile() by urabe */
/*                  99.4.19 byte-order-free */
/*                  2000.3.21 c_save=shr->c; bug fixed */
/*                  2000.4.17 deleted definition of usleep() */
/*                  2000.4.24 skip ch with>MAX_SR, strerror() */
/*                  2001.11.14 strerror()*/
/*                  2004.10.27 daemon mode (Uehira) */
/*                  2005.2.20 added fclose() in read_chfile() */
/*                  2009.12.21 64bit clean? (Uehira) */

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
#include <syslog.h>

#include "daemon_mode.h"
#include "winlib.h"

/* #define DEBUG       0 */
#define BELL        0
#define MAX_SR      HEADER_4B
/* #define SR_MON      5 */

static const char rcsid[] =
  "$Id: raw_mon.c,v 1.6.2.1 2010/12/28 12:55:42 uehira Exp $";

char *progname,*logfile;
int  daemon_mode, syslog_mode;
int  exit_status;

static int32_w buf_raw[MAX_SR],buf_mon[SR_MON][2];
static uint8_w  ch_table[WIN_CHMAX];
static char *chfile;
static int n_ch,negate_channel;

/* prototypes */
static void read_chfile(void);
int main(int argc,char *argv[]);

static void
read_chfile()
  {
  FILE *fp;
  int i,j,k;
  char tbuf[1024];

  if(chfile!=NULL)
    {
    if((fp=fopen(chfile,"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile);
#endif
      if(negate_channel) for(i=0;i<WIN_CHMAX;i++) ch_table[i]=1;
      else for(i=0;i<WIN_CHMAX;i++) ch_table[i]=0;
      i=j=0;
      while(fgets(tbuf,sizeof(tbuf),fp))
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
      fprintf(stderr,"\n");
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
    for(i=0;i<WIN_CHMAX;i++) ch_table[i]=1;
    n_ch=i;
    write_log("all channels");
    }
  signal(SIGHUP,(void *)read_chfile);
  }

int
main(int argc, char *argv[])
  {
  struct Shm  *shr,*shm;
  key_t rawkey,monkey;
  /* int shmid_raw,shmid_mon; */
  WIN_bs  uni;  /* 64bit ok */
  char tb[100];
  uint8_w   *ptr,*ptw,*ptr_lim,*ptr_save;
  int i;
  uint32_w  re;
  WIN_bs  size;
  size_t  size_shm;
  WIN_ch ch;
  WIN_sr sr;
  unsigned long c_save;  /* 64bit ok */

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;

  if(strcmp(progname,"raw_mond")==0) daemon_mode=1;

  if(argc<4)
    {
      WIN_version();
      fprintf(stderr, "%s\n", rcsid);
      fprintf(stderr,
	      " usage : '%s [raw_key] [mon_key] [shm_size(KB)] ([ch_file]/- ([log file]))'\n",
	      progname);
      exit(1);
    }
  rawkey=atol(argv[1]);
  monkey=atol(argv[2]);
  size_shm=atol(argv[3])*1000;
  chfile=NULL;
  if(argc>4)
    {
    if(strcmp("-",argv[4])==0) chfile=NULL;
    else
      {
      if(argv[4][0]=='-')
        {
        chfile=argv[4]+1;
        negate_channel=1;
        }
      else
        {
        chfile=argv[4];
        negate_channel=0;
        }
      }
    }    
  if(argc>5) logfile=argv[5];
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

  read_chfile();

  /* raw shared memory */
  shr = Shm_read(rawkey, "raw");
  /* if((shmid_raw=shmget(rawkey,0,0))<0) err_sys("shmget raw"); */
  /* if((shr=(struct Shm *)shmat(shmid_raw,(void *)0,0))== */
  /*     (struct Shm *)-1) err_sys("shmat raw"); */

  /* mon shared memory */
  shm = Shm_create(monkey, size_shm, "mon");
  /* if((shmid_mon=shmget(monkey,size_shm,IPC_CREAT|0666))<0) err_sys("shmget mon"); */
  /* if((shm=(struct Shm *)shmat(shmid_mon,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat mon"); */

  /* sprintf(tb,"start raw_key=%ld id=%d mon_key=%ld id=%d size=%ld", */
  /*   rawkey,shmid_raw,monkey,shmid_mon,size_shm); */
  /* write_log(tb); */

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

reset:
  /* initialize buffer */
  Shm_init(shm, size_shm);
  /*   shm->p=shm->c=0; */
  /*   shm->pl=(size_shm-sizeof(*shm))/10*9; */
  /*   shm->r=(-1); */

  ptr=shr->d;
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;

  for(;;)
    {
    ptr_lim=ptr+(size=mkuint4(ptr_save=ptr));
    c_save=shr->c;
    ptr+=4;
#if DEBUG
    for(i=0;i<6;i++) printf("%02X",ptr[i]);
    printf(" : %d R\n",size);
#endif
  /* make mon data */
    ptw=shm->d+shm->p;
    ptw+=4;               /* size (4) */
    for(i=0;i<6;i++) *ptw++=(*ptr++); /* YMDhms (6) */

    do    /* loop for ch's */
      {
      re=win2fix(ptr,buf_raw,&ch,&sr);
      if((re==0) || (sr>MAX_SR))
        {
        sprintf(tb,"%02X%02X%02X%02X%02X%02X%02X%02X",
          ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
        strcat(tb," ?");
        write_log(tb);
        break;
        }
      if(ch_table[ch])
        {
        *ptw++=ch>>8;
        *ptw++=ch;
        get_mon(sr,buf_raw,buf_mon);  /* get mon data from raw */
        for(i=0;i<SR_MON;i++) ptw=compress_mon(buf_mon[i],ptw);
        }
      } while((ptr+=re)<ptr_lim);

    uni=(WIN_bs)(ptw-(shm->d+shm->p));
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
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    if((ptr=ptr_lim)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) usleep(100000);
    if(shr->c<c_save || mkuint4(ptr_save)!=size)
      {
      write_log("reset");
      goto reset;
      }
    }
  }
