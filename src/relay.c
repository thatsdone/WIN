/* $Id: relay.c,v 1.15.4.3.2.10.2.1 2011/01/08 02:03:22 uehira Exp $ */
/*-
 "relay.c"      5/23/94-5/25/94,6/15/94-6/16/94,6/23/94,3/16/95 urabe
                3/26/95 check_packet_no; port#
                5/24/96 added processing of "host table full"
                5/28/96 bzero -> memset
                5/31/96 bug fix
                6/11/96 discard duplicated resent packets 
                6/23/97 SNDBUF/RCVBUF=65535 
                97.9.3               &50000 
                98.6.30 FreeBSD 
                2000.4.24/2001.11.14 strerror()
                2001.11.14 ntohs()
                2002.12.10 multicast(-i,-t), stdin(to_port=0), src_port(-p)
                2003.3.25 bug fixed in optind
                2003.3.26 bug fixed in psize for stdin 
                2003.9.9 no req resend(-r), receive mcast(-g), send delay(-d)
                2003.10.26 "sinterface"
                2004.9.9 fixed a bug in -s option ("sinterface")
                2004.10.26 daemon mode (Uehira)
                2004.11.15 check_pno() and read_chfile() brought from recvt.c
                           with host contol but without channel control (-f)
                2004.11.15 write host statistics on HUP signal
                2004.11.15 no packet info (-n)
                2004.11.15 corrected byte-order of port no. in log
                2004.11.16 maximize size of receive socket buffer (-b)
                2004.11.26 some systems (exp. Linux), select(2) changes timeout value
                2005.2.20 added fclose() in read_chfile()
                2005.5.18 -N for don't change (and ignore) packet numbers
                2010.9.30 64bit clean?
		2011.1.7 -e for automatically reload chfile
		            if packet comes from deny host.
-*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>

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

#include "daemon_mode.h"
#include "winlib.h"
#include "udpu.h"

/* #define DEBUG     0 */
#define DEBUG1    0
#define BELL      0
#define MAXMESG   2048
#define N_PACKET  64    /* N of old packets to be requested */  
#define BUFNO     128
#define N_HOST    100   /* max N of hosts */  

static const char rcsid[] =
  "$Id: relay.c,v 1.15.4.3.2.10.2.1 2011/01/08 02:03:22 uehira Exp $";

static int sock_in,sock_out;   /* socket */
static uint8_w sbuf[BUFNO][MAXMESG],ch_table[WIN_CHMAX];
static int negate_channel,hostlist[N_HOST][2],n_host,no_pinfo,
    n_ch;
static ssize_t  psize[BUFNO];
static int  daemon_mode;
static char *host_name,*chfile;
static int auto_reload_chfile;

char *progname, *logfile;
int  syslog_mode, exit_status;

struct {
    in_addr_t host;  /* ok */
    in_port_t port;  /* ok */
    int32_w no;
    uint8_w nos[256/8];
    unsigned long n_bytes;       /*- 64bit ok -*/
    unsigned long n_packets;       /*- 64bit ok -*/
  /*     unsigned int n_bytes; */
  /*     unsigned int n_packets; */
    } ht[N_HOST];

/* prototypes */
static void read_chfile(void);
static int check_pno(struct sockaddr_in *, unsigned int, unsigned int,
		     int, socklen_t, ssize_t, int, int);
static int get_packet(int, uint8_w);
static void usage(void);
int main(int, char *[]);

