/* $Id: recvnmx.c,v 1.3 2001/08/10 00:39:14 urabe Exp $ */
/* "recvnmx.c"    2001.7.18-19 modified from recvt.c and nmx2raw.c  urabe */
/*                2001.8.10 */

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
#define DEBUG2    0
#define DEBUG3    0
#define BELL      0
#define MAXMESG   2048
#define NB        60    /* max n of bundles */
#define BUFSIZ    1024
#define MAXCH     1024

char *progname,logfile[256];
struct ip_mreq stMreq;
unsigned short station;
struct Shm {
  unsigned long p;    /* write point */
  unsigned long pl;   /* write limit */
  unsigned long r;    /* latest */
  unsigned long c;    /* counter */
  unsigned char d[1]; /* data buffer */
};
struct Nmx_Packet {
  int ptype;
  time_t tim;
  struct tm *t;
  int subsec;
  char *model;
  int serno;
  int oldest;
  int seq;
  int sr;
  int ch;
  int first;
  struct {
    int bundle_type;
    union {char c[16];char cc[4][4];unsigned char uc[4][4];} u;
  } b[NB];
  int crc_calculated;
  int crc_given;
};
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

/* winform.c  4/30/91,99.4.19   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */

winform(inbuf,outbuf,sr,sys_ch)
  long *inbuf;      /* input data array for one sec*/
  unsigned char *outbuf;  /* output data array for one sec */
  int sr;         /* n of data (i.e. sampling rate) */
  unsigned short sys_ch;  /* 16 bit long channel ID number */
  {
  int dmin,dmax,aa,bb,br,i,byte_leng;
  long *ptr;
  unsigned char *buf;

  /* differentiate and obtain min and max */
  ptr=inbuf;
  bb=(*ptr++);
  dmax=dmin=0;
  for(i=1;i<sr;i++)
    {
    aa=(*ptr);
    *ptr++=br=aa-bb;
    bb=aa;
    if(br>dmax) dmax=br;
    else if(br<dmin) dmin=br;
    }

  /* determine sample size */
  if(((dmin&0xfffffff8)==0xfffffff8 || (dmin&0xfffffff8)==0) &&
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0)) byte_leng=0;
  else if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0)) byte_leng=1;
  else if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0)) byte_leng=2;
  else if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0)) byte_leng=3;
  else byte_leng=4;
  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  *buf++=inbuf[0]>>24;
  *buf++=inbuf[0]>>16;
  *buf++=inbuf[0]>>8;
  *buf++=inbuf[0];
  /* second and after */
  switch(byte_leng)
    {
    case 0:
      for(i=1;i<sr-1;i+=2)
        *buf++=(inbuf[i]<<4)|(inbuf[i+1]&0xf);
      if(i==sr-1) *buf++=(inbuf[i]<<4);
      break;
    case 1:
      for(i=1;i<sr;i++)
        *buf++=inbuf[i];
      break;
    case 2:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>24;
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    }
  return (int)(buf-outbuf);
  }

int write_shm(int ch,int sr,time_t tim,int *buf,struct Shm *shm)
{
  struct tm *t;
  unsigned char *ptw,*ptw_save;
  unsigned long uni;
  int i;

  if(ch<0) return 0;
  ptw=ptw_save=shm->d+shm->p;
  ptw+=4;          /* size (4) */
  uni=time(0);
  *ptw++=uni>>24;  /* tow (H) */
  *ptw++=uni>>16;
  *ptw++=uni>>8;
  *ptw++=uni;      /* tow (L) */
  t=localtime(&tim);
  *ptw++=d2b[t->tm_year%100];
  *ptw++=d2b[t->tm_mon+1];
  *ptw++=d2b[t->tm_mday];
  *ptw++=d2b[t->tm_hour];
  *ptw++=d2b[t->tm_min];
  *ptw++=d2b[t->tm_sec];
  ptw+=winform(buf,ptw,sr,ch);
  uni=ptw-(shm->d+shm->p);
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
  return ptw-ptw_save;
}

