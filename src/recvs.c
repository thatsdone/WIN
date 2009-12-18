/* $Id: recvs.c,v 1.6.2.1.2.6 2009/12/18 11:33:44 uehira Exp $ */
/* "recvs.c"    receive sync frames      2000.3.14       urabe */
/* 2000.3.21 */
/* 2000.4.17 */
/* 2000.4.24/2001.11.14 strerror() */
/* 2001.11.14 ntohs() */
/*                2005.6.24 don't change optarg's content (-o) */

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

#include <unistd.h>
#if HAVE_STROPTS_H
#include <stropts.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#if HAVE_SYS_SER_SYNC_H
#include <sys/ser_sync.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <errno.h>
#if AURORA
#include "/opt/AURAacs/syncuser.h"
#endif

#include "winlib.h"

#define DEBUG     0
#define DEBUG1    0
#define DEBUG2    0   /* report illegal time-stamp - normally "1" */
#define DEBUG3    0   /* HDLC input data debugging */
#define BELL      0
#define TEST_RESEND 0
#define MAXMESG   2048

unsigned char rbuf[MAXMESG];
char tb[256];
int pn_req;
unsigned short station;

char *progname, *logfile;
int  syslog_mode = 0, exit_status = EXIT_SUCCESS;

#if AURORA
config_aurora(fd,baud)
  int fd;
  int baud;
  {
  int i;
  hdlc_cfg_t config;
  struct strioctl ioctlptr;

  config.pad       =0;                /* ? */
  config.max_frame =MAXMESG;
  config.addressing=CFG_ADDR_NONE;
  config.duplex    =CFG_FDX;
  config.idle      =CFG_IFLAG;
  if(baud)
    {
    config.tx_clock  =CFG_TCLK_INT;
    switch(baud)
      {
      case 9600:
        config.speed=CFG_S9600;
        break; 
      case 19200:
        config.speed=CFG_S19200;
        break; 
      case 38400:
        config.speed=CFG_S38400;
        break; 
      case 56000:
        config.speed=CFG_S56000;
        break; 
      case 64000:
        config.speed=CFG_S64000;
        break; 
      case 128000:
        config.speed=CFG_S128000;
        break; 
      default:
        fprintf(stderr,"Illegal bit rate %d bps.",baud);
        exit(-1);
      }
    }
  else
    {
    config.tx_clock=CFG_TCLK_EXT;
    config.speed=CFG_S9600;
    }
  config.rx_clock  =CFG_RCLK_EXT;
  config.encoding  =CFG_NRZ;
  config.crc       =CFG_CRC_V41; /* V41 is important ! */
  config.cnx       =CFG_CNX_NONE;
  config.notify    =CFG_NOTIFY;
  config.rtn_data  =CFG_NORTN_DATA;
  config.rtn_all   =CFG_NORTN_ALL;
  config.gap       =0;                /* ? */

  ioctlptr.ic_cmd   =BIOCFG;
  ioctlptr.ic_timout=0;
  ioctlptr.ic_len   =sizeof(hdlc_cfg_t);
  ioctlptr.ic_dp    =(char *)&config;

  if((i=ioctl(fd,I_STR,&ioctlptr))==(-1)) {
    perror("HDLC CFG failed");
    exit(-1);
  }

  ioctlptr.ic_cmd   =BIODCNX;
  ioctlptr.ic_timout=0;
  ioctlptr.ic_len   =0;
  ioctlptr.ic_dp    =NULL;

  if((i=ioctl(fd,I_STR,&ioctlptr))==(-1)) {
    perror("HDLC DCNX failed");
    exit(-1);
  }

  ioctlptr.ic_cmd   =BIOCNX;
  ioctlptr.ic_timout=0;
  ioctlptr.ic_len   =0;
  ioctlptr.ic_dp    =NULL;

  if((i=ioctl(fd,I_STR,&ioctlptr))==(-1)) {
    perror("HDLC CNX failed");
    exit(-1);
  }

  read_aurora(fd);
  }

read_aurora(fd)
  int fd;
  {
  unsigned char rcvmsg[MAXMESG],rcvctl[MAXMESG];
  bx_msg_t *bxmsg;
  bx_tstmp_msg_t *bxtmsg;
  struct strbuf rdataptr,rctlptr;
  int rflag;

  /* Populate the STREAM buffer structures */
  rdataptr.maxlen=sizeof(rcvmsg);
  rdataptr.len = 0;
  rdataptr.buf =(char *)rcvmsg;
    
  rctlptr.maxlen=sizeof(rcvctl);
  rctlptr.len=0;
  rctlptr.buf=(char *)rcvctl;
  rflag=0;

  /* Read */
  if(getmsg(fd,&rctlptr,&rdataptr,&rflag)<0) perror("Error while reading");
  else {
    if(rdataptr.len >0) {
      printf("%d bytes data received\n",rdataptr.len);
      }
    if(rctlptr.len >0) {
       printf("%d bytes control received\n",rctlptr.len);
       /* Check the control part */
       bxmsg = (bx_msg_t *)rctlptr.buf;
       if(bxmsg->bx_msgid == NOTIFY_CNX)
          printf("Connection established\n");
       else if (bxmsg->bx_msgid == NOTIFY_DCNX) 
          printf("Disconnected ...\n");
       else if (bxmsg->bx_msgid == NOTIFY_TX_CMPLT) {
          if(bxmsg->bx_status != STS_XOK) {
             printf("Transmition problem...");
             switch(bxmsg->bx_status)
               {
               case STS_XUR:
                 printf("Underrun.");
                 break;
               case STS_XUSR:
                 printf("User Abort.");
                 break;
               }
          }
       }
    }
  }
  return rdataptr.len;
}
#endif

