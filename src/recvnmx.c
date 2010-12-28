/* $Id: recvnmx.c,v 1.16.2.2 2010/12/28 12:55:42 uehira Exp $ */
/* "recvnmx.c"    2001.7.18-19 modified from recvt.c and nmx2raw.c  urabe */
/*                2001.8.18 */
/*                2001.10.5 workaround for hangup */
/*                2001.11.2 workaround for bundle types 32,33,34,13 */
/*                2001.11.14 strerror(), ntohs()*/
/*                2002.2.20 allow SR of only 20/100/200Hz */
/*                2002.3.3  read ch_map file on HUP */
/*                2002.3.5  eobsize on -B option */
/*                2002.7.5  Trident */
/*                2002.8.9  process re-tx packets properly */
/*                2005.4.14 NP format */
/*                2010.10.13 64bit check? */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>

#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define DIRNAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define DIRNAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

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
#include "udpu.h"

/* #define DEBUG     0 */
#define DEBUG1    0
#define DEBUG2    0
#define DEBUG3    0
#define DEBUG4    0
#define DEBUG5    0
#define BELL      0
#define MAXMESG   2048
#define NB        60    /* max n of bundles */
#define NB2       7     /* max n of bundles for NP */
#define MAXSR     200
#define BUFSIZE   ((16*NB+MAXSR+1)*4)
#define MAXCH     1024

static const char rcsid[] =
  "$Id: recvnmx.c,v 1.16.2.2 2010/12/28 12:55:42 uehira Exp $";

char *progname,*logfile;
int  syslog_mode = 0, exit_status;

static char chmapfile[1024];
static struct ip_mreq stMreq;
static uint16_w station,chmap[WIN_CHMAX];
static int use_chmap;
static char *model[32]=
  {"HRD","ORION","RM3","RM4","LYNX","CYGNUS","EUROPA","CARINA",
   "TimeServer","TRIDENT","JANUS","TAURUS",
   "012","013","014","015","016","017","018","019",
   "020","021","022","023","024","025","026","027","028","029","030","031"};

struct Nmx_Packet {
  int npformat;
  int ptype;
  time_t tim;
  struct tm *t;
  int subsec;
  int model;
  int serno;
  unsigned int oldest;
  unsigned int seq;
  double lat,lon;
  int alt;
  int ns;
  int sr;
  int ch;
  int first;
  int last;
  struct {
    int bundle_type;
    union {int8_w c[16];int8_w cc[4][4];uint8_w uc[4][4];} u;
  } b[NB];
  struct {
    unsigned int bundle_type;
    union {int8_w c[60];int8_w cc[15][4];uint8_w uc[15][4];} u;
  } b2[NB];
  int crc_calculated;
  int crc_given;
  int bpp;
};

/* prototypes */
static WIN_bs write_shm(int, int, time_t, int32_w *, struct Shm *,
			int, size_t);
static int parse_one_packet_np(uint8_w *, ssize_t, struct Nmx_Packet *);
static int parse_one_packet(uint8_w *, ssize_t, struct Nmx_Packet *);
static int bundle2fix_np(struct Nmx_Packet *, int32_w *);
static int bundle2fix(struct Nmx_Packet *, int32_w *);
static int proc_soh(struct Nmx_Packet *);
static int ch2idx(int32_w *[], struct Nmx_Packet *, int);
static void read_ch_map(void);
static void usage(void);
int main(int, char *[]);

