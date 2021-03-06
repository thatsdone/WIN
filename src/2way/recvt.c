/* $Id: recvt.c,v 1.4 2001/01/08 15:05:05 urabe Exp $ */
/* "recvt.c"      4/10/93 - 6/2/93,7/2/93,1/25/94    urabe */
/*                2/3/93,5/25/94,6/16/94 */
/*                1/6/95 bug in adj_time fixed (tm[0]--) */
/*                2/17/95 "illegal time" message stopped */
/*                3/15-16/95 write_log changed */
/*                3/26/95 check_packet_no; port# */
/*                10/16/95 added processing of "host table full" */
/*                5/22/96 support merged packet with ID="0x0A" */
/*                5/22/96 widen time window to +/-30 min */
/*                5/28/96 bcopy -> memcpy */
/*                6/6/96,7/9/96 discard duplicated resent packets & fix bug */ 
/*                12/29/96 "next" */
/*                97.6.23 RCVBUF->65535 */
/*                97.9.4        & 50000 */
/*                97.12.18 channel selection by file */
/*                97.12.23 ch no. in "illegal time" & LITTLE_ENDIAN */
/*                98.4.23 b2d[] etc. */
/*                98.6.26 yo2000 */
/*                99.2.4  moved signal(HUP) to read_chfile() by urabe */
/*                99.4.19 byte-order-free */
/*                2000.3.13 >=A1 format */
/*                2000.4.24 added SS & SR check, check_time +/-60m */
/*                2000.4.24 strerror() */
/*                2000.4.25 added check_time() in >=A1 format */
/*                2000.4.26 host control, statistics, -a, -m, -p options */
/*                2000.12.25- bidirectional version */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#define DEBUG     0
#define DEBUG1    0
#define DEBUG2    1
#define DEBUG3    0
#define MAXMESG   2048
#define N_PACKET  64    /* N of old packets to be requested */  
#define N_HOST    100   /* max N of hosts */  

unsigned char rbuf[MAXMESG],ch_table[65536];
char tb[100],*progname,logfile[256],chfile[256];
int n_ch,negate_channel,hostlist[N_HOST][2],n_host;
struct {
    int host;
    int port;
    int no;
    unsigned char nos[256/8];
    unsigned int n_bytes;
    unsigned int n_packets;
    } ht[N_HOST];

mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
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