config_zsh(fd,baud)
  int fd;
  int baud;
  {
  int i;
  struct scc_mode config;
  struct strioctl ioctlptr;
#if DEBUG2
  ioctlptr.ic_cmd   =S_IOCGETMODE;
  ioctlptr.ic_timout=0;
  ioctlptr.ic_len   =sizeof(config);
  ioctlptr.ic_dp    =(char *)&config;;
  if((i=ioctl(fd,I_STR,&ioctlptr))==(-1))
    {
    perror("HDLC CFG failed");
    exit(-1);
    }
  printf("txc=%d rxc=%d iflag=%d config=%d baud=%d ret=%d\n",
    config.sm_txclock,config.sm_rxclock,config.sm_iflags,config.sm_config,
    config.sm_baudrate,config.sm_retval);
#endif
  if(baud) config.sm_txclock=TXC_IS_BAUD;
  else config.sm_txclock=TXC_IS_TXC;
  config.sm_rxclock=RXC_IS_RXC;
  config.sm_iflags=TRXD_NO_INVERT;
  config.sm_config=0;
  config.sm_baudrate=baud;

  ioctlptr.ic_cmd   =S_IOCSETMODE;
  ioctlptr.ic_timout=0;
  ioctlptr.ic_len   =sizeof(config);
  ioctlptr.ic_dp    =(char *)&config;;
  if((i=ioctl(fd,I_STR,&ioctlptr))==(-1))
    {
    perror("HDLC CFG failed");
    exit(-1);
    }
#if DEBUG2
  printf("txc=%d rxc=%d iflag=%d config=%d baud=%d ret=%d\n",
    config.sm_txclock,config.sm_rxclock,config.sm_iflags,config.sm_config,
    config.sm_baudrate,config.sm_retval);
#endif
  }