static WIN_bs
write_shm(int ch, int sr, time_t tim, int32_w *buf, struct Shm *shm,
	  int eobsize, size_t pl)
{
  struct tm *t;
  uint8_w *ptw,*ptw_save,*ptw_save2;
  uint32_w uni;
#if DEBUG
  int i;
#endif

  if(ch<0) return (0);
  ptw=ptw_save=shm->d+shm->p;
  ptw+=4;          /* size (4) */
  uni=(uint32_w)(time(NULL)-TIME_OFFSET);
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
  ptw+=winform(buf,ptw,(WIN_sr)sr,(WIN_ch)ch);
  ptw_save2=ptw;
  if(eobsize) ptw+=4;
  uni=ptw-ptw_save;
  ptw_save[0]=uni>>24;  /* size (H) */
  ptw_save[1]=uni>>16;
  ptw_save[2]=uni>>8;
  ptw_save[3]=uni;      /* size (L) */
  if(eobsize)
    {
    ptw_save2[0]=ptw_save[0];  /* size (H) */
    ptw_save2[1]=ptw_save[1];
    ptw_save2[2]=ptw_save[2];
    ptw_save2[3]=ptw_save[3];  /* size (L) */
    }
#if DEBUG
  for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+4+i]);
  printf(" : %d M\n",uni);
#endif
  shm->r=shm->p;
  if(eobsize && ptw>shm->d+pl) {shm->pl=ptw-shm->d-4;ptw=shm->d;}
  if(!eobsize && ptw>shm->d+shm->pl) ptw=shm->d;
  shm->p=ptw-shm->d;
  shm->c++;
  return (uni);
}

static int
parse_one_packet_np(uint8_w *inbuf, ssize_t len, struct Nmx_Packet *pk)
{
#define SIGNATURE_NP 0x4E50
#define TAURUS 0xE80B
  uint16_w signature,size;
  uint16_w mdl;
  uint8_w *ptr;
  unsigned long long t,thigh,tlow;
  int x1,x2,x3,x5,ch,i,j;

  ptr=inbuf;
  signature=mkuint2(ptr);
  if(signature!=SIGNATURE_NP) return (-1);
  ptr+=2;
  size=mkuint2(ptr);
  if(size!=len) return (-1);
  ptr+=2;
  pk->seq=mkuint4(ptr);
  ptr+=8;
  thigh=mkuint4(ptr);
  ptr+=4;
  tlow=mkuint4(ptr);
  ptr+=4;
  t=tlow+(thigh<<32);
  pk->tim=t/(unsigned long long)1000000000;
  pk->t=localtime(&pk->tim);
  pk->subsec=(t%(unsigned long long)1000000000)/100000;
  pk->lat=0.000001*(double)((long)mkuint4(ptr));
  ptr+=4;
  pk->lon=0.000001*(double)((long)mkuint4(ptr));
  ptr+=4;
  pk->alt=(short)mkuint2(ptr);
  ptr+=2;
  mdl=(short)mkuint2(ptr);
  if(mdl!=TAURUS) return (-1);
  pk->model=11;
  ptr+=2;
  pk->serno=mkuint2(ptr);
  ptr+=2;
  ch=(*ptr);
  if(ch==151) pk->ch=0;
  else if(ch==153) pk->ch=1;
  else if(ch==155) pk->ch=2;
  else return (-1);
  ptr+=1;
  ptr+=2;
  size=mkuint2(ptr); /* payload size */
  pk->bpp=(size-14)/64;
  ptr+=2;
  x1=mkuint2(ptr); /* payload chid */
  ptr+=2;
  x2=mkuint2(ptr);
  ptr+=2;
  x3=mkuint2(ptr);
  ptr+=2;
  pk->ns=mkuint2(ptr);
  ptr+=2;
  x5=mkuint2(ptr);
  ptr+=2;
  pk->sr=mkuint2(ptr);
  ptr+=2;
  for(i=0;i<pk->bpp;i++)
    {
    pk->b2[i].bundle_type=mkuint4(ptr);
    ptr+=4;
    for(j=0;j<60;j++) pk->b2[i].u.c[j]=(*ptr++);
    if(i==0)
      {
      pk->first=mkuint4(pk->b2[i].u.uc[0]);
      pk->last=mkuint4(pk->b2[i].u.uc[1]);
      }
    }
/* printf("size=%d seq=%d tim=%lu.%04lu lat=%f lon=%f alt=%d\n",
size,pk->seq,pk->tim,pk->subsec,pk->lat,pk->lon,pk->alt);
printf("model=%d serno=%d ch=%d size=%d ns=%d sr=%d bpp=%d first=%d last=%d\n",
model,pk->serno,pk->ch,size,pk->ns,pk->sr,pk->bpp,pk->first,pk->last);
*/
  pk->ptype=1; /* assumed */
  return (pk->bpp);
}

