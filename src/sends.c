/* $Id: sends.c,v 1.9 2011/06/01 11:09:22 uehira Exp $ */
/*   program "sends"   2000.3.20 urabe                   */
/*   2000.3.21 */
/*   2000.4.17 */
/*   2000.4.24 strerror() */
/*   2000.12.20 option -i fixed */
/*   2001.11.14 strerror(),ntohs() */
/*   2004.11.26 some systems (exp. Linux), select(2) changes timeout value */

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
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <unistd.h>
#if HAVE_STROPTS_H
#include <stropts.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#if HAVE_SYS_SER_SYNC_H
#include <sys/ser_sync.h>
#endif
#include <errno.h>
#if AURORA
#include "/opt/AURAacs/syncuser.h"
#endif

#include "winlib.h"

#define DEBUG       0
#define DEBUG1      0
#define DEBUG2      0
#define TEST_RESEND 0
#define MAXMESG   (1500-28)  /* max of UDP data size, +28 <= IP MTU  */
#define BUFNO     128

int sock,psize[BUFNO];
unsigned char sbuf[BUFNO][MAXMESG],rbuf[MAXMESG];
char *progname, *logfile;
int  syslog_mode = 0, exit_status = EXIT_SUCCESS;

get_packet(bufno,no)
  int bufno;  /* present(next) bufno */
  unsigned char no; /* packet no. to find */
  {
  int i;
  if((i=bufno-1)<0) i=BUFNO-1;
  while(i!=bufno && psize[i]>0)
    {
    if(sbuf[i][2]==no) return (i);
    if(--i<0) i=BUFNO-1;
    }
  return (-1);  /* not found */
  }

#if AURORA
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
  return (rdataptr.len);
}

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

main(argc,argv)
  int argc;
  char *argv[];
  {
  FILE *fp;
  key_t shm_key;
  unsigned long uni;
  int fd,i,j,c_save,shp,bufno,bufno_f,fromlen,c,station,baud,aurora,dupl;
  struct sockaddr_in to_addr,from_addr;
  struct hostent *h;
  unsigned short to_port;
  int size,re;
  unsigned char *ptr,*ptr_save,*ptr_lim,*ptw,*ptw_save,device[80],
    no,no_f,host_name[100],tbuf[100],nop;
  struct Shm  *shm;
  extern int optind;
  extern char *optarg;
  struct timeval timeout,tp;
  struct timezone tzp;
  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];
  baud=aurora=to_port=dupl=station=0;
  sock=(-1);
  sprintf(tbuf,
" usage : '%s (-ad) (-p req_port) (-b rate) (-i my_ID) [shm_key] [device] ([log file]))'",
    progname);
  while((c=getopt(argc,argv,"ap:b:di:"))!=-1)
    {
    switch(c)
      {
      case 'i':   /* station ID in HEX */
        sscanf(optarg,"%x",&station);
        break;
      case 'b':   /* baud rate */
        baud=atoi(optarg);
        break;
      case 'a':   /* use AURORA board */
        aurora=1;
        break;
      case 'd':   /* double-send mode */
        dupl=1;
        break;
      case 'p':   /* request port */
        to_port=atoi(optarg);
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tbuf);
        exit(1);
      }
    }
  optind--;
  if(argc<3+optind)
    {
    fprintf(stderr,"%s\n",tbuf);
    exit(0);
    }

  shm_key=atoi(argv[1+optind]);
  strcpy(device,argv[2+optind]);
  logfile=NULL;
  if(argc>3+optind) logfile=argv[3+optind];
    
  /* shared memory */
  shm = Shm_read(shm_key, "start");
  /* if((shmid=shmget(shm_key,0,0))<0) err_sys("shmget"); */
  /* if((shm=(struct Shm *)shmat(shmid,(void *)0,0))==(struct Shm *)-1) */
  /*   err_sys("shmat"); */

  /* sprintf(tbuf,"start shm_key=%d id=%d",shm_key,shmid); */
  /* write_log(tbuf); */

  if(to_port)
    {
    if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("socket");
    memset((char *)&to_addr,0,sizeof(to_addr));
    to_addr.sin_family=AF_INET;
    to_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    to_addr.sin_port=htons(to_port);
    if(bind(sock,(struct sockaddr *)&to_addr,sizeof(to_addr))<0)
      err_sys("bind");
    sprintf(tbuf,"accept resend request at localhost:%d",to_port);
    write_log(tbuf);
    }

  if((fd=open(device,O_RDWR))<0) err_sys("open HDLC device");
  if(aurora)
#if AURORA
    config_aurora(fd,baud)
