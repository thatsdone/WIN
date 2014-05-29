/*  sts.c              modified from cmg.c 2012.6.18,7.3 urabe */
/*  2014.5.23-29 urabe */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "udpu.h"
#include "winlib.h"

#define DEBUG     0
#define IOBPORT   20000
#define PERIOD    0.05 /* in seconds */

static const char rcsid[] =
  "$Id: sts.c,v 1.2 2014/05/29 07:21:21 urabe Exp $";

char *progname,*logfile;
int  syslog_mode, exit_status;

int sock;
struct sockaddr_in to_addr,from_addr;

char *kara(tbuf)
  char *tbuf;
  {
  int len,re,fromlen,i,j;
  static char outbuf[1024];
  for(j=0;j<10;j++)
    {   
    re=sendto(sock,tbuf,strlen(tbuf)-1,0,(struct sockaddr *)&to_addr,
        sizeof(to_addr));
    for(i=0;i<100;i++)
      {
      usleep(10000);
      fromlen=sizeof(from_addr);
      if((len=recvfrom(sock,outbuf,1024,0,(struct sockaddr *)&from_addr,&fromlen))>=0)
        {
        outbuf[len]=0;
        return outbuf;
        }
      }
    }
  *outbuf=0;
  return outbuf;
  }

ctrlc()
  {
  kara("1234 bout 0000\n",sock,&to_addr,&from_addr);
  system("/bin/stty -cbreak");
  printf("\n");
  exit(0);
  }