static int
parse_one_packet(uint8_w *inbuf, ssize_t len, struct Nmx_Packet *pk)
{
#define SIGNATURE 0x7ABCDE0F
#define MES_DATA 1
  int srtab[32]={0,1,2,5,10,20,40,50,80,100,125,200,250,500,1000,25,120};
  uint8_w *ptr,b,*ptr_lim;
  int nb,cnt,sr,i,oldest,signature,mestype,length;

  nb=cnt=0;
  i=0;
  signature=inbuf[i+3]+(inbuf[i+2]<<8)+(inbuf[i+1]<<16)+(inbuf[i]<<24); 
  i=4;
  mestype=inbuf[i+3]+(inbuf[i+2]<<8)+(inbuf[i+1]<<16)+(inbuf[i]<<24); 
  if(signature!=SIGNATURE || mestype!=MES_DATA) return (-1);
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
        pk->model=(b>>3);
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
  return (pk->bpp=nb-1);
}

static int
bundle2fix_np(struct Nmx_Packet *pk, int32_w *dbuf)
  {
#define GPB 15 /* groups/bundle */
  int n,i,j,k,difsize[GPB],flag;
  int32_w  data, diff0; 
  int32_w diff4;
  int16_w diff2;

  n=flag=0;
/* don't use the first difference (why?) -> diff0 */
  dbuf[n++]=data=pk->first;
  for(k=0;k<pk->bpp;k++)
    {
  /*printf("bundle_type=%08X\n",pk->b2[k].bundle_type);*/
    for(i=0;i<GPB;i++)
      {
      difsize[i]=((pk->b2[k].bundle_type)>>((GPB-1-i)*2))&0x3;
      if(difsize[i]==0) continue;
      else if(difsize[i]==1)
        {
        for(j=0;j<4;j++)
          {
          if(flag==0) {diff0=pk->b2[k].u.cc[i][j];flag=1;}
          else dbuf[n++]=data=data+pk->b2[k].u.cc[i][j];
          }
        }
      else if(difsize[i]==2)
        {
        diff2=mkuint2(pk->b2[k].u.uc[i]);
        if(flag==0) {diff0=diff2;flag=1;}
        else dbuf[n++]=data=data+diff2;
        diff2=mkuint2(pk->b2[k].u.uc[i]+2);
        dbuf[n++]=data=data+diff2;
        }
      else if(difsize[i]==3)
        {
        diff4=mkuint4(pk->b2[k].u.uc[i]);
        if(flag==0) {diff0=diff4;flag=1;}
        else dbuf[n++]=data=data+diff4;
        }
      }
    }
/*printf("n=%d first=%d last=%d\n",n,dbuf[0],dbuf[n-1]);*/
  return (n);
  }

