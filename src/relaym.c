/* $Id: relaym.c,v 1.2 2004/11/11 11:36:51 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <syslog.h>

#include "daemon_mode.h"
#include "subst_func.h"
#include "udpu.h"
#include "win_log.h"


#define MAXMESG   2048
#define N_PACKET  64     /* N of old packets to be requested */  
#define BUFNO     128    /* max. save packet */
#define N_HOST    100    /* max. number of host */
#define N_DHOST   128    /* max. number of destination host */

#define MAXNAMELEN   1025
#define MAXMSG       1025

static char rcsid[] =
  "$Id: relaym.c,v 1.2 2004/11/11 11:36:51 uehira Exp $";

char *progname, *logfile;
int  daemon_mode, syslog_mode;

static ssize_t        psize[BUFNO];
static unsigned char  sbuf[BUFNO][MAXMESG];
static unsigned char  sq[N_DHOST], sq_f[N_DHOST];
static int            sqindx[N_DHOST][BUFNO];

/* destination host info. */
struct hostinfo {
  char  hostname[MAXNAMELEN];
  char  port[MAXNAMELEN];
  char  host_[NI_MAXHOST];  /* host address */
  char  port_[NI_MAXSERV];  /* port No. */
  int   sock;
  int   ID;
  struct sockaddr_storage  ss;
  struct sockaddr  *sa;
  socklen_t   salen;
  struct hostinfo  *next;
};

static void usage(void);
static struct hostinfo * read_param(const char *, int *);
static int check_pno(struct sockaddr *, unsigned int, unsigned int,
		     int, socklen_t, int);

