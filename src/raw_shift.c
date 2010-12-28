/* $Id: raw_shift.c,v 1.2.2.2 2010/12/28 12:55:42 uehira Exp $ */

/* "raw_shift.c"    2002.4.1 - 4.1 urabe */
/*                  modified from raw_100.c */
/*                  2005.2.20 added fclose() in read_chfile() */
/*                   2010.10.12 64bit clean. eobsize_in(auto) (uehira) */

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
#include <unistd.h>
#include <errno.h>

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

/* #define DEBUG       0 */
#define BELL        0
#define MAX_SR   4095

static const char rcsid[] =
  "$Id: raw_shift.c,v 1.2.2.2 2010/12/28 12:55:42 uehira Exp $";

static uint8_w ch_table[WIN_CHMAX];
static char *chfile;
static int n_ch,negate_channel;

int syslog_mode = 0, exit_status;
char *progname,*logfile;

/* prototypes */
static void read_chfile(void);
int main(int, char *[]);

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

int
main(int argc, char *argv[])
  {
  struct Shm  *shr,*shm;
  key_t rawkey,monkey;
  /* int shmid_raw,shmid_mon; */
  uint32_w uni;
  char tb[100];
  uint8_w *ptr,*ptw,*ptr_lim,*ptr_save;
  int i,tow,rest,bits_shift;
  int eobsize_in, eobsize_in_count;
  WIN_sr  sr;
  WIN_bs  size;
  size_t  size_shm;
  WIN_ch ch1;
  WIN_sr  sr1;
  unsigned long c_save;  /* 64bit ok */
  WIN_ch ch;
  uint32_w gs,gs1;
  static int32_w buf1[MAX_SR],buf2[MAX_SR];

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];
  exit_status = EXIT_SUCCESS;
  if(argc<5)
    {
    WIN_version();
    fprintf(stderr, "%s\n", rcsid);
    fprintf(stderr,
      " usage : '%s [in_key] [out_key] [shm_size(KB)] [bits]\\\n",
      progname);
    fprintf(stderr,
      "                       (-/[ch_file]/-[ch_file]/+[ch_file] ([log file]))'\n");
    exit(1);
    }
  rawkey=atol(argv[1]);
  monkey=atol(argv[2]);
  size_shm=(size_t)atol(argv[3])*1000;
  bits_shift=atoi(argv[4]);
  chfile=NULL;
  rest=1;
  eobsize_in = 0;
  if(argc>5)
    {
    if(strcmp("-",argv[5])==0) chfile=NULL;
    else
      {
      if(argv[5][0]=='-')
        {
        chfile=argv[5]+1;
        negate_channel=1;
        }
      else if(argv[5][0]=='+')
        {
	chfile=argv[5]+1;
        negate_channel=0;
        }
      else
        {
        chfile=argv[5];
        rest=negate_channel=0;
        }
      }
    }    
  if(argc>6) logfile=argv[6];
  else logfile=NULL;

  snprintf(tb,sizeof(tb),"bit_shift=%d",bits_shift);
  write_log(tb);
    
  read_chfile();

  /* in shared memory */
  shr = Shm_read(rawkey, "in");
  /* if((shmid_raw=shmget(rawkey,0,0))<0) err_sys("shmget in"); */
  /* if((shr=(struct Shm *)shmat(shmid_raw,(void *)0,0))== */
  /*     (struct Shm *)-1) err_sys("shmat in"); */

  /* out shared memory */
  shm = Shm_create(monkey, size_shm, "out");
  /* if((shmid_mon=shmget(monkey,size_shm,IPC_CREAT|0666))<0) err_sys("shmget out"); */
  /* if((shm=(struct Shm *)shmat(shmid_mon,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat out"); */

  /* sprintf(tb,"start in_key=%d id=%d out_key=%d id=%d size=%d", */
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
  /* ptr=shr->d; */  /* verbose */
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;
  tow=(-1);

  size = mkuint4(ptr);
  if (mkuint4(ptr + size - 4) == size)
    eobsize_in = 1;
  else
    eobsize_in = 0;
  eobsize_in_count = eobsize_in;
  snprintf(tb,sizeof(tb),"eobsize_in=%d",eobsize_in);
  write_log(tb);

  for(;;)
    {
    /* ptr_lim=ptr+(size=mkuint4(ptr_save=ptr)); */
    size=mkuint4(ptr_save=ptr);
    if (mkuint4(ptr+size-4) == size) {
      if (++eobsize_in_count == 0)
	eobsize_in_count = 1;
    } else
      eobsize_in_count=0;
    if(eobsize_in && eobsize_in_count==0) goto reset;
    if(!eobsize_in && eobsize_in_count>3) goto reset;
    ptr_lim=ptr+size;
    if(eobsize_in)
      ptr_lim -= 4;

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
      gs = win_get_chhdr(ptr, &ch, &sr);
      if(ch_table[ch])
        {
#if DEBUG
        fprintf(stderr," %u",gs);
#endif
        if(sr<=MAX_SR)
          {
          win2fix(ptr,buf1,&ch1,&sr1);
          for(i=0;i<sr1;i++) buf2[i]=buf1[i]>>bits_shift;
          ptw+=(gs1=winform(buf2,ptw,sr1,ch1));
#if DEBUG
          fprintf(stderr,"->%u ",gs1);
#endif
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
    if((uni=(uint32_w)(ptw-(shm->d+shm->p)))>i)
      {
      /* uni=ptw-(shm->d+shm->p); */ /* verbose */
      shm->d[shm->p  ]=uni>>24; /* size (H) */
      shm->d[shm->p+1]=uni>>16;
      shm->d[shm->p+2]=uni>>8;
      shm->d[shm->p+3]=uni;     /* size (L) */

#if DEBUG
      if(tow) for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+8+i]);
      else for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+4+i]);
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
    if((ptr=ptr_save+size)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) usleep(100000);
    if(shr->c<c_save || mkuint4(ptr_save)!=size)
      {
      write_log("reset");
      goto reset;
      }
    }
  }