parse_one_packet(unsigned char *inbuf,int len,struct Nmx_Packet *pk)
{
#define SIGNATURE 0x7ABCDE0F
#define MES_DATA 1
  int srtab[32]={0,1,2,5,10,20,40,50,80,100,125,200,250,500,1000,25,120};
  char *model[32]={"HRD","ORION","RM3","RM4","LYNX","CYGNUS","CALLISTO",
    "CARINA"};
  unsigned char *ptr,b,*ptr_lim;
  int nb,cnt,sr,i,oldest,signature,mestype,length;
  nb=cnt=0;
  i=0;
  signature=inbuf[i+3]+(inbuf[i+2]<<8)+(inbuf[i+1]<<16)+(inbuf[i]<<24); 
  i=4;
  mestype=inbuf[i+3]+(inbuf[i+2]<<8)+(inbuf[i+1]<<16)+(inbuf[i]<<24); 
  if(signature!=SIGNATURE || mestype!=MES_DATA) return -1;
  i=8;
  length=inbuf[i+3]+(inbuf[i+2]<<8)+(inbuf[i+1]<<16)+(inbuf[i]<<24); 
  i=12;
  oldest=inbuf[i]+(inbuf[i+1]<<8)+(inbuf[i+2]<<16)+(inbuf[i+3]<<24); 
  i+=4;
  ptr=inbuf+i;
  ptr_lim=inbuf+len;
  while(ptr<ptr_lim) {
    b=(*ptr++);
    if(nb==0){
      if(cnt==0) pk->ptype=b;
      else if(cnt==1) pk->tim=b;
      else if(cnt==2) pk->tim+=(b<<8);
      else if(cnt==3) pk->tim+=(b<<16);
      else if(cnt==4) {pk->tim+=(b<<24);pk->t=localtime(&pk->tim);}
      else if(cnt==5) pk->subsec=b;
      else if(cnt==6) pk->subsec+=(b<<8);
      else if(cnt==7) pk->serno=b;
      else if(cnt==8) {
        pk->model=model[(b>>3)];
        if(pk->model==NULL) pk->model="?";
        pk->serno+=((b&0x7)<<8);
      }
      else if(cnt==9) pk->seq=b;
      else if(cnt==10) pk->seq+=(b<<8);
      else if(cnt==11) pk->seq+=(b<<16);
      else if(cnt==12) pk->seq+=(b<<24);
      else if((pk->ptype&0x0f)==1){ /* compressed data packet */
        if(cnt==13) {sr=(b>>3);pk->sr=srtab[sr];pk->ch=(b&0x7);}
        else if(cnt==14) pk->first=(b<<8);
        else if(cnt==15) pk->first+=(b<<16);
        else if(cnt==16) {pk->first+=(b<<24);pk->first>>=8;}
      }
    }
    else { /* second and after bundle */
      if(cnt==0) pk->b[nb-1].bundle_type=b;
      else pk->b[nb-1].u.c[cnt-1]=b;
    }
    if(cnt==16){
      nb++;
      cnt=0;
    }
    else cnt++;
#if DEBUG1
  printf("(%d %d %02X)",nb,cnt,b);
#endif
  }
  return nb-1;
}

int bundle2fix(int bpp,struct Nmx_Packet *pk,int *dbuf)
{
  int n,i,j,k,difsize[4],data;
  long diff4;
  short diff2;
  n=0;
  dbuf[n++]=data=pk->first;
  for(k=0;k<bpp;k++){
    difsize[0]=((pk->b[k].bundle_type)>>6)&0x3;
    difsize[1]=((pk->b[k].bundle_type)>>4)&0x3;
    difsize[2]=((pk->b[k].bundle_type)>>2)&0x3;
    difsize[3]=(pk->b[k].bundle_type)&0x3;
    for(i=0;i<4;i++){ /* datasets #1 - #4 */
      if(difsize[i]==1){
        for(j=0;j<4;j++) dbuf[n++]=data=data+pk->b[k].u.cc[i][j];
      }
      else if(difsize[i]==2){
        diff2=pk->b[k].u.uc[i][0]+(pk->b[k].u.uc[i][1]<<8);
        dbuf[n++]=data=data+diff2;
        diff2=pk->b[k].u.uc[i][2]+(pk->b[k].u.uc[i][3]<<8);
        dbuf[n++]=data=data+diff2;
      }
      else if(difsize[i]==3){
        diff4=pk->b[k].u.uc[i][0]+(pk->b[k].u.uc[i][1]<<8)
          +(pk->b[k].u.uc[i][2]<<16)+(pk->b[k].u.uc[i][3]<<24);
        dbuf[n++]=data=data+diff4;
      }
    }
  }
  return n;
}