void end_program(int);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  struct conntable  *ct, *ct_top = NULL;
  char *input_port;
  struct sockaddr_storage  ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  socklen_t   fromlen;
  ssize_t     re;
  int  maxsoc, maxsoc1;
  struct hostinfo  *hinf, *hinf_top = NULL;
  fd_set  rset, rset1;
  struct timeval  timeout, tv1, tv2;
  double  idletime;
  unsigned char   no_f;
  int  bufno, bufno_f;
  int  destnum;
  int  c;
  int  delay, noreq;
  char  msg[MAXMSG];
  FILE  *fp;
  int   i, j;

  if (progname = strrchr(argv[0],'/'))
    progname++;
  else
    progname = argv[0];

  daemon_mode = syslog_mode = 0;
  if (strcmp(progname,"relaymd") == 0)
    daemon_mode = 1;

  delay = noreq = 0;
  while ((c = getopt(argc, argv, "Dd:r")) != -1)
    switch (c) {
    case 'D':
      daemon_mode = 1;  /* daemon mode */
      break;
    case 'd':           /* delay time in msec */
      delay = atoi(optarg);
      break;
    case 'r':           /* disable resend request */
      noreq=1;
      break;
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  if (argc < 2)
    usage();

  /* input port: number or service name */
  input_port = argv[0];

  /* destination hostname(s) & port(s) */
  hinf_top = read_param(argv[1], &destnum);
  if (destnum < 1) {
    (void)fprintf(stderr, "No destination host. Bye!\n");
    exit(1);
  }

  /* logfile */
  if (argc > 2) {
    logfile = argv[2];
    fp = fopen(logfile, "a");
    if (fp == NULL) {
      (void)fprintf(stderr, "logfile '%s': %s\n", logfile, strerror(errno));
      exit(1);
    }
  }
  else {
    logfile = NULL;
    if (daemon_mode)
      syslog_mode = 1;
  }

  /* daemon mode */
  if (daemon_mode) {
    daemon_init(progname, LOG_USER, syslog_mode);
    umask(022);
  }

  (void)snprintf(msg, sizeof(msg), "start in_port=%s, prm_file=%s",
		 input_port, argv[1]);
  write_log(msg);

  /* 'in' port of localhost */
  if ((ct_top = udp_accept(input_port, &maxsoc)) == NULL)
    err_sys("udp_accept");
  /*  printf("maxsoc=%d\n", maxsoc); */
  
  /* 'out' port */
  maxsoc1 = -1;
  for (hinf = hinf_top; hinf != NULL; hinf = hinf->next) {
    hinf->sock = udp_dest(hinf->hostname, hinf->port, hinf->sa, &hinf->salen);
    /*      printf("hinf->sock=%d\n", hinf->sock); */
    if (hinf->sock < 0)
      err_sys("udp_dest");
    (void)getnameinfo(hinf->sa, hinf->salen,
		      hinf->host_, NI_MAXHOST, hinf->port_, NI_MAXSERV,
		      NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
    if (maxsoc1 < hinf->sock)
      maxsoc1 = hinf->sock;
  }

  if (noreq)
    write_log("resend request disabled");

  /* set signal handler */
  signal(SIGTERM, (void *)end_program);
  signal(SIGINT, (void *)end_program);

  for (i = 0; i < N_DHOST; ++i)
    sq[i] = 0;
  for (i = 0; i < BUFNO; ++i)
    psize[i] = 2;
  bufno = 0;
  timeout.tv_sec = timeout.tv_usec = 0;
  for (i = 0; i < N_DHOST; ++i)
    for (j = 0; j < BUFNO; ++j)
      sqindx[i][j] = -1;

  FD_ZERO(&rset);
  FD_ZERO(&rset1);
  maxsoc++;
  maxsoc1++;
  /*    memset(&ss, 0, sizeof(ss)); */
  /*    fromlen = sizeof(ss); */

  /*** main loop ***/
  for (;;) {
    /*  printf("bufno=%d\n", bufno); */
    for (ct = ct_top; ct != NULL; ct = ct->next)
      FD_SET(ct->soc, &rset);
    if (select(maxsoc, &rset, NULL, NULL, NULL) < 0)
      err_sys("select");

    /* read data */
    for (ct = ct_top; ct != NULL; ct = ct->next) {
      if (!FD_ISSET(ct->soc, &rset))
	continue;

      /*  printf("ct->soc = %d\n", ct->soc); */

      (void)gettimeofday(&tv1, NULL);
      memset(&ss, 0, sizeof(ss));   /* need not ? */
      fromlen = sizeof(ss);
      if ((psize[bufno] = recvfrom(ct->soc, sbuf[bufno],
				   MAXMESG, 0, sa, &fromlen)) < 0)
	err_sys("main: recvfrom");
      (void)gettimeofday(&tv2, NULL);
      idletime = (double)(tv2.tv_sec - tv1.tv_sec)
	+ (double)(tv2.tv_usec - tv1.tv_usec) * 0.000001;
      
      /* check packet sequence number */
      if (check_pno(sa, (unsigned int)sbuf[bufno][0],
		    (unsigned int)sbuf[bufno][1],
		    ct->soc, fromlen, noreq) < 0) {   /* duplicated packet */
	(void)snprintf(msg, sizeof(msg), 
		       "discard duplicated resent packet #%d as #%d",
		       sbuf[bufno][0], sbuf[bufno][1]);
	write_log(msg);
      }
      
      /* delay */
      if (delay > 0 && idletime > 0.5)
	(void)usleep(delay * 1000);
      
      /* send data */
      for (hinf = hinf_top; hinf != NULL; hinf = hinf->next) {
	/*  printf("sq[hinf->ID]: sq[%d] = %d\n", hinf->ID, sq[hinf->ID]); */
	sbuf[bufno][0] = sbuf[bufno][1] = sq[hinf->ID];
	if (sendto(hinf->sock, sbuf[bufno],
		   psize[bufno], 0, hinf->sa, hinf->salen) < 0)
	  err_sys("main: sendto");
	
	for (i = 0; i < BUFNO; ++i)
	  if (sqindx[hinf->ID][i] == bufno) {
	    sqindx[hinf->ID][i] = -1;
	    break;
	  }
	sqindx[hinf->ID][sq[hinf->ID]] = bufno;
	sq[hinf->ID]++;
      }
      
      if (++bufno == BUFNO)
	bufno = 0;
    }  /* for (ct = ct_top; ct != NULL; ct = ct->next) */

    /*** accept resend request packet ***/
    /*  printf("Go resend request check!!!!\n\n"); */
    for (;;) {
      for (hinf = hinf_top; hinf != NULL; hinf = hinf->next)
	FD_SET(hinf->sock, &rset1);

      if (select(maxsoc1, &rset1, NULL, NULL, &timeout) > 0) {
	for (hinf = hinf_top; hinf != NULL; hinf = hinf->next) {
	  if (!FD_ISSET(hinf->sock, &rset1))
	    continue;
	  if (recvfrom(hinf->sock, sbuf[bufno],
		       MAXMESG, 0, hinf->sa, &hinf->salen) != 1)
	    continue;

	  no_f = sbuf[bufno][0];
	  if ((bufno_f = sqindx[hinf->ID][no_f]) >= 0) {
	    /*  printf("bufno_f=%d\n", bufno_f); */
	    (void)memcpy(sbuf[bufno], sbuf[bufno_f],
			 psize[bufno] = psize[bufno_f]);
	    sbuf[bufno][0] = sq[hinf->ID];    /* packet no. */
	    sbuf[bufno][1] = no_f;            /* old packet no. */
	    re = sendto(hinf->sock, sbuf[bufno],
			psize[bufno], 0, hinf->sa, hinf->salen);
	    (void)snprintf(msg, sizeof(msg), 
			   "resend to %s:%s #%d(#%d) as #%d, %d B",
			   hinf->host_, hinf->port_,
			   no_f, bufno_f, sq[hinf->ID], re);
	    write_log(msg);
	    
	    for (i = 0; i < BUFNO; ++i)
	      if (sqindx[hinf->ID][i] == bufno) {
		sqindx[hinf->ID][i] = -1;
		break;
	      }	  
	    sqindx[hinf->ID][sq[hinf->ID]] = bufno;
	    sq[hinf->ID]++;
	    if (++bufno == BUFNO)
	      bufno = 0;
	  }
	} /* for (hinf = hinf_top; hinf != NULL; hinf = hinf->next) */
      } else  /* if (select(......) > 0) */
	break;
    }  /* for (;;) */

  }  /* for (;;) (main loop) */
}

void
end_program(int status)
{

  write_log("end");
  if (syslog_mode)
    closelog();
  /*  printf("%d\n", status); */
  exit(status);
}

static void
usage(void)
{

  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage of %s :\n", progname);
  if (daemon_mode)
    (void)fprintf(stderr,
		  "  %s (-d [delay_ms]) [in_port] [param] (logfile)\n",
		  progname);
  else
    (void)fprintf(stderr,
		  "  %s (-D) (-d [delay_ms]) [in_port] [param] (logfile)\n",
		  progname);
  exit(1);
}

static struct hostinfo *
read_param(const char *prm, int *hostnum)
{
  FILE  *fp;
  struct hostinfo  *hinf_top = NULL, *hinf, **hinft = &hinf_top;
  char  buf[MAXMSG], hname[MAXNAMELEN], port[MAXNAMELEN];

  if ((fp = fopen(prm, "r")) == NULL) {
    fprintf(stderr, "%s : %s\n", prm, strerror(errno));
    exit(1);
  }
  
  *hostnum = 0;
  while(fgets(buf, sizeof(buf), fp) != NULL) { 
    if (buf[0] == '#')   /* skip comment */
      continue;
    if (sscanf(buf, "%s %s", hname, port) < 2)
      continue;

    /* save host information */
    if ((hinf = (struct hostinfo *)malloc(sizeof(struct hostinfo))) == NULL)
      continue;
    memset(hinf, 0, sizeof(struct hostinfo));
    (void)strcpy(hinf->hostname, hname);
    (void)strcpy(hinf->port, port);
    hinf->sa = (struct sockaddr *)&hinf->ss;
    hinf->ID = (*hostnum);
    *hinft = hinf;
    hinft = &hinf->next;
    
    (*hostnum)++;
    if (*hostnum >= N_DHOST)
      break;
  }  /* while(fgets(buf, sizeof(buf), fp) */

  (void)fclose(fp);

  return (hinf_top);
}

/*
 * check packet sequence number
 * returns -1 if duplicated
 */
static int
check_pno(struct sockaddr *from_addr, unsigned int pn, unsigned int pn_f,
	  int sock, socklen_t fromlen, int nr)
{
  static int host[N_HOST], no[N_HOST];
  static struct {
    char host[NI_MAXHOST];
    char port[NI_MAXSERV];
    int  no;
    unsigned char nos[256/8];
  } h[N_HOST];
  int i, j;
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port No. */
  unsigned int pn_1;
  static unsigned int 
    mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  unsigned char pnc;
  char  tb[MAXMESG];

  j = (-1);
  if (getnameinfo(from_addr, fromlen,
		  host_, sizeof(host_), port_, sizeof(port_),
		  NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV))
    err_sys("getnameinfo");

  /*  printf("%s:%s\n", host_, port_); */
  for (i = 0; i < N_HOST; i++) {
    if (h[i].host[0] == '\0')
      break;
    if ((strcmp(h[i].host, host_) == 0) && (strcmp(h[i].port, port_) == 0)) {
      j = h[i].no;
      h[i].no = pn;
      h[i].nos[pn >> 3] |= mask[pn & 0x07];  /* set bit for the packet no */
      break;
    }
  }
  if (i == N_HOST) {   /* table is full */
    for (i = 0; i < N_HOST; i++)
      h[i].host[0] = '\0';
    write_log("host table full - flushed.");
    i = 0;
  }

  if (j < 0) {
    (void)strcpy(h[i].host, host_);
    (void)strcpy(h[i].port, port_);
    h[i].no = pn;
    h[i].nos[pn >> 3] |= mask[pn & 0x07]; /* set bit for the packet no */
    (void)snprintf(tb, sizeof(tb), "registered host %s:%s (%d)",
		   host_, port_, i);
    write_log(tb);
  } else {  /* check packet no */
    pn_1 = (j + 1) & 0xff;
    if (!nr && (pn != pn_1)) {
      if (((pn - pn_1) & 0xff) < N_PACKET)
	do {   /* send request-resend packet(s) */
	  pnc = pn_1;
	  if (sendto(sock, &pnc, 1, 0, from_addr, fromlen) < 0)
	    err_sys("check_pno: sendto");
	  (void)snprintf(tb, sizeof(tb), "request resend %s:%s #%d",
			 host_, port_, pn_1);
	  write_log(tb);
	  /* reset bit for the packet no */
	  h[i].nos[pn_1 >> 3] &= ~mask[pn_1 & 0x07];
	} while ((pn_1 = (++pn_1 & 0xff)) != pn);
      else {
	(void)snprintf(tb, sizeof(tb), "no request resend %s:%s #%d-#%d",
		       host_, port_, pn_1, pn);
	write_log(tb);
      }
    }
  }   /* if (j < 0) */

  /* if the resent packet is duplicated, return with -1 */
  if (pn != pn_f && h[i].nos[pn_f >> 3] & mask[pn_f & 0x07])
    return (-1);

  return (0);
}