static void
read_chfile()
  {
  FILE *fp;
  int i,j,k,ii;
  char tbuf[1024],hostname[1024],tb[256];
  struct hostent *h;
  static time_t ltime,ltime_p;
  time_t  tdif, tdif2;

  n_host=0;
  if(chfile != NULL)
    {
    if((fp=fopen(chfile,"r"))!=NULL)
      {
#if DEBUG
      fprintf(stderr,"ch_file=%s\n",chfile);
#endif
      if(negate_channel) for(i=0;i<WIN_CHMAX;i++) ch_table[i]=1;
      else for(i=0;i<WIN_CHMAX;i++) ch_table[i]=0;
      ii=0;
      while(fgets(tbuf,sizeof(tbuf),fp))
        {
        *hostname=0;
        if(sscanf(tbuf,"%s",hostname)==0) continue;  /* buffer overrun ok */
        if(*hostname==0 || *hostname=='#') continue;
        if(*tbuf=='*') /* match any channel */
          {
          if(negate_channel) for(i=0;i<WIN_CHMAX;i++) ch_table[i]=0;
          else for(i=0;i<WIN_CHMAX;i++) ch_table[i]=1;
          }
        else if(n_host==0 && (*tbuf=='+' || *tbuf=='-'))
          {
          if(*tbuf=='+') hostlist[ii][0]=1;   /* allow */
          else hostlist[ii][0]=(-1);          /* deny */
          if(tbuf[1]=='\r' || tbuf[1]=='\n' || tbuf[1]==' ' || tbuf[1]=='\t')
            {
            hostlist[ii][1]=0;                  /* any host */
            if(*tbuf=='+') write_log("allow from the rest");
            else write_log("deny from the rest");
            }
          else
            {
            if(sscanf(tbuf+1,"%s",hostname)>0) /* hostname */
              {
              if(!(h=gethostbyname(hostname)))
                {
		  snprintf(tb,sizeof(tb),"host '%s' not resolved",hostname);
                write_log(tb);
                continue;
                }
              memcpy((char *)&hostlist[ii][1],h->h_addr,4);
              if(*tbuf=='+') sprintf(tb,"allow");
              else sprintf(tb,"deny");
              snprintf(tb+strlen(tb),sizeof(tb)-strlen(tb),
		       " from host %d.%d.%d.%d",
		       ((uint8_w *)&hostlist[ii][1])[0],
		       ((uint8_w *)&hostlist[ii][1])[1],
		       ((uint8_w *)&hostlist[ii][1])[2],
		       ((uint8_w *)&hostlist[ii][1])[3]);
              write_log(tb);
              }
            }
          if(++ii==N_HOST)
            {
            n_host=ii;
            write_log("host control table full"); 
            }
          }
        else
          {
          sscanf(tbuf,"%x",&k);
          k&=0xffff;
#if DEBUG
          fprintf(stderr," %04X",k);
#endif
          if(negate_channel) ch_table[k]=0;
          else ch_table[k]=1;
          }
        }
#if DEBUG
      fprintf(stderr,"\n");
#endif
      if(ii>0 && n_host==0) n_host=ii;
      snprintf(tb,sizeof(tb),"%d host rules",n_host);
      write_log(tb);
      j=0;
      if(negate_channel)
        {
        for(i=0;i<WIN_CHMAX;i++) if(ch_table[i]==0) j++;
        if((n_ch=j)==WIN_CHMAX) sprintf(tb,"-all channels");
        else snprintf(tb,sizeof(tb),"-%d channels",n_ch=j);
        }
      else
        {
        for(i=0;i<WIN_CHMAX;i++) if(ch_table[i]==1) j++;
        if((n_ch=j)==WIN_CHMAX) sprintf(tb,"all channels");
        else snprintf(tb,sizeof(tb),"%d channels",n_ch=j);
        }
      write_log(tb);
      fclose(fp);
      }
    else
      {
#if DEBUG
      fprintf(stderr,"ch_file '%s' not open\n",chfile);
#endif
      snprintf(tb,sizeof(tb),"channel list file '%s' not open",chfile);
      err_sys(tb);
      /* write_log(tb); */
      /* write_log("end"); */
      /* exit(1); */
      }
    }
  else
    {
    for(i=0;i<WIN_CHMAX;i++) ch_table[i]=1;
    n_ch=i;
    n_host=0;
    write_log("all channels");
    }

  time(&ltime);
  tdif=ltime-ltime_p;
  tdif2=tdif/2;
  if(ht[0].host)
    {
    snprintf(tb,sizeof(tb),"statistics in %ld s (pkts, B, pkts/s, B/s)",tdif);
    write_log(tb);
    }
  for(i=0;i<N_HOST;i++) /* print statistics for hosts */
    {
    if(ht[i].host==0) break;
    snprintf(tb,sizeof(tb), "  src %d.%d.%d.%d:%d   %lu %lu %lu %lu",
	     ((uint8_w *)&ht[i].host)[0],((uint8_w *)&ht[i].host)[1],
	     ((uint8_w *)&ht[i].host)[2],((uint8_w *)&ht[i].host)[3],
	     ntohs(ht[i].port),ht[i].n_packets,ht[i].n_bytes,
	     (ht[i].n_packets+tdif2)/tdif,(ht[i].n_bytes+tdif2)/tdif);
    write_log(tb);
    ht[i].n_packets=ht[i].n_bytes=0;
    }
  ltime_p=ltime;
  signal(SIGHUP,(void *)read_chfile);
  }