check_time(ptr,pre,post)
  char *ptr;
  int pre,post;
  {
  static int tm_prev[6],flag;
  int tm[6],rt1[6],rt2[6],i,j;

  if(!bcd_dec(tm,ptr)) return 1; /* out of range */
  if(flag && time_cmp(tm,tm_prev,5)==0) return 0;
  else flag=0;

  /* compare time with real time */
  get_time(rt1);
  for(i=0;i<5;i++) rt2[i]=rt1[i];
  for(i=0;i<pre;i++)  /* time before ? */                 
    {
    if(time_cmp(tm,rt2,5)==0)
      {
      for(j=0;j<5;j++) tm_prev[j]=tm[j];
      flag=1;
#if DEBUG1
      printf("diff=-%d m\n",i);
#endif
      return 0;
      }
    rt2[4]--;
    adj_time_m(rt2);
    }
  for(i=0;i<post;i++)  /* time after ? */
    {
    if(time_cmp(tm,rt1,5)==0)
      {
      for(j=0;j<5;j++) tm_prev[j]=tm[j];
      flag=1;
#if DEBUG1
      printf("diff=+%d m\n",i);
#endif
      return 0;
      }
    rt1[4]++;
    adj_time_m(rt1);
    }
#if DEBUG1
  printf("diff out of range (-%dm - +%dm)\n",pre,post);
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
  int i,j,k,ii;
  char tbuf[1024],host_name[80];
  struct hostent *h;
  static unsigned long ltime,ltime_p;

  n_host=0;
  if(*chfile)
    {
    if((fp=fopen(chfile,"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile);
#endif
      if(negate_channel) for(i=0;i<65536;i++) ch_table[i]=1;
      else for(i=0;i<65536;i++) ch_table[i]=0;
      i=j=ii=0;
      while(fgets(tbuf,1024,fp))
        {
        if(*tbuf=='#') continue;
        if(n_host==0 && (*tbuf=='+' || *tbuf=='-'))
          {
          if(*tbuf=='+') hostlist[ii][0]=1;   /* allow */
          else hostlist[ii][0]=(-1);          /* deny */
          if(sscanf(tbuf+1,"%s",host_name)>0) /* hostname */
            {
            if(!(h=gethostbyname(host_name)))
              {
              sprintf(tb,"host '%s' not resolved",host_name);
              write_log(logfile,tb);
              continue;
              }
            memcpy((char *)&hostlist[ii][1],h->h_addr,4);
            if(*tbuf=='+') sprintf(tb,"allow");
            else sprintf(tb,"deny");
            sprintf(tb+strlen(tb)," from host %d.%d.%d.%d",
 ((unsigned char *)&hostlist[ii][1])[0],((unsigned char *)&hostlist[ii][1])[1],
 ((unsigned char *)&hostlist[ii][1])[2],((unsigned char *)&hostlist[ii][1])[3]);
            write_log(logfile,tb);
            if(++ii==N_HOST)
              {
              n_host=ii;
              write_log(logfile,"host control table full"); 
              }
            }
          else
            {
            if(*tbuf=='+') write_log(logfile,"allow from the rest");
            else write_log(logfile,"deny from the rest");
            n_host=++ii;
            }
          continue;
          }
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
      if(ii>0 && n_host==0) n_host=ii;
      sprintf(tb,"%d host rules",n_host);
      write_log(logfile,tb);
      n_ch=j;
      if(negate_channel) sprintf(tb,"-%d channels",n_ch);
      else sprintf(tb,"%d channels",n_ch);
      write_log(logfile,tb);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile);
#endif
      sprintf(tb,"channel list file '%s' not open",chfile);
      write_log(logfile,tb);
      write_log(logfile,"end");
      exit(1);
      }
    }
  else
    {
    for(i=0;i<65536;i++) ch_table[i]=1;
    n_ch=i;
    n_host=0;
    write_log(logfile,"all channels");
    }

  time(&ltime);
  j=ltime-ltime_p;
  k=j/2;
  if(ht[0].host)
    {
    sprintf(tb,"statistics in %d s (pkts, B, pkts/s, B/s)",j);
    write_log(logfile,tb);
    }
  for(i=0;i<N_HOST;i++) /* print statistics for hosts */
    {
    if(ht[i].host==0) break;
    sprintf(tb,"  src %d.%d.%d.%d:%d   %d %d %d %d",
      ((unsigned char *)&ht[i].host)[0],((unsigned char *)&ht[i].host)[1],
      ((unsigned char *)&ht[i].host)[2],((unsigned char *)&ht[i].host)[3],
      ht[i].port,ht[i].n_packets,ht[i].n_bytes,(ht[i].n_packets+k)/j,
      (ht[i].n_bytes+k)/j);
    write_log(logfile,tb);
    ht[i].n_packets=ht[i].n_bytes=0;
    }
  ltime_p=ltime;
  signal(SIGHUP,(void *)read_chfile);
  }

check_pno(from_addr,pn,pn_f,sock,fromlen,n) /* returns -1 if duplicated */
  struct sockaddr_in *from_addr;  /* sender address */
  unsigned int pn,pn_f;           /* present and former packet Nos. */
  int sock;                       /* socket */
  int fromlen;                    /* length of from_addr */
  int n;                          /* size of packet */
  {
  int i,j;
  int host_;  /* 32-bit-long host address */
  int port_;  /* port No. */
  unsigned int pn_1;
  static unsigned int mask[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
  unsigned char pnc;

  j=(-1);
  host_=from_addr->sin_addr.s_addr;
  port_=from_addr->sin_port;
  for(i=0;i<n_host;i++)
    {
    if(hostlist[i][0]==1 &&(n_host==i+1 || hostlist[i][1]==host_)) break;
    if(hostlist[i][0]==(-1) &&(n_host==i+1 || hostlist[i][1]==host_))
      {
#if DEBUG3
      sprintf(tb,"deny packet from host %d.%d.%d.%d:%d",
        ((unsigned char *)&host_)[0],((unsigned char *)&host_)[1],
        ((unsigned char *)&host_)[2],((unsigned char *)&host_)[3],port_);
      write_log(logfile,tb);
#endif
      return -1;
      }
    }
  for(i=0;i<N_HOST;i++)
    {
    if(ht[i].host==0) break;
    if(ht[i].host==host_ && ht[i].port==port_)
      {
      j=ht[i].no;
      ht[i].no=pn;
      ht[i].nos[pn>>3]|=mask[pn&0x07]; /* set bit for the packet no */
      ht[i].n_bytes+=n;
      ht[i].n_packets++;
      break;
      }
    }
  if(i==N_HOST)   /* table is full */
    {
    for(i=0;i<N_HOST;i++) ht[i].host=0;
    write_log(logfile,"host table full - flushed.");
    i=0;
    }
  if(j<0)
    {
    ht[i].host=host_;
    ht[i].port=port_;
    ht[i].no=pn;
    ht[i].nos[pn>>3]|=mask[pn&0x07]; /* set bit for the packet no */
    sprintf(tb,"registered host %d.%d.%d.%d:%d (%d)",
      ((unsigned char *)&host_)[0],((unsigned char *)&host_)[1],
      ((unsigned char *)&host_)[2],((unsigned char *)&host_)[3],port_,i);
    write_log(logfile,tb);
    ht[i].n_bytes=n;
    ht[i].n_packets=1;
    }
  else /* check packet no */
    {
    pn_1=(j+1)&0xff;
    if(pn!=pn_1 && ((pn-pn_1)&0xff)<N_PACKET) do
      { /* send request-resend packet(s) */
      pnc=pn_1;
      sendto(sock,&pnc,1,0,from_addr,fromlen);
      sprintf(tb,"request resend %s:%d #%d",
        inet_ntoa(from_addr->sin_addr),from_addr->sin_port,pn_1);
      write_log(logfile,tb);
#if DEBUG1
      printf("<%d ",pn_1);
#endif
      ht[i].nos[pn_1>>3]&=~mask[pn_1&0x07]; /* reset bit for the packet no */
      } while((pn_1=(++pn_1&0xff))!=pn);
    }
  if(pn!=pn_f && ht[i].nos[pn_f>>3]&mask[pn_f&0x07])
    {   /* if the resent packet is duplicated, return with -1 */
#if DEBUG2
    sprintf(tb,"discard duplicated resent packet #%d for #%d",pn,pn_f);
    write_log(logfile,tb);
#endif
    return -1;
    }
  return 0;
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
    if(sr>MAX_SR || ss>MAX_SS)
      {
      sprintf(tb,"ill ch hdr %02X%02X%02X%02X %02X%02X%02X%02X",
            ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
      write_log(logfile,tb);
      return n;
      }
    if(ss) gs=ss*(sr-1)+8;
    else gs=(sr>>1)+8;
    if(ch_table[ch])
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

set_long(ptr,uni)
  unsigned char *ptr;
  unsigned long uni;
  {
  ptr[0]=uni>>24;
  ptr[1]=uni>>16;
  ptr[2]=uni>>8;
  ptr[3]=uni;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  key_t shm_key,shm_key2;
  int shmid,shmid2;
  unsigned long uni;
  unsigned char *ptr,tm[6],*ptr_size,*p;
  int i,j,k,size,size2,fromlen,n,re,nlen,sock,nn,all,pre,post,c,downgo;
  struct sockaddr_in to_addr,from_addr;
  unsigned short to_port;
  extern int optind;
  extern char *optarg;
  struct Shm {
    unsigned long p;    /* write point */
    unsigned long pl;   /* write limit */
    unsigned long r;    /* latest */
    unsigned long c;    /* counter */
    unsigned char d[1];   /* data buffer */
    } *sh,*sh2;
  struct timeval timeout;
#define USAGE \
"(-a) (-m pre) (-p post) [port] [shmkey(:shmkey2)] [shmsize(:shmsize2)(KB)] ([ctlfile]/- ([logfile]))"
  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];

  all=0;
  pre=post=30;
  timeout.tv_sec=timeout.tv_usec=0;
  while((c=getopt(argc,argv,"am:p:"))!=EOF)
    {
    switch(c)
      {
      case 'a':   /* accept >=A1 packets */
        all=1;
        break;
      case 'm':   /* time limit before RT in minutes */
        pre=atoi(optarg);
        break;
      case 'p':   /* time limit after RT in minutes */
        post=atoi(optarg);
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr," usage : '%s %s'\n",progname,USAGE);
        exit(1);
      }
    }
  optind--;
  if(argc<4+optind)
    {
    fprintf(stderr," usage : '%s %s'\n",progname,USAGE);
    exit(1);
    }

  to_port=atoi(argv[1+optind]);
  if((ptr=strchr(argv[2+optind],':'))==0)
    {
    shm_key=atoi(argv[2+optind]);
    downgo=0;
    }
  else
    {
    *ptr=0;
    shm_key=atoi(argv[2+optind]);
    shm_key2=atoi(ptr+1);
    downgo=1;
    }

  if((ptr=strchr(argv[3+optind],':'))==0)
    {
    size=atoi(argv[3+optind])*1000;
    if(downgo) size2=1000*100; /* 100 KB is the default size */
    }
  else
    {
    *ptr=0;
    size=atoi(argv[3+optind])*1000;
    size2=atoi(ptr+1)*1000;
    }

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

  /* shared memory */
  if((shmid=shmget(shm_key,size,IPC_CREAT|0666))<0) err_sys("shmget");
  if((sh=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");
  if(downgo) /* recvt creates shm for reading too */
    {
    if((shmid2=shmget(shm_key2,size2,IPC_CREAT|IPC_EXCL|0666))<0) /* existed */
      {
      if((shmid2=shmget(shm_key2,size2,IPC_CREAT|0666))<0) err_sys("shmget(2)");
      }
    else /* created */
     {
     sh2->c=0;
     sh2->pl=(size-sizeof(*sh2))/10*9;
     sh2->p=sh2->r=(-1);
     }
    if((sh2=(struct Shm *)shmat(shmid2,(char *)0,0))==(struct Shm *)-1)
      err_sys("shmat(2)");
    }

  /* initialize buffer */
  sh->c=0;
  sh->pl=(size-sizeof(*sh))/10*9;
  sh->p=sh->r=(-1);

  sprintf(tb,"start shm_key=%d id=%d size=%d",shm_key,shmid,size);
  write_log(logfile,tb);
  sprintf(tb,"start shm_key2=%d id=%d size=%d",shm_key2,shmid2,size2);
  write_log(logfile,tb);
  if(all) write_log(logfile,"accept >=A1 packets");
  sprintf(tb,"TS window -%dm - +%dm",pre,post);
  write_log(logfile,tb);

  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");
  i=65535;
  if(setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))<0)
    {
    i=50000;
    if(setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))<0)
      err_sys("SO_RCVBUF setsockopt error\n");
    }
  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  to_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  to_addr.sin_port=htons(to_port);

  if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0)
    err_sys("bind");

  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGPIPE,(void *)ctrlc);

  for(i=0;i<6;i++) tm[i]=(-1);
  ptr=ptr_size=sh->d;
  read_chfile();
  if(sh2->r==(-1)) shp2=(-1);
  else
    {
    c_save2=sh2->c;
    size2=mklong(ptr_save2=sh2->d+(shp2=sh2->r));
    }

  while(1)
    {
    k=1<<sock;
    if(select(sock+1,&k,NULL,NULL,&timeout)>0)
      {
      fromlen=sizeof(from_addr);
      n=recvfrom(sock,rbuf,MAXMESG,0,&from_addr,&fromlen);
      }
    else /* check input packets from shm2 */
      {
      if(shp2>=0) /* read next in shm2 */
        {
        if(shp2+size2>sh2->pl) shp2=0; /* advance pointer */
        else shp2+=size2;
        if(sh2->p==shp2) continue;
        i=mklong(ptr_save2);
        if(sh2->c<c_save2 || i!=size2)
          {   /* previous block has been destroyed */
          write_log(logfile,"reset");
          goto reset;
          }
        c_save2=sh2->c;
        size2=mklong(ptr_save2=ptr2=sh2->d+shp2);
        ptr_lim2=ptr2+size2;
        ptr2+=4;
        if(tow) ptr2+=4;

        }
      else if(sh2->r>=0) /* shm2 started to be written */
        {
        c_save2=sh2->c;
        size2=mklong(ptr_save2=sh2->d+(shp2=sh2->r));
        }
      else continue; /* shm2 not written yet */
      } 
#if DEBUG1
    if(rbuf[0]==rbuf[1]) printf("%d ",rbuf[0]);
    else printf("%d(%d) ",rbuf[0],rbuf[1]);
    for(i=0;i<16;i++) printf("%02X",rbuf[i]);
    printf("\n");
#endif

    if(check_pno(&from_addr,rbuf[0],rbuf[1],sock,fromlen,n)<0) continue;

    if(all)
      {
      if(rbuf[2]<0xA0) /* check packet ID */
        {
        if(check_time(rbuf+2,pre,post)==0)
          {
          ptr_size=ptr;
          ptr+=4;   /* size */
          set_long(ptr,time(0)); /* tow */
          ptr+=4;
          *ptr++=0xA1; /* packet ID */
          memcpy(ptr,rbuf+2,6); /* TS */
          ptr+=6;
          ptr+=wincpy(ptr,rbuf+8,n-8); /* data */
          if((uni=ptr-ptr_size)>15)
            {
            set_long(ptr_size,ptr-ptr_size); /* size */
#if DEBUG
            printf("(%d)",time(0));
            for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
            printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
            sh->r=sh->p;      /* latest */
            if(ptr>sh->d+sh->pl) ptr=sh->d;
            sh->p=ptr-sh->d;
            sh->c++;
            }
          else ptr=ptr_size; /* reset write pointer */
          }
#if DEBUG2
        else /* time stamp check failed */
          {
          p=rbuf;
          sprintf(tb,"ill time %02X:%02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
            p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],
            p[12],p[13],p[14],p[15],
            inet_ntoa(from_addr.sin_addr),from_addr.sin_port);
          write_log(logfile,tb);
          }
#endif
        }
      else if(rbuf[2]==0xA0) /* merged packet */
        {
        nlen=n-3;
        j=3;
        while(nlen>0)
          {
          n=(rbuf[j]<<8)+rbuf[j+1]; /* size of one-sec data */
          j+=2;
          if(check_time(rbuf+j,pre,post)==0)
            {
            ptr_size=ptr;
            ptr+=4;   /* size */
            set_long(ptr,time(0)); /* tow */
            ptr+=4;
            *ptr++=0xA1; /* packet ID */
            memcpy(ptr,rbuf+j,6); /* TS */
            ptr+=6;
            ptr+=wincpy(ptr,rbuf+j+6,n-8); /* data */
            if((uni=ptr-ptr_size)>15)
              {
              set_long(ptr_size,ptr-ptr_size); /* size */
#if DEBUG
              printf("(%d)",time(0));
              for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
              printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
              sh->r=sh->p;      /* latest */
              if(ptr>sh->d+sh->pl) ptr=sh->d;
              sh->p=ptr-sh->d;
              sh->c++;
              }
            else ptr=ptr_size; /* reset write pointer */
            }
          else /* time stamp check failed */
            {
#if DEBUG2
            p=rbuf+j;
            sprintf(tb,"ill time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
              p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],
              p[12],p[13],inet_ntoa(from_addr.sin_addr),from_addr.sin_port);
            write_log(logfile,tb);
#endif
            }
          nlen-=n;
          j+=n-2;
          }
        }
      else if(rbuf[2]<=0xA3) /* A1,A2,A3 packet */
        {

        if(check_time(rbuf+3,pre,post)==0)
          {
          ptr_size=ptr;
          ptr+=4;   /* size */
          set_long(ptr,time(0)); /* tow */
          ptr+=4;
          *ptr++=0xA1; /* packet ID */
          memcpy(ptr,rbuf+3,6); /* TS */
          ptr+=6;
          ptr+=wincpy(ptr,rbuf+9,n-9-1); /* data; the last byte is left */
          *ptr++=rbuf[n-1]; /* the last byte is station status */
          if((uni=ptr-ptr_size)>16)
            {
            set_long(ptr_size,ptr-ptr_size); /* size */
#if DEBUG
            printf("(%d)",time(0));
            for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
            printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
            sh->r=sh->p;      /* latest */
            if(ptr>sh->d+sh->pl) ptr=sh->d;
            sh->p=ptr-sh->d;
            sh->c++;
            }
          else ptr=ptr_size; /* reset write pointer */
          }
#if DEBUG2
        else
          {
          p=rbuf;
          sprintf(tb,"ill time %02X:%02X %02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
            p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],
            p[12],p[13],p[14],p[15],p[16],
            inet_ntoa(from_addr.sin_addr),from_addr.sin_port);
          write_log(logfile,tb);
          }