static int
bundle2fix(struct Nmx_Packet *pk, int32_w *dbuf)
{
  int n,i,j,k,difsize[4];
  int32_w data;
  int32_w diff4;
  int16_w diff2;

  n=0;
  dbuf[n++]=data=pk->first;
  for(k=0;k<pk->bpp;k++){
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
  return (n);
}

static int
proc_soh(struct Nmx_Packet *pk)
{
  struct tm *t;
  time_t tim;
  char tb[256];
  union {uint8_w c[4];float f;} u;
  int i,j;

  sprintf(tb,"%02X %02d/%02d/%02d %02d:%02d:%02d %s#%d p#%d",
    pk->ptype,pk->t->tm_year%100,pk->t->tm_mon+1,pk->t->tm_mday,pk->t->tm_hour,
    pk->t->tm_min,pk->t->tm_sec,model[pk->model],pk->serno,pk->seq);
#if SOH
  write_log(tb);
#endif
  for(i=0;i<pk->bpp;i++){ /* decode time */
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
/*        sprintf(tb+strlen(tb),"%10.5f ",u.f);*/
          sprintf(tb+strlen(tb),"%02X",u.c[0]);
          sprintf(tb+strlen(tb),"%02X",u.c[1]);
          sprintf(tb+strlen(tb),"%02X",u.c[2]);
          sprintf(tb+strlen(tb),"%02X ",u.c[3]);
        }
        break;
      case 15:
      case 39:
      case 7:
        for(j=4;j<16;j+=2)
          sprintf(tb+strlen(tb),"%02X%02X ",(uint8_w)pk->b[i].u.c[j],
            (uint8_w)pk->b[i].u.c[j+1]);
        break;
      default:
        for(j=4;j<16;j++)
          sprintf(tb+strlen(tb),"%02X",(uint8_w)pk->b[i].u.c[j]);
    }
#if SOH
    write_log(tb);
#endif
  }
  return (0);
}

static int
ch2idx(int32_w *rbuf[], struct Nmx_Packet *pk, int winch)
{
  char tb[256];
  static int m[MAXCH],s[MAXCH],c[MAXCH],n_idx;
  int i;

  for(i=0;i<n_idx;i++){
    if(pk->model==m[i] && pk->serno==s[i] && pk->ch==c[i]) break;
  }
  if(i==n_idx){
    if(n_idx==MAXCH){
      fprintf(stderr,"n_idx=%d at limit.\n",n_idx);
      return (-1);
      }
    if((rbuf[i]=(int32_w *)win_xmalloc(BUFSIZE))==NULL){
      fprintf(stderr,"malloc failed. n_idx=%d\n",n_idx);
      return (-1);
      }
    m[i]=pk->model;
    s[i]=pk->serno;
    c[i]=pk->ch;
    n_idx++;
    sprintf(tb,"registered model=%d(%s) serno=%d ch=%d idx=%d",
      pk->model,model[pk->model],pk->serno,pk->ch,i);
    if(winch>=0) sprintf(tb+strlen(tb)," winch=%04X",winch);
    write_log(tb);
    if(pk->npformat)
      {
      sprintf(tb,"NP : GPS POS = %fN %fE %dm",pk->lat,pk->lon,pk->alt);
      write_log(tb);
      }
  }
  return (i);
}

static void
read_ch_map()  
{
  char tb[256],mdl[256];
  FILE *fp;
  int i,k,serno,ch;

  /* read channel map file */
  if(*chmapfile){
    if((fp=fopen(chmapfile,"r"))!=NULL) {
      k=0;
      for(i=0;i<WIN_CHMAX;i++) chmap[i]=0xffff;
      while(fgets(tb,256,fp)) {
        if(*tb=='#') continue;
        sscanf(tb,"%s%d%x",mdl,&serno,&ch);
        if(serno>2047) {
          sprintf(tb,"S/N '%d' illegal !",serno);
          write_log(tb);
          fprintf(stderr,"%s\n",tb);
          exit(1);
        }
        for(i=0;i<32;i++) if(strcmp(mdl,model[i])==0) break;
        if(i==32) {
          sprintf(tb,"model '%s' not registered !",mdl);
          write_log(tb);
          fprintf(stderr,"%s\n",tb);
          exit(1);
        }
        chmap[(i<<11)|serno]=ch;
        k++;
      }
    fclose(fp);
    use_chmap=1;
    sprintf(tb,"%d lines read from ch_map file '%s'",k,chmapfile);
    write_log(tb);
    }
    else{
      sprintf(tb,"ch_map file '%s' not open !",chmapfile);
      write_log(tb);
      fprintf(stderr,"channel map file '%s' not open!\n",chmapfile);
      exit(1);
    }
  }
  signal(SIGHUP,(void *)read_ch_map);
}