/* returns -1 if duplicated */
static int
check_pno(struct sockaddr_in *from_addr, unsigned int pn, unsigned int pn_f,
	  int sock, socklen_t fromlen, ssize_t n, int nr, int nopno)
/* struct sockaddr_in *from_addr;   sender address */
/* unsigned int  pn,pn_f;           present and former packet Nos. */
/* int sock;                        socket */
/* socklen_t fromlen;               length of from_addr */
/* ssize_t n;                       size of packet */
/* int nr;                          no resend request if 1 */
/* int nopno;                       don't check pno */

/* global : hostlist, n_host, no_pinfo ?*/
  {
  int i,j;
  in_addr_t  host_;  /* 32-bit-long host address in network byte-order */
  in_port_t  port_;  /* port No. in network byte-order */
  unsigned int  pn_1;
  static uint8_w mask[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
  uint8_w pnc;
  char tb[256];

  j=(-1);
  host_=from_addr->sin_addr.s_addr;
  port_=from_addr->sin_port;
  for(i=0;i<n_host;i++)
    {
    if(hostlist[i][0]==1 && (hostlist[i][1]==0 || hostlist[i][1]==host_)) break;
    if(hostlist[i][0]==(-1) && (hostlist[i][1]==0 || hostlist[i][1]==host_))
      {
      if(!no_pinfo)
        {
	snprintf(tb,sizeof(tb),"deny packet from host %d.%d.%d.%d:%d",
          ((uint8_w *)&host_)[0],((uint8_w *)&host_)[1],
	 ((uint8_w *)&host_)[2],((uint8_w *)&host_)[3],ntohs(port_));
	write_log(tb);
        }
      /* automatically reload chfile */
      if (auto_reload_chfile)
	read_chfile();
       return (-2);
      }
    }
  if(nopno) return (0);
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
    write_log("host table full - flushed.");
    i=0;
    }
  if(j<0)
    {
    ht[i].host=host_;
    ht[i].port=port_;
    ht[i].no=pn;
    ht[i].nos[pn>>3]|=mask[pn&0x07]; /* set bit for the packet no */
    snprintf(tb,sizeof(tb),"registered host %d.%d.%d.%d:%d (%d)",
      ((uint8_w *)&host_)[0],((uint8_w *)&host_)[1],
      ((uint8_w *)&host_)[2],((uint8_w *)&host_)[3],ntohs(port_),i);
    write_log(tb);
    ht[i].n_bytes=n;
    ht[i].n_packets=1;
    }
  else /* check packet no */
    {
    pn_1=(j+1)&0xff;
    if(!nr && (pn!=pn_1))
      {
      if(((pn-pn_1)&0xff)<N_PACKET) do
        { /* send request-resend packet(s) */
	  pnc=(uint8_w)pn_1;
        sendto(sock,&pnc,1,0,(struct sockaddr *)from_addr,fromlen);
        snprintf(tb,sizeof(tb),"request resend %s:%d #%u",
          inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1);
        write_log(tb);
#if DEBUG1
        printf("<%u ",pn_1);
#endif
        ht[i].nos[pn_1>>3]&=~mask[pn_1&0x07]; /* reset bit for the packet no */
        } while((pn_1=(++pn_1&0xff))!=pn);
      else
        {
	  snprintf(tb,sizeof(tb),"no request resend %s:%d #%u-#%u",
          inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1,pn);
        write_log(tb);
        }
      }
    }
  if(pn!=pn_f && ht[i].nos[pn_f>>3]&mask[pn_f&0x07])
    {   /* if the resent packet is duplicated, return with -1 */
    if(!no_pinfo)
      {
	snprintf(tb,sizeof(tb),"discard duplicated resent packet #%u for #%u",pn,pn_f);
      write_log(tb);
      }
    return (-1);
    }
  return (0);
  }