#endif
    ;
  else config_zsh(fd,baud);

  if(baud) sprintf(tbuf,"use internal TX clock %d bps",baud);
  else sprintf(tbuf,"use external TX clock");
  write_log(tbuf);

  signal(SIGPIPE,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGTERM,(void *)end_program);
  for(i=0;i<BUFNO;i++) psize[i]=(-1);
  no=bufno=0;

reset:
  while(shm->r==(-1)) sleep(1);
  c_save=shm->c;
  size=mkuint4(ptr_save=shm->d+(shp=shm->r));

  for(;;)
    {
    if(shp+size>shm->pl) shp=0; /* advance pointer */
    else shp+=size;
    while(shm->p==shp) usleep(10000);
    i=mkuint4(ptr_save);
    if(shm->c<c_save || i!=size)
      {   /* previous block has been destroyed */
      write_log("reset");
      goto reset;
      }
    c_save=shm->c;
    size=mkuint4(ptr_save=ptr=shm->d+shp);
    ptr_lim=ptr+size;
    ptr+=4; /* size */
    ptr+=4; /* tow */

    for(j=0;j<=dupl;j++)
      {
      ptw=ptw_save=sbuf[bufno];
      *ptw++=station>>8; /* station address (high) */
      *ptw++=station;    /* station address (low) */
      *ptw++=no;  /* packet no. */
      if(j) *ptw++=nop;
      else *ptw++=no;  /* packet no.(2) */
      memcpy(ptw,ptr,size-8);
      ptw+=size-8;
#if TEST_RESEND
      if(no%10!=9) {
#endif
      re=write(fd,ptw_save,psize[bufno]=ptw-ptw_save);
#if DEBUG1
      fprintf(stderr,"%5d>  ",re);
      for(i=0;i<20;i++) fprintf(stderr,"%02X",ptw_save[i]);
      fprintf(stderr,"\n");
#endif
#if TEST_RESEND
      } else psize[bufno]=ptw-ptw_save;
#endif
#if DEBUG
      fprintf(stderr,"%5d>  ",re);
#endif
      if(++bufno==BUFNO) bufno=0;
      nop=no++;
      }
/* resend if requested */
    if(sock>=0) for(;;)
      {
      i=1<<sock;
      timeout.tv_sec=timeout.tv_usec=0;
      if(select(sock+1,&i,NULL,NULL,&timeout)>0)
        {
        fromlen=sizeof(from_addr);
        if(recvfrom(sock,rbuf,MAXMESG,0,&from_addr,&fromlen)==8 &&
            (bufno_f=get_packet(bufno,no_f=rbuf[7]))>=0)
            for(j=0;j<=dupl;j++)
          {
          memcpy(sbuf[bufno],sbuf[bufno_f],psize[bufno]=psize[bufno_f]);
          sbuf[bufno][2]=no;    /* packet no. */
          sbuf[bufno][3]=no_f;  /* old packet no. */
          re=write(fd,sbuf[bufno],psize[bufno]);
#if DEBUG1
          fprintf(stderr,"%5d>  ",re);
          for(i=0;i<20;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
          fprintf(stderr,"\n");
#endif
          sprintf(tbuf,"resend for %s:%d #%d as #%d, %d B",
            inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),no_f,no,re);
          write_log(tbuf);
          if(++bufno==BUFNO) bufno=0;
          no++;
          }
        }
      else break;
      }
    else for(;;)
      {
      i=1<<fd;
      timeout.tv_sec=timeout.tv_usec=0;
      if(select(fd+1,&i,NULL,NULL,&timeout)>0)
        {
        re=read(fd,rbuf,MAXMESG);
/*        if((re=read(fd,rbuf,MAXMESG))==8 && rbuf[4]==0xDE && */
        if(re==8 && rbuf[4]==0xDE && 
            (bufno_f=get_packet(bufno,no_f=rbuf[7]))>=0)
            for(j=0;j<=dupl;j++)
          {
          memcpy(sbuf[bufno],sbuf[bufno_f],psize[bufno]=psize[bufno_f]);
          sbuf[bufno][2]=no;    /* packet no. */
          sbuf[bufno][3]=no_f;  /* old packet no. */
          re=write(fd,sbuf[bufno],psize[bufno]);
#if DEBUG1
          fprintf(stderr,"%5d>  ",re);
          for(i=0;i<20;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
          fprintf(stderr,"\n");
#endif
          sprintf(tbuf,"resend for line #%d as #%d, %d B",no_f,no,re);
          write_log(tbuf);
          if(++bufno==BUFNO) bufno=0;
          no++;
          }
        }
      else break;
      }
    }
  }
