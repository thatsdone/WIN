/* $Id: send_raw.c,v 1.3.2.3 2001/11/19 02:23:47 uehira Exp $ */
/*
    program "send_raw/send_mon.c"   1/24/94 - 1/25/94,5/25/94 urabe
                                    6/15/94 - 6/16/94
            "sendt_raw/sendt_mon.c" 12/6/94 - 12/6/94
                                    3/15/95-3/26/95 write_log(), read_chfile()
                                    2/29/96  exit if chfile does not open
                                    5/22/96-5/25/96  merged packet
                                    5/24/96  negated channel list
                                    5/28/96  usleep by select
                                    5/31/96  bug fixed
                                    10/22/96 timezone correction
                                    97.6.23  SNDBUF->65535
                                    97.8.5   fgets/sscanf
                                    97.12.23 SNDBUF->65535/50000
                                             logging of requesting host
                                             LITTLE_ENDIAN
                                    98.4.15  eliminate interval control
                                    98.5.11  bugs in resend fixed
                                    98.6.23  shift_sec (only for -DSUNOS4)
                                    99.2.4   moved signal(HUP) to read_chfile()
                                    99.4.20  byte-order-free
                                    99.6.11  setsockopt(SO_BROADCAST)
                                    99.12.3  mktime()<-timelocal()
                                  2000.3.13  "all" mode / options -amrt
                                  2000.4.17 deleted definition of usleep() 
                                  2000.4.24 strerror()
                                  2001.8.19 send interval control
*/

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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include "subst_func.h"

#define DEBUG       0
#define DEBUG1      0
#define TEST_RESEND 0
#define MAXMESG   (1500-28)  /* max of UDP data size, +28 <= IP MTU  */
#define SR_MON      5
#define BUFNO     128
#define NWSTEP    4000  /* 4000 for Ultra1 */
#define NWLIMIT 100000

/*
#if defined(SUNOS4)
#define mktime timelocal
#endif
*/

int sock,raw,mon,tow,all,psize[BUFNO],n_ch,negate_channel;
unsigned char sbuf[BUFNO][MAXMESG+8],ch_table[65536],rbuf[MAXMESG];
     /* sbuf[BUFNO][MAXMESG+8] ; +8 for overrun by "size" and "time" */
char *progname,logfile[256],chfile[256];
static int b2d[]={
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,0,0,0,0,0,0,  /* 0x00 - 0x0F */
    10,11,12,13,14,15,16,17,18,19,0,0,0,0,0,0,
    20,21,22,23,24,25,26,27,28,29,0,0,0,0,0,0,
    30,31,32,33,34,35,36,37,38,39,0,0,0,0,0,0,
    40,41,42,43,44,45,46,47,48,49,0,0,0,0,0,0,
    50,51,52,53,54,55,56,57,58,59,0,0,0,0,0,0,
    60,61,62,63,64,65,66,67,68,69,0,0,0,0,0,0,
    70,71,72,73,74,75,76,77,78,79,0,0,0,0,0,0,
    80,81,82,83,84,85,86,87,88,89,0,0,0,0,0,0,
    90,91,92,93,94,95,96,97,98,99,0,0,0,0,0,0}; /* 0x90 - 0x9F */
static unsigned char d2b[]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99};

mklong(ptr)       
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;       
  }

mkshort(ptr)
  unsigned char *ptr;
  {
  unsigned short a;
  a=((ptr[0]<<8)&0xff00)+(ptr[1]&0xff);
  return a;
  }