static int
get_packet(int bufno, uint8_w no)
/* int bufno;   present(next) bufno */
/* uint8_w no;  packet no. to find */
  {
  int i;

  if((i=bufno-1)<0) i=BUFNO-1;
  while(i!=bufno && psize[i]>2)
    {
    if(sbuf[i][0]==no) return (i);
    if(--i<0) i=BUFNO-1;
    }
  return (-1);  /* not found */
  }

static void
usage()
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if (daemon_mode)
    fprintf(stderr,
	    " usage : '%s (-eNnr) (-b [sbuf(KB)]) (-d [delay_ms]) (-f [host_file]) (-g [mcast_group]) (-i [interface]) (-h [len(s)]) (-s [sinterface]) (-p [src port]) (-t [ttl]) [in_port] [host] [host_port] ([log file])'\n",
	    progname);
  else
    fprintf(stderr,
	    " usage : '%s (-eNnrD) (-b [sbuf(KB)]) (-d [delay_ms]) (-f [host_file]) (-g [mcast_group]) (-i [interface]) (-h [len(s)]) (-s [sinterface]) (-p [src port]) (-t [ttl]) [in_port] [host] [host_port] ([log file])'\n",
	    progname);
}

int
main(int argc, char *argv[])
  {
  struct timeval timeout,tv1,tv2;
  double idletime;
  int c,i,bufno,bufno_f,ttl,delay,noreq,sockbuf,nopno;
  ssize_t re;
  socklen_t fromlen;  /*- 64bit ok -*/
  struct sockaddr_in to_addr,from_addr;
  uint16_t  to_port;
  /* struct hostent *h; */
  uint16_t  host_port,src_port;
  uint8_w no,no_f;
  char tb[256];
  struct ip_mreq stMreq;
  char mcastgroup[256]; /* multicast address */
  char interface[256]; /* multicast interface for receive */
  char sinterface[256]; /* multicast interface for send */
  in_addr_t  mif; /* multicast interface address */

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;
  if(strcmp(progname,"relayd")==0) daemon_mode=1;

  (*interface)=(*mcastgroup)=(*sinterface)=0;
  chfile = NULL;
  ttl=1;
  no_pinfo=src_port=delay=noreq=negate_channel=nopno=0;
  sockbuf=DEFAULT_RCVBUF;
  auto_reload_chfile = 0;

  while((c=getopt(argc,argv,"b:Dd:ef:g:i:Nnp:rs:t:T:"))!=-1)
    {
    switch(c)
      {
      case 'b':   /* preferred socket buffer size (KB) */
        sockbuf=atoi(optarg);
        break;
      case 'D':
	daemon_mode = 1;  /* daemon mode */
	break;   
      case 'd':   /* delay time in msec */
        delay=atoi(optarg);
        break;
      case 'e':  /* automatically reload chfile if packet comes from denyhost */
	auto_reload_chfile = 1;
	break;
      case 'f':   /* host control file */
        chfile=optarg;
        break;
      case 'g':   /* multicast group for input (multicast IP address) */
        /* strcpy(mcastgroup,optarg); */
	if (snprintf(mcastgroup, sizeof(mcastgroup), "%s", optarg)
	    >= sizeof(mcastgroup)) {
	  fprintf(stderr,"'%s': -g option : Buffer overrun!\n",progname);
	  exit(1);
	}
        break;
      case 'i':   /* interface (ordinary IP address) which receive mcast */
        /* strcpy(interface,optarg); */
	if (snprintf(interface, sizeof(interface), "%s", optarg)
	    >= sizeof(interface)) {
	  fprintf(stderr,"'%s': -i option : Buffer overrun!\n",progname);
	  exit(1);
	}
        break;
      case 'N':   /* no pno */
        nopno=no_pinfo=noreq=1;
        break;
      case 'n':   /* supress info on abnormal packets */
        no_pinfo=1;
        break;
      case 'p':   /* source port */
        src_port=atoi(optarg);
        break;
      case 'r':   /* disable resend request */
        noreq=1;
        break;
      case 's':   /* interface (ordinary IP address) which sends mcast */
        /* strcpy(sinterface,optarg); */
	if (snprintf(sinterface, sizeof(sinterface), "%s", optarg)
	    >= sizeof(sinterface)) {
	  fprintf(stderr,"'%s': -s option : Buffer overrun!\n",progname);
	  exit(1);
	}
        break;
      case 'T':   /* ttl for MCAST */
      case 't':   /* ttl for MCAST */
        ttl=atoi(optarg);
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
  if (to_port == 0 && daemon_mode)
    {
      fprintf(stderr,
	      "daemon mode cannnot active in case of data from STDIN\n");
      exit(1);
    }
  host_name=argv[2+optind];
  host_port=atoi(argv[3+optind]);
  if(argc>4+optind) logfile=argv[4+optind];
  else
    {
      logfile=NULL;
      if (daemon_mode)
	syslog_mode = 1;
    }
  
  /* daemon mode */
  if (daemon_mode) {
    daemon_init(progname, LOG_USER, syslog_mode);
    umask(022);
  }

  snprintf(tb,sizeof(tb),"start in_port=%d to host '%s' port=%d",
	   to_port,host_name,host_port);
  write_log(tb);
  snprintf(tb,sizeof(tb),"auto_reload_chfile=%d",auto_reload_chfile);
  write_log(tb);

  if(to_port>0)
    {
    /* 'in' port */
    sock_in = udp_accept4(to_port, sockbuf);

    /* if((sock_in=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("sock_in"); */
    /* for(j=sockbuf;j>=16;j-=4) */
    /*   { */
    /*   i=j*1024; */
    /*   if(setsockopt(sock_in,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))>=0) */
    /*     break; */
    /*   } */
    /* if(j<16) err_sys("SO_RCVBUF setsockopt error\n"); */
    /* sprintf(tb,"RCVBUF size=%d",j*1024); */
    /* write_log(tb); */

    /* memset((char *)&to_addr,0,sizeof(to_addr)); */
    /* to_addr.sin_family=AF_INET; */
    /* to_addr.sin_addr.s_addr=htonl(INADDR_ANY); */
    /* to_addr.sin_port=htons(to_port); */
    /* if(bind(sock_in,(struct sockaddr *)&to_addr,sizeof(to_addr))<0) */
    /* err_sys("bind_in"); */

    /* in multicast */
    if(*mcastgroup)
      {
      stMreq.imr_multiaddr.s_addr=inet_addr(mcastgroup);
      if(*interface) stMreq.imr_interface.s_addr=inet_addr(interface);
      else stMreq.imr_interface.s_addr=INADDR_ANY;
      if(setsockopt(sock_in,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&stMreq,
        sizeof(stMreq))<0) err_sys("IP_ADD_MEMBERSHIP setsockopt error\n");
      }
    }

  /* destination host/port */
  sock_out = udp_dest4(host_name, host_port, &to_addr, 64, src_port);
  /* if(!(h=gethostbyname(host_name))) err_sys("can't find host"); */
  /* memset((char *)&to_addr,0,sizeof(to_addr)); */
  /* to_addr.sin_family=AF_INET; */
  /* memcpy((caddr_t)&to_addr.sin_addr,h->h_addr,h->h_length); */
  /* /\*  to_addr.sin_addr.s_addr=inet_addr(inet_ntoa(h->h_addr));*\/ */
  /* to_addr.sin_port=htons(host_port); */
  /* if((sock_out=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("sock_out"); */
  /* i=1; */
  /* if(setsockopt(sock_out,SOL_SOCKET,SO_BROADCAST,(char *)&i,sizeof(i))<0) */
  /*   err_sys("SO_BROADCAST setsockopt error\n"); */
  /* i=65535; */
  /* if(setsockopt(sock_out,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0) */
  /*   { */
  /*   i=50000; */
  /*   if(setsockopt(sock_out,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0) */
  /*     err_sys("SO_SNDBUF setsockopt error\n"); */
  /*   } */

  /* /\* bind my socket to a local port *\/ */
  /* memset((char *)&from_addr,0,sizeof(from_addr)); */
  /* from_addr.sin_family=AF_INET; */
  /* from_addr.sin_addr.s_addr=htonl(INADDR_ANY); */
  /* from_addr.sin_port=htons(src_port); */
  /* if(src_port) */
  /*   { */
  /*   sprintf(tb,"src_port=%d",src_port); */
  /*   write_log(tb); */
  /*   } */
  /* if(bind(sock_out,(struct sockaddr *)&from_addr,sizeof(from_addr))<0) */
  /*   err_sys("bind_out"); */

  /* out multicast */
  if(*sinterface)
    {
    mif=inet_addr(sinterface);
    if(setsockopt(sock_out,IPPROTO_IP,IP_MULTICAST_IF,&mif,sizeof(mif))<0)
      err_sys("IP_MULTICAST_IF setsockopt error\n");
    }
  if(ttl>1)
    {
    no=ttl;
    if(setsockopt(sock_out,IPPROTO_IP,IP_MULTICAST_TTL,&no,sizeof(no))<0)
      err_sys("IP_MULTICAST_TTL setsockopt error\n");
    }

  if(nopno) write_log("packet numbers pass through");
  if(noreq) write_log("resend request disabled");
  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);
  signal(SIGPIPE,(void *)end_program);
  no=0;
  for(i=0;i<BUFNO;i++) psize[i]=2;
  bufno=0;
  read_chfile();

  for(;;)
    {
    if(to_port>0)
      {
      gettimeofday(&tv1,NULL);
      fromlen=sizeof(from_addr);
      psize[bufno]=recvfrom(sock_in,sbuf[bufno],MAXMESG,0,
			  (struct sockaddr *)&from_addr,&fromlen);
      gettimeofday(&tv2,NULL);
      idletime=(double)(tv2.tv_sec-tv1.tv_sec)+
                 (double)(tv2.tv_usec-tv1.tv_usec)*0.000001;
#if DEBUG1
      printf("idletime=%lf\n",idletime);
      if(sbuf[bufno][0]==sbuf[bufno][1]) printf("%d ",sbuf[bufno][0]);
      else printf("%d(%d) ",sbuf[bufno][0],sbuf[bufno][1]);
#endif

      if(check_pno(&from_addr,sbuf[bufno][0],sbuf[bufno][1],sock_in,fromlen,
          psize[bufno],noreq,nopno)<0) continue;
      if(delay>0 && idletime>0.5) usleep(delay*1000);
      }
    else /* read from stdin */
      {
	if(fgets((char *)(sbuf[bufno]+2),MAXMESG-2,stdin)==NULL) end_program();
      if(sbuf[bufno][2]==0x04) end_program();
      psize[bufno]=strlen((char *)(sbuf[bufno]+2))+2;
      }

    if(!nopno) sbuf[bufno][0]=sbuf[bufno][1]=no;
    re=sendto(sock_out,sbuf[bufno],psize[bufno],0,
	      (const struct sockaddr *)&to_addr,sizeof(to_addr));
#if DEBUG
    for(i=0;i<11;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
    fprintf(stderr," : %zd > %zd\n",psize[bufno],re);
#endif
#if DEBUG1
    printf(">%u ",no);
#endif
    if(++bufno==BUFNO) bufno=0;
    no++;

    for(;;)
      {
      i=1<<sock_out;
      timeout.tv_sec=timeout.tv_usec=0;
      if(select(sock_out+1,(fd_set *)&i,NULL,NULL,&timeout)>0)
        {
        fromlen=sizeof(from_addr);
        if(recvfrom(sock_out,sbuf[bufno],MAXMESG,0,
		    (struct sockaddr *)&from_addr,&fromlen)==1 &&
            (bufno_f=get_packet(bufno,no_f=sbuf[bufno][0]))>=0)
          {
          memcpy(sbuf[bufno],sbuf[bufno_f],psize[bufno]=psize[bufno_f]);
          sbuf[bufno][0]=no;    /* packet no. */
          sbuf[bufno][1]=no_f;  /* old packet no. */
          re=sendto(sock_out,sbuf[bufno],psize[bufno],0,
		    (const struct sockaddr *)&to_addr,sizeof(to_addr));
          snprintf(tb,sizeof(tb),"resend to %s:%d #%d as #%d, %zd B",
		   inet_ntoa(to_addr.sin_addr),ntohs(to_addr.sin_port),
		   no_f,no,re);
          write_log(tb);
#if DEBUG
          for(i=0;i<8;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
          fprintf(stderr," : %zd > %zd\n",psize[bufno],re);
#endif
#if DEBUG1
          printf(">%d ",no);
#endif
          if(++bufno==BUFNO) bufno=0;
          no++;
          }
        }
      else break;
      }
#if BELL
    fprintf(stderr,"\007");
    fflush(stderr);
#endif
    }
  }
