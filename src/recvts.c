/* "recvts.c"     97.9.19 modified from recvt.c      urabe */
/*                97.9.21  ch_table */
/*                98.4.23  b2d[] etc. */
/*                98.6.26  yo2000 */
/*                99.2.4   moved signal(HUP) to read_chfile() by urabe */
/*                99.4.19  byte-order-free */
/*                2001.2.20 wincpy() */
/*                2002.5.11 pre/post */
/*                2002.8.4 fixed bug in advancing SHM pointers */

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

#include "subst_func.h"

#define DEBUG     0
#define DEBUG1    0
#define DEBUG2    1
#define DEBUG3    0
#define BELL      0
#define MAXMESG   2048

extern const int sys_nerr;
extern const char *const sys_errlist[];
extern int errno;

unsigned char rbuf[MAXMESG],ch_table[65536];
char tb[256],*progname,logfile[256],chfile[256];
int n_ch,negate_channel;

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

bcd_dec(dest,sour)
  unsigned char *sour;
  int *dest;
  {
  static int b2d[]={
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,  /* 0x00 - 0x0F */
    10,11,12,13,14,15,16,17,18,19,-1,-1,-1,-1,-1,-1,
    20,21,22,23,24,25,26,27,28,29,-1,-1,-1,-1,-1,-1,
    30,31,32,33,34,35,36,37,38,39,-1,-1,-1,-1,-1,-1,
    40,41,42,43,44,45,46,47,48,49,-1,-1,-1,-1,-1,-1,
    50,51,52,53,54,55,56,57,58,59,-1,-1,-1,-1,-1,-1,
    60,61,62,63,64,65,66,67,68,69,-1,-1,-1,-1,-1,-1,
    70,71,72,73,74,75,76,77,78,79,-1,-1,-1,-1,-1,-1,
    80,81,82,83,84,85,86,87,88,89,-1,-1,-1,-1,-1,-1,
    90,91,92,93,94,95,96,97,98,99,-1,-1,-1,-1,-1,-1,  /* 0x90 - 0x9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  int i;
  i=b2d[sour[0]];
  if(i>=0 && i<=99) dest[0]=i; else return 0;
  i=b2d[sour[1]];
  if(i>=1 && i<=12) dest[1]=i; else return 0;
  i=b2d[sour[2]];
  if(i>=1 && i<=31) dest[2]=i; else return 0;
  i=b2d[sour[3]];
  if(i>=0 && i<=23) dest[3]=i; else return 0;
  i=b2d[sour[4]];
  if(i>=0 && i<=59) dest[4]=i; else return 0;
  i=b2d[sour[5]];
  if(i>=0 && i<=60) dest[5]=i; else return 0;
  return 1;
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
  if(errno<sys_nerr) write_log(logfile,sys_errlist[errno]);
  ctrlc();
  }

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
      fprintf(stderr,"\n",k);
#endif
      n_ch=j;
      if(negate_channel) sprintf(tbuf,"-%d channels",n_ch);
      else sprintf(tbuf,"%d channels",n_ch);
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
    for(i=0;i<65536;i++) ch_table[i]=1;
    n_ch=i;
    write_log(logfile,"all channels");
    }
  signal(SIGHUP,(void *)read_chfile);
  }

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

wincpy(ptw,ptr,size)
  unsigned char *ptw,*ptr;
  int size;
  {
#define MAX_SR 500
#define MAX_SS 4
  int sr,n,ss;
  unsigned char *ptr_lim;
  unsigned short ch;
  int gs;
  unsigned long gh;
  ptr_lim=ptr+size;
  n=0;
  do    /* loop for ch's */
    {
    gh=mklong(ptr);
    ch=(gh>>16)&0xffff;
    sr=gh&0xfff;
    ss=(gh>>12)&0xf;
    if(ss) gs=ss*(sr-1)+8;
    else gs=(sr>>1)+8;
    if(sr>MAX_SR || ss>MAX_SS || ptr+gs>ptr_lim)
      {
#if DEBUG2
      sprintf(tb,
"ill ch hdr %02X%02X%02X%02X %02X%02X%02X%02X psiz=%d sr=%d ss=%d gs=%d rest=%d",
        ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],
        size,sr,ss,gs,ptr_lim-ptr);
      write_log(logfile,tb);
#endif
      return n;
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
  return n;
  }

get_packet(fd,pbuf)
  int fd;
  unsigned char *pbuf;
  {
  static int p,len;
  int i,psize,plim;
  static unsigned char buf[4000];
  if(!(p>0 && len>0 && p<len))
    {
    len=read(fd,buf,4000);
#if DEBUG3
    printf("%d\n",len);
#endif
    p=18;
    }
  psize=(buf[p]<<8)+buf[p+1];
  p+=2;
#if DEBUG3
  printf("  %3d ",psize);
#endif
  plim=p+psize;
  p+=4;
  memcpy(pbuf,buf+p,psize-5);
#if DEBUG3
  printf("%02X ",buf[p++]);
  for(i=0;i<6;i++) printf("%02X",buf[p++]);
  printf(" ");
  for(i=0;i<20;i++) printf("%02X",buf[p++]);
  printf(" : %d/%d\n",plim,len);
#endif
  p=plim;
  return psize-5;
  }

time_t check_ts(ptr,pre,post)
  char *ptr;
  int pre,post;
  {
  int diff,tm[6];
  time_t ts,rt;
  struct tm mt;
  if(!bcd_dec(tm,ptr)) return 0; /* out of range */
  memset((char *)&mt,0,sizeof(mt));
  if((mt.tm_year=tm[0])<50) mt.tm_year+=100;   
  mt.tm_mon=tm[1]-1;
  mt.tm_mday=tm[2];
  mt.tm_hour=tm[3];
  mt.tm_min=tm[4];
  mt.tm_sec=tm[5];
  mt.tm_isdst=0;
  ts=mktime(&mt);
  /* compare time with real time */
  time(&rt);
  diff=ts-rt;
  if((pre==0 || pre<diff) && (post==0 || diff<post)) return ts;
#if DEBUG1
  printf("diff %d s out of range (%ds - %ds)\n",diff,pre,post);
#endif
  return 0;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  key_t shm_key;
  int shmid;
  unsigned long uni;
  unsigned char *ptr,tm[6],*ptr_size;
  int i,j,k,c,size,n,re,fd,nn,pre,post;
  extern int optind;
  extern char *optarg;
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *sh;
  time_t ts;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  sprintf(tb,
" usage : '%s (-m [pre(m)]) (-p [post(m)]) [shm_key] [shm_size(KB)] ([ch file]/- ([log file]))'\n",
      progname);
  pre=post=0;
  while((c=getopt(argc,argv,"m:p:"))!=EOF)
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
        fprintf(stderr,"%s\n",tb);
        exit(1);
      }
    }
  optind--;
  if(argc<3+optind)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }
  pre=(-pre*60);
  post*=60;

  shm_key=atoi(argv[1+optind]);
  size=atoi(argv[2+optind])*1000;
  *chfile=(*logfile)=0;
  if(argc>3+optind)
    {
    if(strcmp("-",argv[3+optind])==0) *chfile=0;
    else
      {
      if(argv[3+optind][0]=='-')
        {
        strcpy(chfile,argv[3+optind]+1);
        negate_channel=1;
        }
      else
        {
        strcpy(chfile,argv[3+optind]);
        negate_channel=0;
        }
      }
    }
  if(argc>4+optind) strcpy(logfile,argv[4+optind]);

  /* shared memory */
  if((shmid=shmget(shm_key,size,IPC_CREAT|0666))<0) err_sys("shmget");
  if((sh=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");

  /* initialize buffer */
  sh->c=0;
  sh->pl=(size-sizeof(*sh))/10*9;
  sh->p=sh->r=(-1);

  sprintf(tb,"start shm_key=%d id=%d size=%d",shm_key,shmid,size);
  write_log(logfile,tb);

  if((fd=open("/dev/brhdlc0",0))<0) err_sys("open /dev/brhdlc0");

  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);

  for(i=0;i<6;i++) tm[i]=(-1);
  ptr=ptr_size=sh->d;
  read_chfile();

  while(1)
    {
    n=get_packet(fd,rbuf);
#if DEBUG3
    printf("%d ",n);
    for(i=0;i<16;i++) printf("%02X",rbuf[i]);
    printf("\n");
#endif

  /* check packet ID */
    if(rbuf[0]==0xA1)
      {
      for(i=0;i<6;i++) if(rbuf[i+1]!=tm[i]) break;
      if(i==6)  /* same time -> just extend the sec block */
        {
        if((nn=wincpy(ptr,rbuf+7,n-7))>0) sh->c++;
        ptr+=nn;
        uni=time(0);
        ptr_size[4]=uni>>24;  /* tow (H) */
        ptr_size[5]=uni>>16;
        ptr_size[6]=uni>>8;
        ptr_size[7]=uni;      /* tow (L) */
#if DEBUG
        if(nn>0) printf("same:nn=%d ptr=%d sh->p=%d\n",nn,ptr,sh->p);
#endif
        }
      else /* new time -> close the previous sec block */
        {
        if((uni=ptr-ptr_size)>14) /* data exist in the previous sec block */
          {
          ptr_size[0]=uni>>24;  /* size (H) */
          ptr_size[1]=uni>>16;
          ptr_size[2]=uni>>8;
          ptr_size[3]=uni;      /* size (L) */
#if DEBUG
          printf("(%d)",time(0));
          for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
          printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
          sh->r=sh->p;      /* latest */
#if DEBUG
          printf("sh->r=%d size=%d\n",sh->r,mklong(sh->d+sh->r));
          if(mklong(sh->d+sh->r)<0 || mklong(sh->d+sh->r)>10000) getchar();
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
          write_log(logfile,tb);
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
          uni=time(0);
          ptr_size[4]=uni>>24;  /* tow (H) */
          ptr_size[5]=uni>>16;
          ptr_size[6]=uni>>8;
          ptr_size[7]=uni;      /* tow (L) */
#if DEBUG
          if(nn>0) printf("new:nn=%d ptr=%d sh->p=%d\n",nn,ptr,sh->p);
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