shift_hour(tm_bcd,hours)
  unsigned char *tm_bcd;
  int hours;
  {
  int tm[4];
  tm[0]=b2d[tm_bcd[0]];
  tm[1]=b2d[tm_bcd[1]];
  tm[2]=b2d[tm_bcd[2]];
  tm[3]=b2d[tm_bcd[3]]+hours;
  if(tm[3]>=24)
    {
    tm[3]-=24;
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
  else if(tm[3]<=-1)
    {
    tm[3]+=24;
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
  tm_bcd[0]=d2b[tm[0]];
  tm_bcd[1]=d2b[tm[1]];
  tm_bcd[2]=d2b[tm[2]];
  tm_bcd[3]=d2b[tm[3]];
  }

shift_sec(tm_bcd,sec)
  unsigned char *tm_bcd;
  int sec;
  {
  int tm[6];
  struct tm *nt,mt;
  unsigned long ltime;
  memset((char *)&mt,0,sizeof(mt));
  if((mt.tm_year=b2d[tm_bcd[0]])<50) mt.tm_year+=100;
  mt.tm_mon=b2d[tm_bcd[1]]-1;
  mt.tm_mday=b2d[tm_bcd[2]];
  mt.tm_hour=b2d[tm_bcd[3]];
  mt.tm_min=b2d[tm_bcd[4]];
  mt.tm_sec=b2d[tm_bcd[5]];
  mt.tm_isdst=0;
  ltime=mktime(&mt)+sec;
  nt=localtime(&ltime);
  tm_bcd[0]=d2b[nt->tm_year%100];
  tm_bcd[1]=d2b[nt->tm_mon+1];
  tm_bcd[2]=d2b[nt->tm_mday];
  tm_bcd[3]=d2b[nt->tm_hour];
  tm_bcd[4]=d2b[nt->tm_min];
  tm_bcd[5]=d2b[nt->tm_sec];
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
  close(sock);
  exit(0);
  }

err_sys(ptr)
  char *ptr;
  {
  perror(ptr);
  write_log(logfile,ptr);
  if(strerror(errno)) write_log(logfile,strerror(errno));
  write_log(logfile,"end");
  close(sock);
  exit(1);
  }

get_packet(bufno,no)
  int bufno;  /* present(next) bufno */
  unsigned char no; /* packet no. to find */
  {
  int i;
  if((i=bufno-1)<0) i=BUFNO-1;
  while(i!=bufno && psize[i]>0)
    {
    if(sbuf[i][0]==no) return i;
    if(--i<0) i=BUFNO-1;
    }
  return -1;  /* not found */
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
      close(sock);
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

main(argc,argv)
  int argc;
  char *argv[];
  {
  FILE *fp;
  key_t shm_key;
  unsigned long uni;
  int i,j,k,c_save,shp,aa,bb,ii,bufno,bufno_f,fromlen,hours_shift,sec_shift,c,
    kk,nw,nwloop;
  struct sockaddr_in to_addr,from_addr;
  struct hostent *h;
  unsigned short host_port,ch;
  int size,gs,sr,re,shmid;
  unsigned long gh;
  unsigned char *ptr,*ptr1,*ptr_save,*ptr_lim,*ptw,*ptw_save,*ptw_size,
    no,no_f,host_name[100],tbuf[100];
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *shm;
  extern int optind;
  extern char *optarg;
  struct timeval timeout,tp;
  struct timezone tzp;
  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];

  raw=mon=tow=all=hours_shift=sec_shift=0;
  timeout.tv_sec=timeout.tv_usec=0;
  if(strcmp(progname,"send_raw")==0) raw=1;
  else if(strcmp(progname,"send_mon")==0) mon=1;
  else if(strcmp(progname,"sendt_raw")==0) {raw=1;tow=1;}
  else if(strcmp(progname,"sendt_mon")==0) {mon=1;tow=1;}
  else exit(1);

  while((c=getopt(argc,argv,"ah:mrs:t"))!=EOF)
    {
    switch(c)
      {
      case 'a':   /* "all" mode */
        all=tow=1;
        break;
      case 'h':   /* time to shift, in hours */
        hours_shift=atoi(optarg);
        break;
      case 'm':   /* "mon" mode */
        mon=1;
        break;
      case 'r':   /* "raw" mode */
        raw=1;
        break;
      case 's':   /* time to shift, in seconds */
        sec_shift=atoi(optarg);
        break;
      case 't':   /* "tow" mode */
        tow=1;
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,
  " usage : '%s (-amrt) (-h [h])/(-s [s]) [shmkey] [dest] [port] ([chfile]/- ([logfile]))'\n",
          progname);
        exit(1);
      }
    }
  optind--;
  if(argc<4+optind)
    {
    fprintf(stderr,
  " usage : '%s (-amrt) (-h [h])/(-s [s]) [shmkey] [dest] [port] ([chfile]/- ([logfile]))'\n",
      progname);
    exit(0);
    }

  shm_key=atoi(argv[1+optind]);
  strcpy(host_name,argv[2+optind]);
  host_port=atoi(argv[3+optind]);
  *chfile=(*logfile)=0;
  if(argc>4+optind)
    {
    if(strcmp("-",argv[4+optind])==0) *chfile=0;
    else
      {
      if(argv[4+optind][0]=='-')
        {
        strcpy(chfile,argv[4+optind]+1);
        negate_channel=1;
        }
      else
        {
        strcpy(chfile,argv[4+optind]);
        negate_channel=0;
        }
      }
    }    
  if(argc>5+optind) strcpy(logfile,argv[5+optind]);
    
  read_chfile();
  if(hours_shift!=0)
    {
    sprintf(tbuf,"hours shift = %d",hours_shift);
    write_log(logfile,tbuf);
    }
  if(sec_shift!=0)
    {
    sprintf(tbuf,"secs shift = %d",sec_shift);
    write_log(logfile,tbuf);
    }

  /* shared memory */
  if((shmid=shmget(shm_key,0,0))<0) err_sys("shmget");
  if((shm=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");

  sprintf(tbuf,"start shm_key=%d id=%d",shm_key,shmid);
  write_log(logfile,tbuf);

  /* destination host/port */
  if(!(h=gethostbyname(host_name))) err_sys("can't find host");
  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  memcpy((caddr_t)&to_addr.sin_addr,h->h_addr,h->h_length);
/*  to_addr.sin_addr.s_addr=mklong(h->h_addr);*/
  to_addr.sin_port=htons(host_port);

  /* my socket */
  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");
  i=65535;
  if(setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0)
    {
    i=50000;
    if(setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0)
      err_sys("SO_SNDBUF setsockopt error\n");
    }
  if(setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(char *)&i,sizeof(i))<0)
    err_sys("SO_BROADCAST setsockopt error\n");
  /* bind my socket to a local port */
  memset((char *)&from_addr,0,sizeof(from_addr));
  from_addr.sin_family=AF_INET;
  from_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  from_addr.sin_port=htons(0);
  if(bind(sock,(struct sockaddr *)&from_addr,sizeof(from_addr))<0)
    err_sys("bind");

  signal(SIGPIPE,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGTERM,(void *)ctrlc);
  for(i=0;i<BUFNO;i++) psize[i]=(-1);
  no=bufno=0;

reset:
  while(shm->r==(-1)) sleep(1);
  c_save=shm->c;
  size=mklong(ptr_save=shm->d+(shp=shm->r));
  ptw=ptw_save=sbuf[bufno];
  *ptw++=no;  /* packet no. */
  *ptw++=no;  /* packet no.(2) */
  if(!all) *ptw++=0xA0;
  nwloop=0;

  while(1)
    {
    if(shp+size>shm->pl) shp=0; /* advance pointer */
    else shp+=size;
    nw=0;
    while(shm->p==shp) {usleep(10000);nw++;}
    if(nw>1 && nwloop<NWLIMIT) nwloop+=NWSTEP;
    else if(nw==0 && nwloop>0) nwloop-=NWSTEP;
    i=mklong(ptr_save);
    if(shm->c<c_save || i!=size)
      {   /* previous block has been destroyed */
      write_log(logfile,"reset");
      goto reset;
      }
    c_save=shm->c;
    size=mklong(ptr_save=ptr=shm->d+shp);
    ptr_lim=ptr+size;
    ptr+=4;
    if(tow) ptr+=4;

    if(all)
      {
      memcpy(ptw,ptr,size-8);
      ptw+=size-8;
      for(kk=0;kk<nwloop;kk++);
      re=sendto(sock,ptw_save,ptw-ptw_save,0,&to_addr,sizeof(to_addr));
#if DEBUG1
      fprintf(stderr,"%5d>  ",re);
      for(k=0;k<20;k++) fprintf(stderr,"%02X",ptw_save[k]);
      fprintf(stderr,"\n");
#endif
      ptw=ptw_save=sbuf[bufno];
      no++;
      *ptw++=no;  /* packet no. */
      *ptw++=no;  /* packet no.(2) */
      continue;
      }

    ptw_size=ptw;
    ptw+=2;                            /* size */
    for(i=0;i<6;i++) *ptw++=(*ptr++);  /* time */
    if(hours_shift) shift_hour(ptw-6,hours_shift);
    if(sec_shift) shift_sec(ptw-6,sec_shift);
#if DEBUG
    for(i=2;i<8;i++) fprintf(stderr,"%02X",ptw_size[i]);
    fprintf(stderr," : %d  ",size);
#endif
    /* send data packets */
    i=j=re=0;
    while(ptr<ptr_lim)
      {
    /* obtain gs(group size) and advance ptr by gs */
      if(raw)
        {
        gh=mklong(ptr1=ptr);
        ch=(gh>>16);
        sr=gh&0xfff;
        if((gh>>12)&0xf) gs=((gh>>12)&0xf)*(sr-1)+8;
        else gs=(sr>>1)+8;
        ptr+=gs;
        }
      else /* mon */
        {
        ch=mkshort(ptr1=ptr);
        ptr+=2;
        for(ii=0;ii<SR_MON;ii++)
          {
          aa=(*(ptr++));
          bb=aa&3;
          if(bb) for(k=0;k<bb*2;k++) ptr++;
          else ptr++;
          }
        gs=ptr-ptr1;
        }
    /* add gs of data to buffer (after sending packet if it is full) */
      if(ch_table[ch] && gs+11<=MAXMESG)
        {
        if(ptw+gs-ptw_save>MAXMESG)
          {
          if(j==0) ptw-=8;
#if TEST_RESEND
          if(no%10!=9) {
#endif
          for(kk=0;kk<nwloop;kk++);
          re=sendto(sock,ptw_save,psize[bufno]=ptw-ptw_save,0,
            &to_addr,sizeof(to_addr));
#if DEBUG1
          fprintf(stderr,"%5d>  ",re);
          for(k=0;k<20;k++) fprintf(stderr,"%02X",ptw_save[k]);
          fprintf(stderr,"\n");
#endif
#if TEST_RESEND
          } else psize[bufno]=ptw-ptw_save;
#endif
#if DEBUG
          fprintf(stderr,"%5d>  ",re);
#endif
          if(++bufno==BUFNO) bufno=0;
          no++;
/* resend if requested */
          while(1)
            {
            k=1<<sock;
            if(select(sock+1,&k,NULL,NULL,&timeout)>0)
              {
              fromlen=sizeof(from_addr);
              if(recvfrom(sock,rbuf,MAXMESG,0,&from_addr,&fromlen)==1 &&
                  (bufno_f=get_packet(bufno,no_f=(*rbuf)))>=0)
                {
                memcpy(sbuf[bufno],sbuf[bufno_f],psize[bufno]=psize[bufno_f]);
                sbuf[bufno][0]=no;    /* packet no. */
                sbuf[bufno][1]=no_f;  /* old packet no. */
                re=sendto(sock,sbuf[bufno],psize[bufno],0,&to_addr,
                  sizeof(to_addr));
#if DEBUG1
                fprintf(stderr,"%5d>  ",re);
                for(k=0;k<20;k++) fprintf(stderr,"%02X",sbuf[bufno][k]);
                fprintf(stderr,"\n");
#endif
                sprintf(tbuf,"resend for %s:%d #%d as #%d, %d B",
                  inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),
                  no_f,no,re);
                write_log(logfile,tbuf);
                if(++bufno==BUFNO) bufno=0;
                no++;
                }
              }
            else break;
            }
          for(k=2;k<8;k++) sbuf[bufno][3+k]=ptw_size[k];
          ptw=ptw_save=sbuf[bufno];
          *ptw++=no; /* packet no. */
          *ptw++=no; /* packet no.(2) */
          *ptw++=0xA0;
          ptw_size=ptw;
          ptw+=2;  /* size */
          ptw+=6;  /* time */
          }
        memcpy(ptw,ptr1,gs);
        ptw+=gs;
        uni=ptw-ptw_size;
        ptw_size[0]=uni>>8;
        ptw_size[1]=uni;
        j++;
        }
      i++;
      }
#if DEBUG
    fprintf(stderr,"\007");
    fprintf(stderr," %d/%d\n",j,i); /* nch_sent/nch */
#endif
    if((uni=ptw-ptw_size)>8)
      {
      ptw_size[0]=uni>>8;
      ptw_size[1]=uni;
      }
    else ptw=ptw_size;
    }
  }