proc_soh(int bpp,struct Nmx_Packet *pk)
{
  struct tm *t;
  time_t tim;
  unsigned char tb[256],*ptr;
  union {unsigned char c[4];float f;} u;
  int i,j;
  sprintf(tb,"%02X %02d/%02d/%02d %02d:%02d:%02d %s#%d p#%d",
    pk->ptype,pk->t->tm_year%100,pk->t->tm_mon+1,pk->t->tm_mday,pk->t->tm_hour,
    pk->t->tm_min,pk->t->tm_sec,pk->model,pk->serno,pk->seq);
#if SOH
  write_log(logfile,tb);
#endif
  for(i=0;i<bpp;i++){ /* decode time */
    tim=pk->b[i].u.uc[0][0]+(pk->b[i].u.uc[0][1]<<8)+
      (pk->b[i].u.uc[0][2]<<16)+(pk->b[i].u.uc[0][3]<<24);
    t=localtime(&tim);
    sprintf(tb," %02d %02d/%02d/%02d %02d:%02d:%02d ",pk->b[i].bundle_type,
      t->tm_year%100,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
    switch(pk->b[i].bundle_type){
      case 32:
      case 33:
      case 34:
      case 13:
        for(j=1;j<4;j++){
          u.c[0]=pk->b[i].u.uc[j][3]; u.c[1]=pk->b[i].u.uc[j][2];
          u.c[2]=pk->b[i].u.uc[j][1]; u.c[3]=pk->b[i].u.uc[j][0];
          sprintf(tb+strlen(tb),"%10.5f ",u.f);
        }
        break;
      case 15:
      case 39:
      case 7:
        for(j=4;j<16;j+=2)
          sprintf(tb+strlen(tb),"%02X%02X ",(unsigned char)pk->b[i].u.c[j],
            (unsigned char)pk->b[i].u.c[j+1]);
        break;
      default:
        for(j=4;j<16;j++)
          sprintf(tb+strlen(tb),"%02X",(unsigned char)pk->b[i].u.c[j]);
    }
#if SOH
    write_log(logfile,tb);
#endif
  }
  return 0;
}

ch2idx(rbuf,serno,ch)
  int *rbuf[];
  int serno,ch;
{
  char tb[256];
  static int s[MAXCH],c[MAXCH],n_idx;
  int i;
  for(i=0;i<n_idx;i++){
    if(serno==s[i] && ch==c[i]) break;
  }
  if(i==n_idx){
    if(n_idx==MAXCH){
      fprintf(stderr,"n_idx=%d at limit.\n",n_idx);
      return -1;
      }
    if((rbuf[i]=(int *)malloc(BUFSIZ))==NULL){
      fprintf(stderr,"malloc failed. n_idx=%d\n",n_idx);
      return -1;
      }
    s[i]=serno;
    c[i]=ch;
    n_idx++;
    sprintf(tb,"registered serno=%d ch=%d idx=%d",serno,ch,i);
    write_log(logfile,tb);
  }
  return i;
}

main(argc,argv)
  int argc;
  char *argv[];
  {
  char tb[256];
  FILE *fp;
  unsigned char pbuf[MAXMESG];
  int *rbuf[MAXCH],*ptr,idx;
  unsigned long uni;
  struct Nmx_Packet pk;
  int i,j,k,size_shm,n,fd,baud,c,bpp,oldest,nn,verbose,rbuf_ptr,sock,fromlen,
    winch,use_chmap;
  static unsigned short chmap[MAXCH];
  key_t shm_key;
  int shmid;
  struct Shm *shm;
  char mcastgroup[256]; /* multicast address */
  char interface[256]; /* multicast interface */
  char chmapfile[1024]; /* channel map file */
#define MCASTGROUP  "224.0.1.1"
#define TO_PORT   32000
  unsigned char *p;
  struct sockaddr_in to_addr,from_addr;
  unsigned short to_port;
  extern int optind;
  extern char *optarg;

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  sprintf(tb,
    " usage : '%s (-c ch_map) (-i interface) (-m mcast_group) (-v) [port] [shm_key] [shm_size(KB)] ([log file])'",
    progname);

  bpp=19; /* Lynx default */
  station=verbose=use_chmap=0;
  *interface=(*mcastgroup)=(*chmapfile)=0;
  while((c=getopt(argc,argv,"c:i:m:v"))!=EOF) {
    switch(c) {
      case 'c':   /* channel map file */
        strcpy(chmapfile,optarg);
        break;
      case 'i':   /* interface (ordinary IP address) which receive mcast */
        strcpy(interface,optarg);
        break;
      case 'm':   /* multicast group (multicast IP address) */
        strcpy(mcastgroup,optarg);
        break;
      case 'v':   /* verbose */
        verbose=1;
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
        exit(1);
    }
  }
  optind--;
  if(argc<4+optind) {
    fprintf(stderr,"%s\n",tb);
    exit(1);
  }
  to_port=atoi(argv[1+optind]);
  shm_key=atoi(argv[2+optind]);
  size_shm=atoi(argv[3+optind])*1000;
  *logfile=0;
  if(argc>4+optind) strcpy(logfile,argv[4+optind]);

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
  if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0) err_sys("bind");

  if(*mcastgroup){
    stMreq.imr_multiaddr.s_addr=inet_addr(mcastgroup);
    if(*interface) stMreq.imr_interface.s_addr=inet_addr(interface);
    else stMreq.imr_interface.s_addr=INADDR_ANY;
    if(setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&stMreq,
      sizeof(stMreq))<0) err_sys("IP_ADD_MEMBERSHIP setsockopt error\n");
  }
  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGPIPE,(void *)ctrlc);

  /* out shared memory */
  if((shmid=shmget(shm_key,size_shm,IPC_CREAT|0666))<0) err_sys("shmget");
  if((shm=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");

  sprintf(tb,"start out_key=%d id=%d size=%d",shm_key,shmid,size_shm);
  write_log(logfile,tb);

  /* initialize buffer */
  shm->p=shm->c=0;
  shm->pl=(size_shm-sizeof(*shm))/10*9;
  shm->r=(-1);

  /* read channel map file */
  if(*chmapfile){
    if((fp=fopen(chmapfile,"r"))!=NULL) {
      for(i=0;i<MAXCH;i++) chmap[i]=0xffff;
      while(fgets(tb,256,fp)) {   
        if(*tb=='#') continue;
        sscanf(tb,"%d%x",&i,&j);
        chmap[i]=j; 
      } 
    fclose(fp);
    use_chmap=1;
    }
  }

  while(1){
    fromlen=sizeof(from_addr);
    n=recvfrom(sock,pbuf,MAXMESG,0,&from_addr,&fromlen);
    if((bpp=parse_one_packet(pbuf,n,&pk))<0) continue;
#if DEBUG1
    p=pbuf;
    printf("%d ",n);
    for(i=0;i<16;i++) printf("%02X",*p++);
    printf(" ");
    for(i=0;i<17;i++) printf("%02X",*p++);
    printf(" bpp=%d\n",bpp);
#endif
    if(use_chmap) {
      if(chmap[pk.serno]==0xffff) winch=(-1); /* do not use the ch */
      else winch=chmap[pk.serno]+pk.ch;
    }
    else winch=(pk.serno<<5)+pk.ch;
    if(verbose)
 printf("%s:%d>%02X %02d/%02d/%02d %02d:%02d:%02d.%04d %s#%d p#%d %dHz ch%d x0=%d %04X\n",
        inet_ntoa(from_addr.sin_addr),from_addr.sin_port,
        pk.ptype,pk.t->tm_year%100,pk.t->tm_mon+1,pk.t->tm_mday,pk.t->tm_hour,
        pk.t->tm_min,pk.t->tm_sec,pk.subsec,pk.model,pk.serno,pk.seq,
        pk.sr,pk.ch,pk.first,winch);
    if((pk.ptype&0x0f)==1){ /* compressed data packet */
      idx=ch2idx(rbuf,pk.serno,pk.ch);
      rbuf_ptr=(pk.subsec*pk.sr+5000)/10000;
      n=bundle2fix(bpp,&pk,rbuf[idx]+rbuf_ptr);
#if DEBUG1
      for(ptr=rbuf[idx]+rbuf_ptr,i=0;i<n;i++) printf("%d ",*ptr++);
      printf("\n");
#endif
      for(i=0,j=0;i+pk.sr<rbuf_ptr+n;i+=pk.sr){
        nn=write_shm(winch,pk.sr,pk.tim+j++,rbuf[idx]+i,shm);
      }
      for(k=i;k<rbuf_ptr+n;k++) rbuf[idx][k-i]=rbuf[idx][k]; 
    }
    else if((pk.ptype&0x0f)==2){ /* status packet */
      proc_soh(bpp,&pk);
    }
  }
}