#endif
        }
      else /* rbuf[2]>0xA3 */
        {
        ptr_size=ptr;
        ptr+=4;   /* size */
        set_long(ptr,time(0)); /* tow */
        ptr+=4;
        memcpy(ptr,rbuf+2,n-2);
        ptr+=n-2;
        set_long(ptr_size,ptr-ptr_size); /* size */
        sh->r=sh->p;      /* latest */
        if(ptr>sh->d+sh->pl) ptr=sh->d;
        sh->p=ptr-sh->d;
        sh->c++;
        }
      } /* end of if(all) */

    else /* if(!all) */
      {
      if(rbuf[2]<0xA0) /* old style (without packet ID) */
        {
        for(i=0;i<6;i++) if(rbuf[i+2]!=tm[i]) break;
        if(i==6)  /* same time */
          {
          ptr+=wincpy(ptr,rbuf+8,n-8);
          set_long(ptr_size+4,time(0)); /* tow */
          }
        else
          {
          if((uni=ptr-ptr_size)>14)
            {
            set_long(ptr_size,uni); /* size */
#if DEBUG1
            printf("(%d)",time(0));
            for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
            printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
            }
          else ptr=ptr_size;
          if(check_time(rbuf+2,pre,post))
            {
#if DEBUG2
            p=rbuf;
            sprintf(tb,"ill time %02X:%02X %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
              p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],
              p[12],p[13],p[14],p[15],
              inet_ntoa(from_addr.sin_addr),from_addr.sin_port);
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
            memcpy(ptr,rbuf+2,6);
            ptr+=6;
            ptr+=wincpy(ptr,rbuf+8,n-8);
            memcpy(tm,rbuf+2,6);
            set_long(ptr_size+4,time(0)); /* tow */
            }
          }
        }
      else if(rbuf[2]==0xA0) /* merged packet */
        {
        nlen=n-3;
        j=3;
        while(nlen>0)
          {
          n=(rbuf[j]<<8)+rbuf[j+1];
          j+=2;
          for(i=0;i<6;i++) if(rbuf[j+i]!=tm[i]) break;
          if(i==6)  /* same time */
            {
            ptr+=wincpy(ptr,rbuf+j+6,n-8);
            set_long(ptr_size+4,time(0)); /* tow */
            }
          else
            {
            if((uni=ptr-ptr_size)>14)
              {
              set_long(ptr_size,uni); /* size */
#if DEBUG1
              printf("(%d)",time(0));
              for(i=8;i<14;i++) printf("%02X",ptr_size[i]);
              printf(" : %d > %d\n",uni,ptr_size-sh->d);
#endif
              }
            else ptr=ptr_size;
            if(check_time(rbuf+j,pre,post))
              {
#if DEBUG2
              p=rbuf+j;
              sprintf(tb,
"ill time %02X%02X%02X.%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X from %s:%d",
                p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],
                p[12],p[13],inet_ntoa(from_addr.sin_addr),from_addr.sin_port);
              write_log(logfile,tb);
#endif
              for(i=0;i<6;i++) tm[i]=(-1);
              goto next;
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
              memcpy(ptr,rbuf+j,6);
              ptr+=6;
              ptr+=wincpy(ptr,rbuf+j+6,n-8);
              memcpy(tm,rbuf+j,6);
              set_long(ptr_size+4,time(0)); /* tow */
              }
            }
next:     nlen-=n;
          j+=n-2;
          }
        }
      } /* end of if(!all) */
    }
  }
