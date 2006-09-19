/* $Id: relay.c,v 1.16 2006/09/19 23:17:00 urabe Exp $ */
/* "relay.c"      5/23/94-5/25/94,6/15/94-6/16/94,6/23/94,3/16/95 urabe */
/*                3/26/95 check_packet_no; port# */
/*                5/24/96 added processing of "host table full" */
/*                5/28/96 bzero -> memset */
/*                5/31/96 bug fix */
/*                6/11/96 discard duplicated resent packets */ 
/*                6/23/97 SNDBUF/RCVBUF=65535 */ 
/*                97.9.3               &50000 */ 
/*                98.6.30 FreeBSD */ 
/*                2000.4.24/2001.11.14 strerror() */
/*                2001.11.14 ntohs() */
/*                2002.12.10 multicast(-i,-t), stdin(to_port=0), src_port(-p) */
/*                2003.3.25 bug fixed in optind */
/*                2003.3.26 bug fixed in psize for stdin  */
/*                2003.9.9 no req resend(-r), receive mcast(-g), send delay(-d) */
/*                2003.10.26 "sinterface" */
/*                2004.9.9 fixed a bug in -s option ("sinterface") */
/*                2004.10.26 daemon mode (Uehira) */
/*                2004.11.15 check_pno() and read_chfile() brought from recvt.c */
/*                           with host contol but without channel control (-f) */
/*                2004.11.15 write host statistics on HUP signal */
/*                2004.11.15 no packet info (-n) */
/*                2004.11.15 corrected byte-order of port no. in log */
/*                2004.11.16 maximize size of receive socket buffer (-b) */
/*                2004.11.26 some systems (exp. Linux), select(2) changes timeout value */
/*                2005.2.20 added fclose() in read_chfile() */
/*                2005.5.18 -N for don't change (and ignore) packet numbers */
/*                2006.9.20 deleted mklong() */

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
#include <errno.h>
#include <syslog.h>

#include "daemon_mode.h"
#include "subst_func.h"

#define DEBUG     0
#define DEBUG1    0
#define BELL      0
#define MAXMESG   2048
#define N_PACKET  64    /* N of old packets to be requested */  
#define BUFNO     128
#define N_HOST    100   /* max N of hosts */  

int sock_in,sock_out;   /* socket */
unsigned char sbuf[BUFNO][MAXMESG],sbuf_in[MAXMESG],ch_table[65536];
int psize[BUFNO],psize_in,negate_channel,hostlist[N_HOST][2],n_host,no_pinfo,
    n_ch;
char *progname,host_name[100],logfile[256],chfile[256];
int  daemon_mode, syslog_mode;

struct {
    int host;
    int port;
    int no;
    unsigned char nos[256/8];
    unsigned int n_bytes;
    unsigned int n_packets;
    } ht[N_HOST];

get_time(rt)
  int *rt;
  {
  struct tm *nt;
  time_t ltime;
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

  if (syslog_mode)
    syslog(LOG_NOTICE, "%s", ptr);
  else
    {
      if(*logfil) fp=fopen(logfil,"a");
      else fp=stdout;
      get_time(tm);
      fprintf(fp,"%02d%02d%02d.%02d%02d%02d %s %s\n",
	      tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],progname,ptr);
      if(*logfil) fclose(fp);
    }
  }

ctrlc()
  {
  write_log(logfile,"end");
  close(sock_in);
  close(sock_out);
  if (syslog_mode)
    closelog();
  exit(0);
  }

err_sys(ptr)
  char *ptr;
{

  if (syslog_mode)
    syslog(LOG_NOTICE, "%s", ptr);
  else
    {
      perror(ptr);
      write_log(logfile,ptr);
    }
  if(strerror(errno)) write_log(logfile,strerror(errno));
  ctrlc();
  }

