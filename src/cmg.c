/* $Id: cmg.c,v 1.3 2014/09/25 10:37:09 uehira Exp $ */
/*  cmg.c              2005.12.14-17,2007.12.26,2010.7.21 urabe */
/*  modify for TK0040A 2013.07.25 miyazaki */
/*  2014.5.22-29 urabe */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "udpu.h"
#include "winlib.h"

#define IOB      "192.168.209.200"
#define IOBPORT   20000
#define IOB30RTA  0
#define TK0040A   1

static const char rcsid[] =
  "$Id: cmg.c,v 1.3 2014/09/25 10:37:09 uehira Exp $";

char *progname,*logfile;
int  syslog_mode, exit_status;

/* prototypes */
static char *kara(char *, int, struct sockaddr_in *, struct sockaddr_in *);
static double get_vol(int);
int main(int, char *[]);

static char *
kara(char *tbuf, int sock, 
      struct sockaddr_in *to_addr, struct sockaddr_in *from_addr)
{
  int len,re,fromlen;
  static char outbuf[1024];

  re=sendto(sock,tbuf,strlen(tbuf)-1,0,(struct sockaddr *)to_addr,
      sizeof(*to_addr));
  fromlen=sizeof(*from_addr);
  if((len=recvfrom(sock,outbuf,1024,0,(struct sockaddr *)from_addr,&fromlen))
    <0) perror("recvfrom");
  outbuf[len]=0;
  return (outbuf);
}

static double
get_vol(int chval)
{
  double v1,v3;

  v3 = (double)chval*(5/(double)1024);
  v1 = (-750*(-3100*v3+1200*(5-v3)+6500)-1200*(6500-3100*v3))/1350000.0;

  return (v1);
}

int
main(int argc, char *argv[])
  {
  int a,c,sock,re,ch1,ch2,ch3,ch4,tm[6];
  struct sockaddr_in to_addr,from_addr;
  struct hostent *h;
  char host_name[256],tbuf[1024],tb[256];
  unsigned short host_port,my_port;
  int model_name;

  if((progname=strrchr(argv[0],'/'))) progname++;
  else progname=argv[0];
  sprintf(tb," usage : '%s (-ulcfnq) (-p myport) [host] ([port])'\n",progname);
  a=0;
  my_port=0;
  logfile="/dev/null";

  while((c=getopt(argc,argv,"cflnuqp:"))!=EOF)
    {
    switch(c)
      {
      case 'c':   /* center */
      case 'C':   /* center */
      case 'f':   /* power off */
      case 'F':   /* power off */
      case 'l':   /* lock */
      case 'L':   /* lock */
      case 'n':   /* power on */
      case 'N':   /* power on */
      case 'u':   /* unlock */
      case 'U':   /* unlock */
      case 'q':   /* quit */
      case 'Q':   /* quit */
        a=c;
        break;
      case 'p':   /* specify my port */
        my_port=atoi(optarg);
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
  sock = udp_accept4(my_port, DEFAULT_RCVBUF, NULL);
  if(my_port){
    snprintf(tb,sizeof(tb),"kara_port=%d my_src_port=%d",
      host_port,my_port);
  } else {
    snprintf(tb,sizeof(tb),"kara_port=%d",host_port);
  }

  strcpy(tbuf,kara("1234 hello\n",sock,&to_addr,&from_addr)+5);
  if (strncmp(tbuf, "IOB30RTA", 8) == 0){
     model_name=IOB30RTA;
     strcat(tb," kara=IOB30RTA");
     strcpy(tbuf,kara("1234 range -26 -26 -26 -26 0 0 0 0\n",
     sock,&to_addr,&from_addr)+5);
     strcpy(tbuf,kara("1234 bout 0000\n",sock,&to_addr,&from_addr)+5);
  } else if (strncmp(tbuf, "HELLO TK0040", 12) == 0){
     model_name=TK0040A;
     strcat(tb," kara=TK0040A");
     strcpy(tbuf,kara("1234 dout 0000\n",sock,&to_addr,&from_addr)+5);
  } else perror("hello");
  logfile="/dev/stdout";
  write_log(tb);
   
  system("/bin/stty cbreak"); 
  fcntl(0,F_SETFL,O_NONBLOCK);

  while(1)
    {
    strcpy(tbuf,kara("1234 ain\n",sock,&to_addr,&from_addr)+5);
    sscanf(kara("1234 ain\n",sock,&to_addr,&from_addr),
      "%*s%*s%d%d%d%d",&ch1,&ch2,&ch3,&ch4);
    get_time(tm);
    switch (model_name)
        {
        case IOB30RTA:
          printf("\r%02d:%02d:%02d  UD[%6.2fV] NS[%6.2fV] EW[%6.2fV] ",
            tm[3],tm[4],tm[5],(double)ch1*(2.5*10.0/32768.0),
            (double)ch2*(2.5*10.0/32768.0),
            (double)ch3*(2.5*10.0/32768.0));
          if((double)ch4*(2.5/32768.0)>0.5) /* LED is ON */
            printf("<LED ON>\033[27m   C/L/U/F/N/Q ? ");
          else printf("<LED OFF>  C/L/U/F/N/Q ? ");
          break;
        case TK0040A:
          printf("\r%02d:%02d:%02d  UD[%6.2fV] NS[%6.2fV] EW[%6.2fV] ",
            tm[3],tm[4],tm[5],(double)get_vol(ch1),
            (double)get_vol(ch2),
            (double)get_vol(ch3));
          if((double)ch4*(5/1024.0)>0.5) /* LED is ON */
            printf("<LED ON>\033[27m   C/L/U/F/N/Q ? ");
          else printf("<LED OFF>  C/L/U/F/N/Q ? ");
          break;
        default:
          break;
        }

/*      printf("\033[7m<LED ON>\033[27m   C/L/U/Q ? ");*/
/*inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port),tbuf);*/
    fflush(stdout);
    usleep(10000);

    if(a) {re=a; goto aaa;}
    re=0;
    if(read(0,&re,1)>0)
      {
aaa:  switch(re)
        {
        case 'F':
        case 'f':
          printf("\nPOWER OFF\n");
          kara("1234 bout ---1\n",sock,&to_addr,&from_addr);
          break;
        case 'N':
        case 'n':
          printf("\nPOWER ON\n");
          kara("1234 bout ---0\n",sock,&to_addr,&from_addr);
          break;
        case 'C':
        case 'c':
          printf("\nCENTRE ...");
          fflush(stdout);
          kara("1234 bout 1---\n",sock,&to_addr,&from_addr);
          sleep(8);
          printf("\n");
          kara("1234 bout 0---\n",sock,&to_addr,&from_addr);
          break;
        case 'L':
        case 'l':
          printf("\nLOCK ...");
          fflush(stdout);
          kara("1234 bout --1-\n",sock,&to_addr,&from_addr);
          sleep(8);
          printf("\n");
          kara("1234 bout --0-\n",sock,&to_addr,&from_addr);
          break;
        case 'U':
        case 'u':
          printf("\nUNLOCK ...");
          fflush(stdout);
          kara("1234 bout -1--\n",sock,&to_addr,&from_addr);
          sleep(8);
          printf("\n");
          kara("1234 bout -0--\n",sock,&to_addr,&from_addr);
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
      if(a) exit(0);
      }
    }
  }