check_pno_s(to_addr,pn,pn_f,sock,fd_req) /* returns -1 if duplicated */
  struct sockaddr_in *to_addr;    /* sender address */
  unsigned int pn,pn_f;           /* present and former packet Nos. */
  int sock;                       /* socket */
  int fd_req;                     /* fd to request resend */
  {
#define N_PACKET  8    /* N of old packets to be requested */
  int i,j;
  static int nos[256],init,no;
  unsigned int pn_1;
  unsigned char pnc[8];

  j=no;      /* save 'no' */
  no=pn;     /* update 'no' */
  nos[pn]=1; /* register 'pn' */

  pn_1=(j+1)&0xff; /* pno expected from previous one */
  if(pn!=pn_1 && ((pn-pn_1)&0xff)<N_PACKET && init) do
    { /* send request-resend packet(s) */
    pnc[0]=station>>8;      /* my station ID (high) */
    pnc[1]=station;         /* my station ID (low) */
    pnc[2]=pnc[3]=pn_req++; /* packet No. */
    pnc[4]=0xDE;            /* packet ID */
    pnc[5]=rbuf[0];         /* station ID of sender (high) */
    pnc[6]=rbuf[1];         /* station ID of sender (low) */
    pnc[7]=pn_1;            /* packet No. to request */
    if(sock>=0)
      {
      sendto(sock,pnc,8,0,(struct sockaddr *)to_addr,sizeof(*to_addr));
      sprintf(tb,"request resend %s:%d #%02X",
        inet_ntoa(to_addr->sin_addr),ntohs(to_addr->sin_port),pn_1);
      write_log(tb);
#if DEBUG1
      printf("<%d ",pn_1);
#endif
      }
    if(fd_req>=0)
      {
      write(fd_req,pnc,8);
      sprintf(tb,"request resend #%02X",pn_1);
      write_log(tb);
#if DEBUG1
      printf("<%d ",pn_1);
#endif
      }
    nos[pn_1]=0;  /* reset bit for the packet no */
    } while((pn_1=(++pn_1&0xff))!=pn);
  init=1;
  if(pn!=pn_f && nos[pn_f]) return -1;
     /* if the resent packet is duplicated, return with -1 */
  return 0;
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  key_t shm_key;
  int shmid;
  unsigned long uni;
  unsigned char *ptr,tm[6],*ptr_size,device[80];
  int i,j,k,size,n,re,fd,baud,aurora,c,fd_req,req_line;
  struct Shm  *sh;
  int sock;
  struct sockaddr_in to_addr;
  struct hostent *h;
  unsigned short host_port;
  unsigned char host_name[100];
  char tb2[256];

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  baud=aurora=host_port=req_line=0;
  fd_req=sock=(-1);

  sprintf(tb,
    " usage : '%s (-as) (-i my_ID) (-p host:port) (-b rate) [device] [shm_key] [shm_size(KB)] ([log file])'",
    progname);
  while((c=getopt(argc,argv,"ap:b:si:"))!=-1)
    {
    switch(c)
      {
      case 'i':   /* my station ID */
        station=atoi(optarg);
        break;
      case 'b':   /* baud rate */
        baud=atoi(optarg);
        break;
      case 'a':   /* use AURORA board */
        aurora=1;
        break;
      case 'p':   /* host:port */
        strcpy(tb2,optarg);
        if((ptr=(unsigned char *)index(tb2,':'))==NULL)
          {
          fprintf(stderr,"Illegal host:port\n");
          exit(-1);
          }
        *ptr=0;
        strcpy(host_name,tb2);
        host_port=atoi(ptr+1);
        break;
      case 's':   /* send request resend packets to line */
        req_line=1;
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
        exit(1);
      }
    }
  optind--;
  if(argc<4+optind)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }
  strcpy(device,argv[1+optind]);
  shm_key=atoi(argv[2+optind]);
  size=atoi(argv[3+optind])*1000;
  logfile=NULL;
  if(argc>4+optind) logfile=argv[4+optind];

  /* shared memory */
  if((shmid=shmget(shm_key,size,IPC_CREAT|0666))<0) err_sys("shmget");
  if((sh=(struct Shm *)shmat(shmid,(char *)0,0))==(struct Shm *)-1)
    err_sys("shmat");

  /* initialize buffer */
  Shm_init(sh, size);    /* previous code had bug?? sh->p=0 ??? */
  /*   sh->c=0; */
  /*   sh->pl=(size-sizeof(*sh))/10*9; */
  /*   sh->p=sh->r=(-1); */

  sprintf(tb,"start shm_key=%d id=%d size=%d",shm_key,shmid,size);
  write_log(tb);

  if(host_port)
    {
    /* destination host/port */
    if(!(h=gethostbyname(host_name))) err_sys("can't find host");
    memset((char *)&to_addr,0,sizeof(to_addr));
    to_addr.sin_family=AF_INET;
    memcpy((caddr_t)&to_addr.sin_addr,h->h_addr,h->h_length);
/*  to_addr.sin_addr.s_addr=mkuint4(h->h_addr);*/
    to_addr.sin_port=htons(host_port);

    /* my socket */
    if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");
    if(setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(char *)&i,sizeof(i))<0)
      err_sys("SO_BROADCAST setsockopt error\n");
    }

  if((fd=open(device,O_RDWR))<0) err_sys("open HDLC device");
  if(aurora)
#if AURORA
    config_aurora(fd,baud)
#endif
    ;
  else config_zsh(fd,baud);
  if(req_line) fd_req=fd;

  if(baud) sprintf(tb,"use internal TX clock %d bps",baud);
  else sprintf(tb,"use external TX clock");
  write_log(tb);
  if(host_port)
    {
    sprintf(tb,"resend_req_port=%s:%d\n",host_name,host_port);
    write_log(tb);
    }
  if(req_line) write_log("resend_req to line");

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

  for(i=0;i<6;i++) tm[i]=(-1);
  ptr=ptr_size=sh->d;

  while(1)
    {
    n=read(fd,rbuf,MAXMESG);
#if TEST_RESEND
    if(rbuf[2]==0x80) continue;
#endif
#if DEBUG3
    printf("%3d ",n);
    printf("%02X%02X %02X:%02X %02X %02X%02X%02X %02X%02X%02X ",
      rbuf[0],rbuf[1],rbuf[2],rbuf[3],rbuf[4],rbuf[5],rbuf[6],rbuf[7],rbuf[8],
      rbuf[9],rbuf[10]);
    for(i=11;i<30;i++) printf("%02X",rbuf[i]);
    printf("\n");
#endif

    if(check_pno_s(&to_addr,rbuf[2],rbuf[3],sock,fd_req)<0)
      {
#if DEBUG2
      sprintf(tb,"discard duplicated resent packet #%d for #%d",
        rbuf[2],rbuf[3]);
      write_log(tb);
#endif
      continue;
      }
    ptr_size=ptr;
    ptr+=4;   /* size */
    ptr+=4;   /* time of write */
    memcpy(ptr,rbuf+4,n-4);
    ptr+=n-4;
    uni=ptr-ptr_size;
    ptr_size[0]=uni>>24;  /* size (H) */
    ptr_size[1]=uni>>16;
    ptr_size[2]=uni>>8;
    ptr_size[3]=uni;      /* size (L) */
    uni=time(NULL);
    ptr_size[4]=uni>>24;  /* tow (H) */
    ptr_size[5]=uni>>16;
    ptr_size[6]=uni>>8;
    ptr_size[7]=uni;      /* tow (L) */

    sh->r=sh->p;      /* latest */
    if(ptr>sh->d+sh->pl) ptr=sh->d;
    sh->p=ptr-sh->d;
    sh->c++;

#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    }
  }
