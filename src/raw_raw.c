/* $Id: raw_raw.c,v 1.9.4.3.2.11 2011/01/12 15:44:30 uehira Exp $ */

/* "raw_raw.c"    97.8.5 urabe */
/*                  modified from raw_100.c */
/*                  98.4.17 FreeBSD */
/*                  99.2.4  moved signal(HUP) to read_chfile() by urabe */
/*                  99.4.19 byte-order-free */
/*                  2000.3.21 c_save=shr->c; bug fixed */
/* 2000.4.24 with -g, shift 1Hz SR data by 4 bits rightward for GTA-45 LP chs */
/* 2000.4.24/2001.11.14 strerror() */
/* 2002.3.5 eobsize_in(auto), eobsize_out(-B) */
/* 2002.3.28 sleep(1) -> usleep(10000) */
/* 2002.5.2 i<1000 -> 1000000 */
/* 2005.2.20 added fclose() in read_chfile() */
/* 2005.3.7  daemon mode (Uehira) */
/* 2010.10.12 64bit (Uehira) */

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

/* #define DEBUG       0 */
#define BELL        0

static const char rcsid[] =
  "$Id: raw_raw.c,v 1.9.4.3.2.11 2011/01/12 15:44:30 uehira Exp $";

static uint8_w ch_table[WIN_CHMAX];
static char *chfile;
static int n_ch,negate_channel;
static int daemon_mode;

char *progname,*logfile;
int  syslog_mode, exit_status;

/* prototypes */
static void read_chfile(void);
static void usage(void);
int main(int, char *[]);

static void
read_chfile(void)
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
      if(negate_channel) snprintf(tbuf,sizeof(tbuf),"-%d channels",n_ch);
      else snprintf(tbuf,sizeof(tbuf),"%d channels",n_ch);
      write_log(tbuf);
      fclose(fp);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile);
#endif
      snprintf(tbuf,sizeof(tbuf),"channel list file '%s' not open",chfile);
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

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if(daemon_mode)
    fprintf(stderr,
	    " usage : '%s (-gB) [in_key] [out_key] [shm_size(KB)] \\\n\
                  (-/[ch_file]/-[ch_file] ([log file]))'\n",progname);
  else
    fprintf(stderr," usage : '%s (-gBD) [in_key] [out_key] [shm_size(KB)] \\\n\
                  (-/[ch_file]/-[ch_file] ([log file]))'\n",progname);
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
  int i,tow,c,shift45,eobsize_in,eobsize_out,
    eobsize_in_count,size2;
  WIN_sr  sr;
  size_t  size_shm, pl_out;
  uint32_w  size;
  unsigned long c_save;  /* 64bit ok */
  WIN_ch ch;
  uint32_w gs;
  int ss;
  /* extern int optind; */
  /* extern char *optarg; */

  shift45=0;
  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;
  if(strcmp(progname,"raw_rawd")==0) daemon_mode=1;

  eobsize_out=eobsize_in=0;
  while((c=getopt(argc,argv,"gBD"))!=-1)
    {
    switch(c)
      {
      case 'g':   /* shift45 mode */
        shift45=1;
        break;
      case 'B':   /* eobsize */
        eobsize_out=1;
        break;
      case 'D':   /* daemon mode */
	daemon_mode = 1;
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
  chfile=NULL;
  if(argc>4+optind)
    {
    if(strcmp("-",argv[4+optind])==0) chfile=NULL;
    else
      {
      if(argv[4+optind][0]=='-')
        {
        chfile=argv[4+optind]+1;
        negate_channel=1;
        }
      else
        {
	chfile=argv[4+optind];
        negate_channel=0;
        }
      }
    }    
  if(argc>5+optind) logfile=argv[5+optind];
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

  if(shift45)
    {
    snprintf(tb,sizeof(tb),"shift 1Hz data by 4 bits right for GTA-45");
    write_log(tb);
    }

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
    ptw=shm->d+shm->p;
    ptw+=4;               /* size (4) */
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
      /* gh=mkuint4(ptr); */
      /* ch=(gh>>16)&0xffff; */
      /* sr=gh&0xfff; */
      /* if((gh>>12)&0xf) gs=((gh>>12)&0xf)*(sr-1)+8; */
      /* else gs=(sr>>1)+8; */
      gs = win_chheader_info(ptr, &ch, &sr, &ss);
      if(ch_table[ch])
        {
#if DEBUG
        fprintf(stderr,"%5d",gs);
#endif
        /* if(shift45 && (gh&0xffff)==0x2001) */
        if(shift45 && sr==1 && ss==2)
          {
          i=mkuint4(ptr+4)>>4;
          ptr[4]=i>>24;
          ptr[5]=i>>16;
          ptr[6]=i>>8;
          ptr[7]=i;
          }
        memcpy(ptw,ptr,gs);
        ptw+=gs;
        }
      ptr+=gs;
      } while(ptr<ptr_lim);
    if(tow) i=14;
    else i=10;
    if((ptw-(shm->d+shm->p))>i)
      {
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