read_chfile()
  {
  FILE *fp;
  int i,j,k,ii;
  char tbuf[1024],host_name[80],tb[256];
  struct hostent *h;
  static time_t ltime,ltime_p;

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
      ii=0;
      while(fgets(tbuf,1024,fp))
        {
        *host_name=0;
        if(sscanf(tbuf,"%s",host_name)==0) continue;
        if(*host_name==0 || *host_name=='#') continue;
        if(*tbuf=='*') /* match any channel */
          {
          if(negate_channel) for(i=0;i<65536;i++) ch_table[i]=0;
          else for(i=0;i<65536;i++) ch_table[i]=1;
          }
        else if(n_host==0 && (*tbuf=='+' || *tbuf=='-'))
          {
          if(*tbuf=='+') hostlist[ii][0]=1;   /* allow */
          else hostlist[ii][0]=(-1);          /* deny */
          if(tbuf[1]=='\r' || tbuf[1]=='\n' || tbuf[1]==' ' || tbuf[1]=='\t')
            {
            hostlist[ii][1]=0;                  /* any host */
            if(*tbuf=='+') write_log(logfile,"allow from the rest");
            else write_log(logfile,"deny from the rest");
            }
          else
            {
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
              }
            }
          if(++ii==N_HOST)
            {
            n_host=ii;
            write_log(logfile,"host control table full"); 
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
      sprintf(tb,"%d host rules",n_host);
      write_log(logfile,tb);
      j=0;
      if(negate_channel)
        {
        for(i=0;i<65536;i++) if(ch_table[i]==0) j++;
        if((n_ch=j)==65536) sprintf(tb,"-all channels");
        else sprintf(tb,"-%d channels",n_ch=j);
        }
      else
        {
        for(i=0;i<65536;i++) if(ch_table[i]==1) j++;
        if((n_ch=j)==65536) sprintf(tb,"all channels");
        else sprintf(tb,"%d channels",n_ch=j);
        }
      write_log(logfile,tb);
      fclose(fp);
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
      ntohs(ht[i].port),ht[i].n_packets,ht[i].n_bytes,(ht[i].n_packets+k)/j,
      (ht[i].n_bytes+k)/j);
    write_log(logfile,tb);
    ht[i].n_packets=ht[i].n_bytes=0;
    }
  ltime_p=ltime;
  signal(SIGHUP,(void *)read_chfile);
  }

check_pno(from_addr,pn,pn_f,sock,fromlen,n,nr,nopno) /* returns -1 if duplicated */
  struct sockaddr_in *from_addr;  /* sender address */
  unsigned int pn,pn_f;           /* present and former packet Nos. */
  int sock;                       /* socket */
  int fromlen;                    /* length of from_addr */
  int n;                          /* size of packet */
  int nr;                         /* no resend request if 1 */
  int nopno;                      /* don't check pno */
  {
  int i,j;
  int host_;  /* 32-bit-long host address in network byte-order */
  int port_;  /* port No. in network byte-order */
  unsigned int pn_1;
  static unsigned int mask[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
  unsigned char pnc;
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
        sprintf(tb,"deny packet from host %d.%d.%d.%d:%d",
          ((unsigned char *)&host_)[0],((unsigned char *)&host_)[1],
          ((unsigned char *)&host_)[2],((unsigned char *)&host_)[3],ntohs(port_));
        write_log(logfile,tb);
        }
      return -1;
      }
    }
  if(nopno) return 0;
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
      ((unsigned char *)&host_)[2],((unsigned char *)&host_)[3],ntohs(port_),i);
    write_log(logfile,tb);
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
        pnc=pn_1;
        sendto(sock,&pnc,1,0,(struct sockaddr *)from_addr,fromlen);
        sprintf(tb,"request resend %s:%d #%d",
          inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1);
        write_log(logfile,tb);
#if DEBUG1
        printf("<%d ",pn_1);
#endif
        ht[i].nos[pn_1>>3]&=~mask[pn_1&0x07]; /* reset bit for the packet no */
        } while((pn_1=(++pn_1&0xff))!=pn);
      else
        {
        sprintf(tb,"no request resend %s:%d #%d-#%d",
          inet_ntoa(from_addr->sin_addr),ntohs(from_addr->sin_port),pn_1,pn);
        write_log(logfile,tb);
        }
      }
    }
  if(pn!=pn_f && ht[i].nos[pn_f>>3]&mask[pn_f&0x07])
    {   /* if the resent packet is duplicated, return with -1 */
    if(!no_pinfo)
      {
      sprintf(tb,"discard duplicated resent packet #%d for #%d",pn,pn_f);
      write_log(logfile,tb);
      }
    return -1;
    }
  return 0;
  }

