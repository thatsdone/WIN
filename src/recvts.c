/* $Id: recvts.c,v 1.2 2000/04/30 10:05:23 urabe Exp $ */
/* "recvts.c"     97.9.19 modified from recvt.c      urabe */
/*                97.9.21  ch_table */
/*                98.4.23  b2d[] etc. */
/*                98.6.26  yo2000 */
/*                99.2.4   moved signal(HUP) to read_chfile() by urabe */
/*                99.4.19  byte-order-free */
/*                2000.4.24 strerror() */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>

#define DEBUG     0
#define DEBUG1    0
#define DEBUG2    1
#define DEBUG3    0
#define BELL      0
#define MAXMESG   2048

unsigned char rbuf[MAXMESG],ch_table[65536];
char tb[100],*progname,logfile[256],chfile[256];
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

adj_time_m(tm)
  int *tm;
  {
  if(tm[4]==60)
    {
    tm[4]=0;
    if(++tm[3]==24)
      {
      tm[3]=0;
      tm[2]++;
      switch(tm[1])
        {
        case 2:
          if(tm[0]%4==0)
            {
            if(tm[2]==30)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
            }
          else
            {
            if(tm[2]==29)
              {
              tm[2]=1;
              tm[1]++;
              }
            break;
            }
        case 4:
        case 6:
        case 9:
        case 11:
          if(tm[2]==31)
            {
            tm[2]=1;
            tm[1]++;
            }
          break;
        default:
          if(tm[2]==32)
            {
            tm[2]=1;
            tm[1]++;
            }
          break;
        }
      if(tm[1]==13)
        {
        tm[1]=1;
        if(++tm[0]==100) tm[0]=0;
        }
      }
    }
  else if(tm[4]==-1)
    {
    tm[4]=59;
    if(--tm[3]==-1)
      {
      tm[3]=23;
      if(--tm[2]==0)
        {
        switch(--tm[1])
          {
          case 2:
            if(tm[0]%4==0)
              tm[2]=29;else tm[2]=28;
            break;
          case 4:
          case 6:
          case 9:
          case 11:
            tm[2]=30;
            break;
          default:
            tm[2]=31;
            break;
          }
        if(tm[1]==0)
          {
          tm[1]=12;
          if(--tm[0]==-1) tm[0]=99;
          }
        }
      }
    }
  }

time_cmp(t1,t2,i)
  int *t1,*t2,i;  
  {
  int cntr;
  cntr=0;
  if(t1[cntr]<70 && t2[cntr]>70) return 1;
  if(t1[cntr]>70 && t2[cntr]<70) return -1;
  for(;cntr<i;cntr++)
    {
    if(t1[cntr]>t2[cntr]) return 1;
    if(t1[cntr]<t2[cntr]) return -1;
    } 
  return 0;  
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

check_time(ptr)
  char *ptr;
  {
  static int tm_prev[6],flag;
  int tm[6],rt1[6],rt2[6],i,j;

  if(!bcd_dec(tm,ptr)) return 1; /* out of range */
  if(flag && time_cmp(tm,tm_prev,5)==0) return 0;
  else flag=0;

  /* compare time with real time */
  get_time(rt1);
  for(i=0;i<5;i++) rt2[i]=rt1[i];
  for(i=0;i<30;i++)  /* within 30 minutes ? */
    {
    if(time_cmp(tm,rt1,5)==0 || time_cmp(tm,rt2,5)==0)
      {
      for(j=0;j<5;j++) tm_prev[j]=tm[j];
      flag=1;
#if DEBUG1
      printf("diff=%d m\n",i);
#endif
      return 0;
      }
    rt1[4]++;
    adj_time_m(rt1);
    rt2[4]--;
    adj_time_m(rt2);
    }
#if DEBUG1
  printf("diff>%d m\n",i);
#endif
  return 1; /* illegal time */
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
  if(strerror(errno)) write_log(strerror(errno));
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

main(argc,argv)
  int argc;
  char *argv[];
  {
  key_t shm_key;
  int shmid;
  unsigned long uni;
  unsigned char *ptr,tm[6],*ptr_size;
  int i,j,k,size,n,re,fd;
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *sh;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  if(argc<3)
    {
    fprintf(stderr,
      " usage : '%s [shm_key] [shm_size(KB)] ([ch file]/- ([log file]))'\n",
      progname);
    exit(1);
    }
  shm_key=atoi(argv[1]);
  size=atoi(argv[2])*1000;
  *chfile=(*logfile)=0;
  if(argc>3)
    {
    if(strcmp("-",argv[3])==0) *chfile=0;
    else
      {
      if(argv[3][0]=='-')
        {
        strcpy(chfile,argv[3]+1);
        negate_channel=1;
        }
      else
        {
        strcpy(chfile,argv[3]);
        negate_channel=0;
        }
      }
    }
  if(argc>4) strcpy(logfile,argv[4]);

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
    if(rbuf[0]==0xA1 && ch_table[(rbuf[7]<<8)+rbuf[8]]==1)
      {
      for(i=0;i<6;i++) if(rbuf[i+1]!=tm[i]) break;
      if(i==6)  /* same time */
        {
        memcpy(ptr,rbuf+7,n-7);
        ptr+=n-7;
        uni=time(0);
        ptr_size[4]=uni>>24;  /* tow (H) */
        ptr_size[5]=uni>>16;
        ptr_size[6]=uni>>8;
        ptr_size[7]=uni;      /* tow (L) */
        }
      else
        {
        uni=ptr-ptr_size;
        ptr_size[0]=uni>>24;  /* size (H) */
        ptr_size[1]=uni>>16;
        ptr_size[2]=uni>>8;
        ptr_size[3]=uni;      /* size (L) */
#if DEBUG
        printf("(%d)",time(0));
        for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
        printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
        if(check_time(rbuf+1))
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
          sh->r=sh->p;      /* latest */
          if(ptr>sh->d+sh->pl) ptr=sh->d;
          sh->p=ptr-sh->d;
          sh->c++;
          ptr_size=ptr;
          ptr+=4;   /* size */
          ptr+=4;   /* time of write */
          memcpy(ptr,rbuf+1,n-1);
          ptr+=n-1;
          memcpy(tm,rbuf+1,6);
          uni=time(0);
          ptr_size[4]=uni>>24;  /* tow (H) */
          ptr_size[5]=uni>>16;
          ptr_size[6]=uni>>8;
          ptr_size[7]=uni;      /* tow (L) */
          }
        }
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    }
  }
