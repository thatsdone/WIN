/* "recvts.c"     97.9.19 modified from recvt.c      urabe */
/*                97.9.21  ch_table */
/*                98.4.23  b2d[] etc. */
/*                98.6.26  yo2000 */
/*                99.2.4   moved signal(HUP) to read_chfile() by urabe */
/*                99.4.19  byte-order-free */
/*                2001.2.20 wincpy() */
/*                2002.5.11 pre/post */
/*                2002.8.4 fixed bug in advancing SHM pointers */
/*                2003.4.4 avoid overwrite ch_table[] by memcpy(rbuf) */
/*                2005.2.20 added fclose() in read_chfile() */
/*                2010.10.13 64bit check? */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/uio.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
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

/* #define DEBUG     0 */
#define DEBUG1    0
#define DEBUG2    1
#define DEBUG3    0
#define BELL      0
#define MAXMESG   2048

static const char rcsid[] =
  "$Id: recvts.c,v 1.11.4.4.2.11.2.1 2011/01/12 16:57:06 uehira Exp $";

/* extern const int sys_nerr; */
/* extern const char *const sys_errlist[]; */
/* extern int errno; */

static uint8_w rbuf[MAXMESG],ch_table[WIN_CHMAX];
static char tb[256], *chfile;
static int n_ch,negate_channel;

char *progname,*logfile;
int syslog_mode = 0, exit_status;

/* prototypes */
static void read_chfile(void);
static WIN_bs wincpy(uint8_w *, uint8_w *, int32_w);
static int32_w get_packet(int, uint8_w *);
static void usage(void);
int main(int, char *[]);

static void
read_chfile(void)
  {
  FILE *fp;
  int i,j,k;
  char tbuf[1024];

  if(chfile != NULL)
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
        if(*tbuf=='#') continue;
        sscanf(tbuf,"%x",&k);
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

static WIN_bs
wincpy(uint8_w *ptw, uint8_w *ptr, int32_w size)
  {
#define MAX_SR 500
#define MAX_SS 4
  int ss;
  WIN_bs  n;
  WIN_sr  sr;
  uint8_w *ptr_lim;
  WIN_ch ch;
  uint32_w gs;
  /* unsigned long gh; */

  ptr_lim=ptr+size;
  n=0;
  do    /* loop for ch's */
    {
    /* gh=mkuint4(ptr); */
    /* ch=(gh>>16)&0xffff; */
    /* sr=gh&0xfff; */
    /* ss=(gh>>12)&0xf; */
    /* if(ss) gs=ss*(sr-1)+8; */
    /* else gs=(sr>>1)+8; */
    gs = win_chheader_info(ptr, &ch, &sr, &ss);
    if(sr>MAX_SR || ss>MAX_SS || ptr+gs>ptr_lim)
      {
#if DEBUG2
      sprintf(tb,
"ill ch hdr %02X%02X%02X%02X %02X%02X%02X%02X psiz=%d sr=%d ss=%d gs=%d rest=%d",
        ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
        size,sr,ss,gs,ptr_lim-ptr);
      write_log(tb);
#endif
      return (n);
      }
    if(ch_table[ch] && ptr+gs<=ptr_lim)
      {
#if DEBUG1
      fprintf(stderr,"%5d",gs);
#endif
      memcpy(ptw,ptr,gs);
      ptw+=gs;
      n+=gs;
      }
    ptr+=gs;
    } while(ptr<ptr_lim);
  return (n);
#undef MAX_SR
#undef MAX_SS
  }

static int32_w
get_packet(int fd, uint8_w *pbuf)
  {
  static int p,len;
  int32_w psize,plim;
  static uint8_w buf[4000];

  if(!(p>0 && len>0 && p<len))
    {
    len=read(fd,buf,4000);
#if DEBUG3
    printf("%d\n",len);
#endif
    p=18;
    }
  psize=(buf[p]<<8)+buf[p+1];  /* 2-byte length */
  p+=2;
#if DEBUG3
  printf("  %3d ",psize);
#endif
  plim=p+psize;
  p+=4;
#if DEBUG3
  printf("%02X ",buf[p++]);
  for(i=0;i<6;i++) printf("%02X",buf[p++]);
  printf(" ");
  for(i=0;i<20;i++) printf("%02X",buf[p++]);
  printf(" : %d/%d\n",plim,len);
#endif
  if(psize-5<MAXMESG)
    {
    memcpy(pbuf,buf+p,psize-5);
    p=plim;
    return (psize-5);
    }
  else
    {
    p=plim;
    return (0);
    }
  }

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr,
	  " usage : '%s (-m [pre(m)]) (-p [post(m)]) [shm_key] [shm_size(KB)] ([ch file]/- ([log file]))'\n",
      progname);
}