static void
usage()
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr,
	  " usage : '%s (-c [ch_map]) (-i [interface]) (-m [mcast_group]) (-vBn) \\\n\
         (-d [fragdir]) [port] [shm_key] [shm_size(KB)] ([log file])'\n",progname);
}

int
main(int argc, char *argv[])
  {
  char tb[256];
  DIR *dir_ptr;
  FILE *fp;
  uint8_w pbuf[MAXMESG];
  int idx,seq_rbuf[MAXCH],fsize_rbuf[MAXCH];
  int32_w *rbuf[MAXCH];
  time_t tim_rbuf[MAXCH];
  /* unsigned long uni; */
  struct Nmx_Packet pk;
  int i,j,k,c,verbose,rbuf_ptr,sock,winch,eobsize,nr;
  socklen_t  fromlen;
  WIN_bs nn;
  size_t  size_shm, pl;
  ssize_t  n;
  key_t shm_key;
  /* int shmid; */
  struct Shm *shm;
  char name[100];
  char fragdir[1024]; /* directory for fragment data */
  char mcastgroup[256]; /* multicast address */
  char interface[256];  /* multicast interface */
#define MCASTGROUP  "224.0.1.1"
#define TO_PORT   32000
  struct sockaddr_in from_addr;
  uint16_t to_port;
  /* extern int optind; */
  /* extern char *optarg; */

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];
  exit_status = EXIT_SUCCESS;

  station=verbose=use_chmap=eobsize=pk.npformat=0;
  *interface=(*mcastgroup)=(*chmapfile)=(*fragdir)=0;
  while((c=getopt(argc,argv,"c:d:i:m:nvB"))!=-1)
    {
    switch(c)
      {
      case 'c':   /* channel map file */
        strcpy(chmapfile,optarg);
        break;
      case 'd':   /* fragment data directory */
        strcpy(fragdir,optarg);
        break;
      case 'i':   /* interface (ordinary IP address) which receive mcast */
        strcpy(interface,optarg);
        break;
      case 'm':   /* multicast group (multicast IP address) */
        strcpy(mcastgroup,optarg);
        break;
      case 'n':   /* NP format */
        pk.npformat=1;
        break;
      case 'v':   /* verbose */
        verbose=1;
        break;
      case 'B':   /* eobsize */
        eobsize=1;
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
  to_port=atoi(argv[1+optind]);
  shm_key=atol(argv[2+optind]);
  size_shm=atol(argv[3+optind])*1000;
  if(argc>4+optind) logfile=argv[4+optind];
  else logfile=NULL;

  sock = udp_accept4(to_port, 64);
  /* if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket"); */
  /* i=65535; */
  /* if(setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))<0) */
  /*   { */
  /*   i=50000; */
  /*   if(setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))<0) */
  /*     err_sys("SO_RCVBUF setsockopt error\n"); */
  /*   } */
  /* memset((char *)&to_addr,0,sizeof(to_addr)); */
  /* to_addr.sin_family=AF_INET; */
  /* to_addr.sin_addr.s_addr=htonl(INADDR_ANY); */
  /* to_addr.sin_port=htons(to_port); */
  /* if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0) err_sys("bind"); */

  if(*mcastgroup)
    {
    stMreq.imr_multiaddr.s_addr=inet_addr(mcastgroup);
    if(*interface) stMreq.imr_interface.s_addr=inet_addr(interface);
    else stMreq.imr_interface.s_addr=INADDR_ANY;
    if(setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&stMreq,
      sizeof(stMreq))<0) err_sys("IP_ADD_MEMBERSHIP setsockopt error\n");
    }
  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);

  /* out shared memory */
  write_log("start");
  shm = Shm_create(shm_key, size_shm, "out");
  /* if((shmid=shmget(shm_key,size_shm,IPC_CREAT|0666))<0) err_sys("shmget"); */
  /* if((shm=(struct Shm *)shmat(shmid,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat"); */

  /* sprintf(tb,"start out_key=%d id=%d size=%d",shm_key,shmid,size_shm); */
  /* write_log(tb); */

  /* initialize buffer */
  Shm_init(shm, size_shm);
  pl = shm->pl;
  /*   shm->p=shm->c=0; */
  /*   shm->pl=pl=(size_shm-sizeof(*shm))/10*9; */
  /*   shm->r=(-1); */

  read_ch_map();

  if(*fragdir)
    {
    if((dir_ptr=opendir(fragdir))==NULL) err_sys("opendir");
    else closedir(dir_ptr);
    }

  for(;;)
    {
    fromlen=sizeof(from_addr);
    n=recvfrom(sock,pbuf,MAXMESG,0,(struct sockaddr *)&from_addr,&fromlen);
    if(pk.npformat)
      {
      if(parse_one_packet_np(pbuf,n,&pk)<0) continue;
#if DEBUG5
      p=pbuf;
      printf("%d ",n);
      for(i=0;i<16;i++) printf("%02X",*p++);
      printf(" ");
      for(i=0;i<17;i++) printf("%02X",*p++);
      printf("\n");
#endif
      }
    else
      {
      if(parse_one_packet(pbuf,n,&pk)<0) continue;
#if DEBUG5
      p=pbuf;
      printf("%d ",n);
      for(i=0;i<16;i++) printf("%02X",*p++);
      printf(" ");
      for(i=0;i<17;i++) printf("%02X",*p++);
      printf(" bpp=%d\n",pk.bpp);
#endif
      }
    if(use_chmap)
      {
      if(chmap[(pk.model<<11)+pk.serno]==0xffff) winch=(-1); /* do not use the ch */
      else winch=chmap[(pk.model<<11)+pk.serno]+pk.ch;
      }
    else winch=((pk.model&0x3)<<14)+(pk.serno<<3)+pk.ch;
    if(verbose)
 printf("%s:%d>%02X %02d/%02d/%02d %02d:%02d:%02d.%04d %s#%d p#%d %dHz ch%d x0=%d %04X\n",
        inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),
        pk.ptype,pk.t->tm_year%100,pk.t->tm_mon+1,pk.t->tm_mday,pk.t->tm_hour,
        pk.t->tm_min,pk.t->tm_sec,pk.subsec,model[pk.model],pk.serno,pk.seq,
        pk.sr,pk.ch,pk.first,winch);
    if((pk.ptype&0x0f)==1 && winch>=0 && pk.sr>=10 && pk.sr<=MAXSR)
      {
         /* compressed data packet */
      idx=ch2idx(rbuf,&pk,winch);
      rbuf_ptr=(pk.subsec*pk.sr+5000)/10000;
#if DEBUG4
      printf("seq=%d(%d) expected=%d(%d)\n",pk.seq,rbuf_ptr,seq_rbuf[idx]+1,
        fsize_rbuf[idx]);
#endif
      if(pk.seq!=seq_rbuf[idx]+1)
        {
        if(*fragdir==0)
          {
          for(k=fsize_rbuf[idx];k<pk.sr;k++) rbuf[idx][k]=0;
#if DEBUG4
          printf("rest filled with zero\n");
#endif
          nn=write_shm(winch,pk.sr,tim_rbuf[idx],rbuf[idx],shm,eobsize,pl);
          }
        else
          {
          /* look for first fragment of seq_rbuf[idx]+1 */
          sprintf(name,"%s/%s%d-%d.%d.F",fragdir,model[pk.model],pk.serno,
            pk.ch,seq_rbuf[idx]+1);
          if((fp=fopen(name,"r"))==NULL)
            {
#if DEBUG4
            printf("%s not found - write rbuf to file\n",name);
#endif
            sprintf(name,"%s/%s%d-%d.%d.L",fragdir,model[pk.model],pk.serno,
              pk.ch,seq_rbuf[idx]);
            if((fp=fopen(name,"w+"))==NULL)
              {
              sprintf(tb,"file %s not open for write",name);
              write_log(tb);
              }
            else
              {
              if(fwrite(rbuf[idx],4,fsize_rbuf[idx],fp)<fsize_rbuf[idx])
                {
                sprintf(tb,"file %s write failed",name);
                write_log(tb);
                }
              fclose(fp);
              }
            }
          else /* found */
            {
            nr=fread(rbuf[idx]+fsize_rbuf[idx],4,pk.sr,fp);
            if(nr!=pk.sr-fsize_rbuf[idx] && nr!=pk.sr-fsize_rbuf[idx]+1)
              {
              sprintf(tb,"file %s size=%d inconsistent (%d)",
                name,nr,pk.sr-fsize_rbuf[idx]);
              write_log(tb);
              }
            fclose(fp);
            unlink(name);
#if DEBUG4
            printf("%s found. %d(%d) read\n",name,nr,pk.sr-fsize_rbuf[idx]);
#endif
            nn=write_shm(winch,pk.sr,tim_rbuf[idx],rbuf[idx],shm,eobsize,pl);
            }
          }
        }

      if(pk.npformat) n=bundle2fix_np(&pk,rbuf[idx]+rbuf_ptr);
      else n=bundle2fix(&pk,rbuf[idx]+rbuf_ptr);

      j=0;
      if(pk.seq!=seq_rbuf[idx]+1)
        {
        if(*fragdir==0)
          {
          for(k=0;k<rbuf_ptr;k++) rbuf[idx][k]=0;
#if DEBUG4
          printf("first part filled with zero\n");
#endif
          }
        else
          {
          /* look for last fragment of pk.seq-1 */
          sprintf(name,"%s/%s%d-%d.%d.L",fragdir,model[pk.model],pk.serno,
            pk.ch,pk.seq-1);
          if((fp=fopen(name,"r"))==NULL)
            {
#if DEBUG4
            printf("%s not found - write rbuf to file\n",name);
#endif
            sprintf(name,"%s/%s%d-%d.%d.F",fragdir,model[pk.model],pk.serno,
              pk.ch,pk.seq);
            if((fp=fopen(name,"w+"))==NULL)
              {
              sprintf(tb,"file %s not open for write",name);
              write_log(tb);
              }
            else
              {
              if(fwrite(rbuf[idx]+rbuf_ptr,4,pk.sr-rbuf_ptr,fp)<pk.sr-rbuf_ptr)
                {
                sprintf(tb,"file %s write failed",name);
                write_log(tb);
                }
              fclose(fp);
              j=1; /* skip the first sec */
              }
            }
          else /* found */
            {
            nr=fread(rbuf[idx],4,pk.sr,fp);
            if(nr!=rbuf_ptr && nr!=rbuf_ptr+1)
              {
              sprintf(tb,"file %s size=%d inconsistent (%d)",name,nr,rbuf_ptr);
              write_log(tb);
              }
            fclose(fp);
            unlink(name);
#if DEBUG4
            printf("%s found. %d(%d) read\n",name,nr,rbuf_ptr);
#endif
            }
          }
        }

#if DEBUG1
      for(ptr=rbuf[idx]+rbuf_ptr,i=0;i<n;i++) printf("%d ",*ptr++);
      printf("\n");
#endif
      for(i=j*pk.sr;i+pk.sr<rbuf_ptr+n;i+=pk.sr)
        {
        nn=write_shm(winch,pk.sr,pk.tim+j++,rbuf[idx]+i,shm,eobsize,pl);
        }
      for(k=i;k<rbuf_ptr+n;k++) rbuf[idx][k-i]=rbuf[idx][k]; 
      seq_rbuf[idx]=pk.seq;
      tim_rbuf[idx]=pk.tim;
      fsize_rbuf[idx]=rbuf_ptr+n-i;
#if DEBUG4
      printf("n=%d i=%d rbuf_ptr=%d fsize_rbuf[idx]=%d\n",
        n,i,rbuf_ptr,fsize_rbuf[idx]);
#endif
      }
    else if((pk.ptype&0x0f)==2)
      { /* status packet */
      proc_soh(&pk);
      }
    }
  }