get_packet(bufno,no)
  int bufno;  /* present(next) bufno */
  unsigned char no; /* packet no. to find */
  {
  int i;
  if((i=bufno-1)<0) i=BUFNO-1;
  while(i!=bufno && psize[i]>2)
    {
    if(sbuf[i][0]==no) return i;
    if(--i<0) i=BUFNO-1;
    }
  return -1;  /* not found */
  }

main(argc,argv)
  int argc;
  char *argv[];
  {
  struct timeval timeout,tv1,tv2;
  double idletime;
  int c,i,j,re,fromlen,n,bufno,bufno_f,pts,ttl,delay,noreq,sockbuf,nopno;
  struct sockaddr_in to_addr,from_addr;
  unsigned short to_port;
  struct hostent *h;
  unsigned short host_port,src_port;
  unsigned char no,no_f;
  char tb[256];
  struct ip_mreq stMreq;
  char mcastgroup[256]; /* multicast address */
  char interface[256]; /* multicast interface for receive */
  char sinterface[256]; /* multicast interface for send */
  extern int optind;
  extern char *optarg;
  unsigned long mif; /* multicast interface address */

  if(progname=strrchr(argv[0],'/')) progname++;
  else progname=argv[0];

  daemon_mode = syslog_mode = 0;
  if(strcmp(progname,"relayd")==0) daemon_mode=1;

  if (daemon_mode)
    sprintf(tb,
    " usage : '%s (-Nnr) (-b [sbuf(KB)]) (-d [delay_ms]) (-f [host_file]) (-g [mcast_group]) (-i [interface]) (-h [len(s)]) (-s [sinterface]) (-p [src port]) (-t [ttl]) [in_port] [host] [host_port] ([log file])'\n",
	    progname);
  else
    sprintf(tb,
    " usage : '%s (-NnrD) (-b [sbuf(KB)]) (-d [delay_ms]) (-f [host_file]) (-g [mcast_group]) (-i [interface]) (-h [len(s)]) (-s [sinterface]) (-p [src port]) (-t [ttl]) [in_port] [host] [host_port] ([log file])'\n",
	    progname);


  *chfile=(*interface)=(*mcastgroup)=(*sinterface)=0;
  ttl=1;
  no_pinfo=src_port=delay=noreq=negate_channel=nopno=0;
  sockbuf=256;

  while((c=getopt(argc,argv,"b:Dd:f:g:i:Nnp:rs:t:T:"))!=EOF)
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
      case 'f':   /* host control file */
        strcpy(chfile,optarg);
        break;
      case 'g':   /* multicast group for input (multicast IP address) */
        strcpy(mcastgroup,optarg);
        break;
      case 'i':   /* interface (ordinary IP address) which receive mcast */
        strcpy(interface,optarg);
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
        strcpy(sinterface,optarg);
        break;
      case 'T':   /* ttl for MCAST */
      case 't':   /* ttl for MCAST */
        ttl=atoi(optarg);
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

  to_port=atoi(argv[1+optind]);
  if (to_port == 0 && daemon_mode)
    {
      fprintf(stderr,
	      "daemon mode cannnot active in case of data from STDIN\n");
      exit(1);
    }
  strcpy(host_name,argv[2+optind]);
  host_port=atoi(argv[3+optind]);
  if(argc>4+optind) strcpy(logfile,argv[4+optind]);
  else
    {
      *logfile=0;
      if (daemon_mode)
	syslog_mode = 1;
    }

   /* daemon mode */
   if (daemon_mode) {
     daemon_init(progname, LOG_USER, syslog_mode);
     umask(022);
   }

  sprintf(tb,"start in_port=%d to host '%s' port=%d",
    to_port,host_name,host_port);
  write_log(logfile,tb);

  if(to_port>0)
    {
    /* 'in' port */
    if((sock_in=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("sock_in");
    for(j=sockbuf;j>=16;j-=4)
      {
      i=j*1024;
      if(setsockopt(sock_in,SOL_SOCKET,SO_RCVBUF,(char *)&i,sizeof(i))>=0)
        break;
      }
    if(j<16) err_sys("SO_RCVBUF setsockopt error\n");
    sprintf(tb,"RCVBUF size=%d",j*1024);
    write_log(logfile,tb);

    memset((char *)&to_addr,0,sizeof(to_addr));
    to_addr.sin_family=AF_INET;
    to_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    to_addr.sin_port=htons(to_port);
    if(bind(sock_in,(struct sockaddr *)&to_addr,sizeof(to_addr))<0)
    err_sys("bind_in");

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
  if(!(h=gethostbyname(host_name))) err_sys("can't find host");
  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  memcpy((caddr_t)&to_addr.sin_addr,h->h_addr,h->h_length);
/*  to_addr.sin_addr.s_addr=inet_addr(inet_ntoa(h->h_addr));*/
  to_addr.sin_port=htons(host_port);
  if((sock_out=socket(AF_INET,SOCK_DGRAM,0))<0) err_sys("sock_out");
  i=1;
  if(setsockopt(sock_out,SOL_SOCKET,SO_BROADCAST,(char *)&i,sizeof(i))<0)
    err_sys("SO_BROADCAST setsockopt error\n");
  i=65535;
  if(setsockopt(sock_out,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0)
    {
    i=50000;
    if(setsockopt(sock_out,SOL_SOCKET,SO_SNDBUF,(char *)&i,sizeof(i))<0)
      err_sys("SO_SNDBUF setsockopt error\n");
    }
  if(*sinterface)
    {
    mif=inet_addr(sinterface);
    if(setsockopt(sock_out,IPPROTO_IP,IP_MULTICAST_IF,(char *)&mif,sizeof(mif))<0)
      err_sys("IP_MULTICAST_IF setsockopt error\n");
    }
  if(ttl>1)
    {
    no=ttl;
    if(setsockopt(sock_out,IPPROTO_IP,IP_MULTICAST_TTL,&no,sizeof(no))<0)
      err_sys("IP_MULTICAST_TTL setsockopt error\n");
    }
  /* bind my socket to a local port */
  memset((char *)&from_addr,0,sizeof(from_addr));
  from_addr.sin_family=AF_INET;
  from_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  from_addr.sin_port=htons(src_port);
  if(src_port)
    {
    sprintf(tb,"src_port=%d",src_port);
    write_log(logfile,tb);
    }
  if(bind(sock_out,(struct sockaddr *)&from_addr,sizeof(from_addr))<0)
    err_sys("bind_out");

  if(nopno) write_log(logfile,"packet numbers pass through");
  if(noreq) write_log(logfile,"resend request disabled");
  signal(SIGTERM,(void *)ctrlc);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGPIPE,(void *)ctrlc);
  no=0;
  for(i=0;i<BUFNO;i++) psize[i]=2;
  bufno=0;
  read_chfile();

  while(1)
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
      printf("idletime=%f\n",idletime);
      if(sbuf[bufno][0]==sbuf[bufno][1]) printf("%d ",sbuf[bufno][0]);
      else printf("%d(%d) ",sbuf[bufno][0],sbuf[bufno][1]);
#endif

      if(check_pno(&from_addr,sbuf[bufno][0],sbuf[bufno][1],sock_in,fromlen,
          psize[bufno],noreq,nopno)<0) continue;
      if(delay>0 && idletime>0.5) usleep(delay*1000);
      }
    else /* read from stdin */
      {
      if(fgets(sbuf[bufno]+2,MAXMESG-2,stdin)==NULL) ctrlc();
      if(sbuf[bufno][2]==0x04) ctrlc();
      psize[bufno]=strlen(sbuf[bufno]+2)+2;
      }

    if(!nopno) sbuf[bufno][0]=sbuf[bufno][1]=no;
    re=sendto(sock_out,sbuf[bufno],psize[bufno],0,
	      (const struct sockaddr *)&to_addr,sizeof(to_addr));
#if DEBUG
    for(i=0;i<8;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
    fprintf(stderr," : %d > %d\n",psize[bufno],re);
#endif
#if DEBUG1
    printf(">%d ",no);
#endif
    if(++bufno==BUFNO) bufno=0;
    no++;

    while(1)
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
          sprintf(tb,"resend to %s:%d #%d as #%d, %d B",
            inet_ntoa(to_addr.sin_addr),ntohs(to_addr.sin_port),no_f,no,re);
          write_log(logfile,tb);
#if DEBUG
          for(i=0;i<8;i++) fprintf(stderr,"%02X",sbuf[bufno][i]);
          fprintf(stderr," : %d > %d\n",psize[bufno],re);
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