int
main(int argc, char *argv[])
  {
  key_t shm_key;
  /* int shmid; */
  uint32_w uni;
  uint8_w *ptr,tm[6],*ptr_size;
  int i,c,n,fd,pre,post;
  WIN_bs nn;
  size_t  size;
  /* extern int optind; */
  /* extern char *optarg; */
  struct Shm  *sh;
  time_t ts;

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];
  exit_status = EXIT_SUCCESS;

  pre=post=0;
  while((c=getopt(argc,argv,"m:p:"))!=-1)
    {
    switch(c)
      {
      case 'm':   /* time limit before RT in minutes */
        pre=atoi(optarg);
        if(pre<0) pre=(-pre);
        break;
      case 'p':   /* time limit after RT in minutes */
        post=atoi(optarg);
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        usage();
        exit(1);
      }
    }
  optind--;
  if(argc<3+optind)
    {
    usage();
    exit(1);
    }
  pre=(-pre*60);
  post*=60;

  shm_key=atol(argv[1+optind]);
  size=(size_t)atol(argv[2+optind])*1000;
  chfile=NULL;
  logfile=NULL;
  if(argc>3+optind)
    {
    if(strcmp("-",argv[3+optind])==0) chfile=NULL;
    else
      {
      if(argv[3+optind][0]=='-')
        {
	chfile=argv[3+optind]+1;
        negate_channel=1;
        }
      else
        {
        chfile=argv[3+optind];
        negate_channel=0;
        }
      }
    }
  if(argc>4+optind) logfile=argv[4+optind];

  if((fd=open("/dev/brhdlc0",0))<0) err_sys("open /dev/brhdlc0");

  /* shared memory */
  sh = Shm_create(shm_key, size, "start");
  /* if((shmid=shmget(shm_key,size,IPC_CREAT|0666))<0) err_sys("shmget"); */
  /* if((sh=(struct Shm *)shmat(shmid,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat"); */

  /* initialize buffer */
  Shm_init(sh, size);   /* previous code had bug????? sh->p=0 ???? */
  /*   sh->c=0; */
  /*   sh->pl=(size-sizeof(*sh))/10*9; */
  /*   sh->p=sh->r=(-1); */

  /* sprintf(tb,"start shm_key=%d id=%d size=%d",shm_key,shmid,size); */
  /* write_log(tb); */

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

  for(i=0;i<6;i++) tm[i]=(-1);
  ptr=ptr_size=sh->d;
  read_chfile();

  for(;;)
    {
    n=get_packet(fd,rbuf);
#if DEBUG3
    printf("%d ",n);
    for(i=0;i<16;i++) printf("%02X",rbuf[i]);
    printf("\n");
#endif

  /* check packet ID */
    if(n>0 && rbuf[0]==0xA1)
      {
      for(i=0;i<6;i++) if(rbuf[i+1]!=tm[i]) break;
      if(i==6)  /* same time -> just extend the sec block */
        {
        if((nn=wincpy(ptr,rbuf+7,n-7))>0) sh->c++;
        ptr+=nn;
        uni=(uint32_w)(time(NULL)-TIME_OFFSET);
        ptr_size[4]=uni>>24;  /* tow (H) */
        ptr_size[5]=uni>>16;
        ptr_size[6]=uni>>8;
        ptr_size[7]=uni;      /* tow (L) */
#if DEBUG
        if(nn>0) printf("same:nn=%d ptr=%p sh->p=%d\n",nn,ptr,sh->p);
#endif
        }
      else /* new time -> close the previous sec block */
        {
	  if((uni=(uint32_w)(ptr-ptr_size))>14) /* data exist in the previous sec block */
          {
          ptr_size[0]=uni>>24;  /* size (H) */
          ptr_size[1]=uni>>16;
          ptr_size[2]=uni>>8;
          ptr_size[3]=uni;      /* size (L) */
#if DEBUG
          printf("(%d)",time(NULL));
          for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
          printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
          sh->r=sh->p;      /* latest */
#if DEBUG
          printf("sh->r=%d size=%d\n",sh->r,mkuint4(sh->d+sh->r));
          if(mkuint4(sh->d+sh->r)<0 || mkuint4(sh->d+sh->r)>10000) getchar();
#endif
          if(ptr>sh->d+sh->pl) ptr=sh->d;
          sh->p=ptr-sh->d;
          }
        else ptr=ptr_size; /* no data in the previous sec block */
        if(!(ts=check_ts(rbuf+1,pre,post)))
          {
#if DEBUG2
          sprintf(tb,"illegal time %02X%02X%02X.%02X%02X%02X in ch %02X%02X",
            rbuf[1],rbuf[2],rbuf[3],rbuf[4],rbuf[5],rbuf[6],rbuf[7],rbuf[8]);
          write_log(tb);
#endif
          for(i=0;i<6;i++) tm[i]=(-1);
          continue;
          }
        else
          {
          ptr_size=ptr;
          ptr+=4;   /* size */
          ptr+=4;   /* time of write */
          memcpy(ptr,rbuf+1,6);
          ptr+=6;
          if((nn=wincpy(ptr,rbuf+7,n-7))>0) sh->c++;
          ptr+=nn;
          memcpy(tm,rbuf+1,6);
          uni=(uint32_w)(time(NULL)-TIME_OFFSET);
          ptr_size[4]=uni>>24;  /* tow (H) */
          ptr_size[5]=uni>>16;
          ptr_size[6]=uni>>8;
          ptr_size[7]=uni;      /* tow (L) */
#if DEBUG
          if(nn>0) printf("new:nn=%d ptr=%p sh->p=%d\n",nn,ptr,sh->p);
#endif
          }
        }
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    }
  }