int
main(argc,argv)
  int argc;
  char *argv[];
  {
  fd_set readfds;
  struct timeval timeout;
  int a,c,n,len,fromlen,re,ch1,ch2,ch3,ch4,tm[6],mode,flags;
  struct hostent *h;
  char host_name[256],tbuf[1024],tb[256];
  unsigned short host_port;
  extern int optind;
  extern char *optarg;
  double volts,vpos,vpos0,period;
  time_t rt;

  volts=1.0; /* default value */
  vpos0=0.0;
  mode=0; /* 0:command mode, +1:PLUS mode, -1:MINUS mode */
  period=PERIOD;

  if((progname=strrchr(argv[0],'/'))) progname++;
  else progname=argv[0];
  sprintf(tb," usage : '%s (-mp) (-v [volts(V)]) (-t [period(s)]) [host] ([port])'\n",progname);

  while((c=getopt(argc,argv,"mpt:v:"))!=EOF)
    {
    switch(c)
      {
      case 'm':   /* M mode; send MINUS until pos moves by volts */
        mode=(-1);
        break;
      case 'p':   /* P mode; send PLUS until pos moves by volts */
        mode=1;
        break;
      case 't':
        period=strtod(optarg,NULL);
        break;
      case 'v':
        volts=strtod(optarg,NULL);
        break;
      default:
        fprintf(stderr," option -%c unknown\n",c);
        fprintf(stderr,"%s\n",tb);
        exit(1);
      }
    }
  optind--;
  if(argc<2+optind)
    {
    fprintf(stderr,"%s\n",tb);
    exit(1);
    }
  else if(argc>2+optind) host_port=atoi(argv[2+optind]);
  else host_port=IOBPORT;
  strcpy(host_name,argv[1+optind]);
  if(!(h=gethostbyname(host_name))) perror("can't find host");
  memset((char *)&to_addr,0,sizeof(to_addr));
  to_addr.sin_family=AF_INET;
  memcpy((caddr_t)&to_addr.sin_addr,h->h_addr,h->h_length);
  to_addr.sin_port=htons(host_port);

  /* my socket */
  if((sock=socket(AF_INET,SOCK_DGRAM,0))<0) perror("socket");
  /* bind my socket to a local port */
  memset((char *)&from_addr,0,sizeof(from_addr));
  from_addr.sin_family=AF_INET;
  from_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  from_addr.sin_port=htons(0);
  if(bind(sock,(struct sockaddr *)&from_addr,sizeof(from_addr))<0)
    perror("bind");

  strcpy(tbuf,kara("1234 hello\n",sock,&to_addr,&from_addr)+5);
  strcpy(tbuf,kara("1234 range -26 -26 -26 -26 0 0 0 0\n",
    sock,&to_addr,&from_addr)+5);
  strcpy(tbuf,kara("1234 bout 0000\n",sock,&to_addr,&from_addr)+5);
  system("/bin/stty cbreak"); 
  fcntl(0,F_SETFL,O_NONBLOCK);
  signal(SIGINT,(void *)ctrlc);
  signal(SIGTERM,(void *)ctrlc);
  signal(SIGPIPE,(void *)ctrlc);
  flags=fcntl(sock, F_GETFL);
  n=fcntl(sock,F_SETFL,flags|O_NONBLOCK);
  flags=fcntl(sock, F_GETFL);
  if(mode>0)
    {
    rt=time(NULL);
    kara("1234 bout 10--\n",sock,&to_addr,&from_addr);
    do
      {
      if(sscanf(kara("1234 ain\n",sock,&to_addr,&from_addr),
        "%*s%*s%d%d%d%d",&ch1,&ch2,&ch3,&ch4)>3)
        {
        get_time(tm);
        vpos=(double)ch1*(2.5*11.0/32768.0);
        if(vpos0==0.0) vpos0=vpos;
        printf("\r%02d:%02d:%02d(%ld)  POS[%6.2fV] doing MOT(+)...",
          tm[3],tm[4],tm[5],time(NULL)-rt,vpos);
        fflush(stdout);
        }
      usleep(100000);
      } while(fabs(vpos-vpos0)<volts);
    kara("1234 bout 00--\n",sock,&to_addr,&from_addr);
    }
  if(mode<0)
    {
    rt=time(NULL);
    kara("1234 bout 01--\n",sock,&to_addr,&from_addr);      
    do
      {
      if(sscanf(kara("1234 ain\n",sock,&to_addr,&from_addr),
        "%*s%*s%d%d%d%d",&ch1,&ch2,&ch3,&ch4)>3)
        {
        get_time(tm);
        vpos=(double)ch1*(2.5*11.0/32768.0);
        if(vpos0==0.0) vpos0=vpos;
        printf("\r%02d:%02d:%02d(%ld)  POS[%6.2fV] doing MOT(-)...",
          tm[3],tm[4],tm[5],time(NULL)-rt,vpos);
        fflush(stdout);
        }
      usleep(100000);
      } while(fabs(vpos-vpos0)<volts);
    kara("1234 bout 00--\n",sock,&to_addr,&from_addr);
    }
  else while(1)
    {
    if(sscanf(kara("1234 ain\n",sock,&to_addr,&from_addr),
      "%*s%*s%d%d%d%d",&ch1,&ch2,&ch3,&ch4)<4)
      {
      usleep(10000);
      continue;
      }
    get_time(tm);
    printf("\r%02d:%02d:%02d  POS[%6.2fV]  M/m/p/P/Q ? ",
      tm[3],tm[4],tm[5],(double)ch1*(2.5*11.0/32768.0));
    fflush(stdout);
    usleep(10000);

    re=0;
    if(read(0,&re,1)>0)
      {
      switch(re)
        {
        case 'P':
          printf("\n+ ...");
          fflush(stdout);
          kara("1234 bout 10--\n",sock,&to_addr,&from_addr);
          usleep((int)(period*1000000.0)*10);
          printf("\n");
          kara("1234 bout 00--\n",sock,&to_addr,&from_addr);
          break;
        case 'p':
          printf("\n+ ...");
          fflush(stdout);
          kara("1234 bout 10--\n",sock,&to_addr,&from_addr);
          usleep((int)(period*1000000.0));
          printf("\n");
          kara("1234 bout 00--\n",sock,&to_addr,&from_addr);
          break;
        case 'M':
          printf("\n- ...");
          fflush(stdout);
          kara("1234 bout 01--\n",sock,&to_addr,&from_addr);
          usleep((int)(period*1000000.0)*10);
          printf("\n");
          kara("1234 bout 00--\n",sock,&to_addr,&from_addr);
          break;
        case 'm':
          printf("\n- ...");
          fflush(stdout);
          kara("1234 bout 01--\n",sock,&to_addr,&from_addr);
          usleep((int)(period*1000000.0));
          printf("\n");
          kara("1234 bout 00--\n",sock,&to_addr,&from_addr);
          break;
        case 'E':
        case 'e':
        case 'Q':
        case 'q':
          system("/bin/stty -cbreak"); 
          printf("\n");
          exit(0);
        default:
          break;
        }
      }
    }
  system("/bin/stty -cbreak"); 
  printf("\n");
  }
