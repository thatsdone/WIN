/* $Id: relaym.c,v 1.4 2004/11/26 13:55:38 uehira Exp $ */

/* 2004/11/26 MF relay.c: 
 *   - check_pno() and read_chfile() brought from relay.c
 *   - with host contol but without channel control (-f)
 *   - write host statistics on HUP signal
 *   - no packet info (-n)
 *   - maximize size of receive socket buffer (-b)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

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
#define WINCHNUM  65536  /* max. number of win channels : 2^16 (2 bytes) */

#define MAXNAMELEN   1025
#define MAXMSG       1025

static char rcsid[] =
  "$Id: relaym.c,v 1.4 2004/11/26 13:55:38 uehira Exp $";

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

char *progname, *logfile;
int  daemon_mode, syslog_mode;

static ssize_t        psize[BUFNO];
static unsigned char  sbuf[BUFNO][MAXMESG];
static unsigned char  sq[N_DHOST], sq_f[N_DHOST];
static int            sqindx[N_DHOST][BUFNO];
static char           chfile[MAXNAMELEN];
static int            no_pinfo, n_host, negate_channel;
static unsigned char  ch_table[WINCHNUM];

struct {
  char  host_[NI_MAXHOST];  /* host address */
  int   f;
} static hostlist[N_HOST];

struct {
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
  int  no;
  unsigned char nos[256/8];
  unsigned int n_bytes;
  unsigned int n_packets;
} static ht[N_HOST];

static void usage(void);
static struct hostinfo * read_param(const char *, int *);
static int check_pno(struct sockaddr *, unsigned int, unsigned int,
		     int, socklen_t, int, int);
static void read_chfile(void);

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
  int  delay, noreq, sockbuf;
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

  chfile[0] = '\0';
  delay = noreq = no_pinfo = negate_channel = 0;
  sockbuf = 256;  /* default socket buffer size in KB */

  while ((c = getopt(argc, argv, "b:Dd:f:nr")) != -1)
    switch (c) {
    case 'b':           /* preferred socket buffer size (KB) */
      sockbuf=atoi(optarg);
      break;
    case 'D':
      daemon_mode = 1;  /* daemon mode */
      break;
    case 'd':           /* delay time in msec */
      delay = atoi(optarg);
      break;
    case 'f':   /* host control file */
      strcpy(chfile, optarg);
      break;
    case 'n':           /* supress info on abnormal packets */
      no_pinfo=1;
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
    (void)fclose(fp);
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
  if ((ct_top = udp_accept(input_port, &maxsoc, sockbuf)) == NULL)
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
  signal(SIGPIPE,(void *)end_program);

  for (i = 0; i < N_DHOST; ++i)
    sq[i] = 0;
  for (i = 0; i < BUFNO; ++i)
    psize[i] = 2;
  bufno = 0;
  for (i = 0; i < N_DHOST; ++i)
    for (j = 0; j < BUFNO; ++j)
      sqindx[i][j] = -1;

  read_chfile();

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
    if (select(maxsoc, &rset, NULL, NULL, NULL) < 0) {
      if (errno == EINTR)
	continue;
      else
	err_sys("select");
    }

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
		    ct->soc, fromlen, psize[bufno], noreq) < 0)
	continue;
      
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

      timeout.tv_sec = timeout.tv_usec = 0;
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
		  "  %s (-nr) (-b [sbuf(KB)]) (-d [delay_ms]) (-f [host_file]) [in_port] [param] (logfile)\n",
		  progname);
  else
    (void)fprintf(stderr,
		  "  %s (-Dnr) (-b [sbuf(KB)]) (-d [delay_ms]) (-f [host_file]) [in_port] [param] (logfile)\n",
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
 * returns -1 if duplicated or data from deny host
 */
static int
check_pno(struct sockaddr *from_addr, unsigned int pn, unsigned int pn_f,
	  int sock, socklen_t fromlen, int n, int nr)
     /* struct sockaddr_in *from_addr;  sender address */
     /* unsigned int pn,pn_f;           present and former packet Nos. */
     /* int sock;                       socket */
     /* int fromlen;                    length of from_addr */
     /* int n;                          size of packet */
     /* int nr;                         no resend request if 1 */
{
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

  for (i = 0; i < n_host; i++) {
    if (hostlist[i].f == 1 &&
	(hostlist[i].host_[0] == '\0' || strcmp(hostlist[i].host_, host_)))
      break;
    if (hostlist[i].f == -1 &&
	(hostlist[i].host_[0] == '\0' || strcmp(hostlist[i].host_, host_))) {
      if (!no_pinfo) {
	(void)snprintf(tb, sizeof(tb),
		       "deny packet from host %s:%s", host_, port_);
	write_log(tb);
      }
      return (-1);
    }
  }
  /*  printf("%s:%s\n", host_, port_); */
  for (i = 0; i < N_HOST; i++) {
    if (ht[i].host[0] == '\0')
      break;
    if ((strcmp(ht[i].host, host_) == 0) && (strcmp(ht[i].port, port_) == 0)) {
      j = ht[i].no;
      ht[i].no = pn;
      ht[i].nos[pn >> 3] |= mask[pn & 0x07];  /* set bit for the packet no */
      ht[i].n_bytes += n;
      ht[i].n_packets++;
      break;
    }
  }
  if (i == N_HOST) {   /* table is full */
    for (i = 0; i < N_HOST; i++)
      ht[i].host[0] = '\0';
    write_log("host table full - flushed.");
    i = 0;
  }

  if (j < 0) {
    (void)strcpy(ht[i].host, host_);
    (void)strcpy(ht[i].port, port_);
    ht[i].no = pn;
    ht[i].nos[pn >> 3] |= mask[pn & 0x07]; /* set bit for the packet no */
    (void)snprintf(tb, sizeof(tb), "registered host %s:%s (%d)",
		   host_, port_, i);
    write_log(tb);
    ht[i].n_bytes = n;
    ht[i].n_packets = 1;
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
	  ht[i].nos[pn_1 >> 3] &= ~mask[pn_1 & 0x07];
	} while ((pn_1 = (++pn_1 & 0xff)) != pn);
      else {
	(void)snprintf(tb, sizeof(tb), "no request resend %s:%s #%d-#%d",
		       host_, port_, pn_1, pn);
	write_log(tb);
      }
    }
  }   /* if (j < 0) */

  /* if the resent packet is duplicated, return with -1 */
  if (pn != pn_f && ht[i].nos[pn_f >> 3] & mask[pn_f & 0x07]) {
    if (!no_pinfo) {
      (void)snprintf(tb, sizeof(tb),
		     "discard duplicated resent packet #%d as #%d",
		     pn, pn_f);
      write_log(tb);
    }
    return (-1);
  }

  return (0);
}

/*
 * read host & channel file
 */
static void
read_chfile(void)
{
  FILE  *fp;
  int   i, ii, j, k;
  char  tbuf[1024], host_name[NI_MAXHOST];
  struct addrinfo  hints, *res, *ai;
  int    gai_error;
  char   tb[MAXMESG], tb1[MAXMESG];
  char   sbuf[NI_MAXSERV];
  static time_t  ltime, ltime_p;

  n_host = 0;
  if (chfile[0] != '\0') {
    if ((fp = fopen(chfile, "r")) == NULL) {
      (void)snprintf(tb, sizeof(tb),
		     "channel list file '%s' not open", chfile);
      write_log(tb);
      end_program(1);
    }

    if (negate_channel)
      for (i = 0; i < WINCHNUM; i++) ch_table[i] = 1;
    else
      for (i = 0; i < WINCHNUM; i++) ch_table[i] = 0;
    ii = 0;

    while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
      if (tbuf[0] == '#') continue;   /* skip comment line */
      tb1[0] = '\0';
      if (sscanf(tbuf, "%s", tb1) == 0) continue;
      if (tb1[0] == '\0') continue;

      if (tbuf[0] == '*') {   /* match any channel */
	if (negate_channel)
	  for (i = 0; i < WINCHNUM; i++) ch_table[i] = 0;
	else
	  for (i = 0; i < WINCHNUM; i++) ch_table[i] = 1;
      } else if (n_host == 0
		 && (strcmp(tb1, "+") == 0 || strcmp(tb1, "-") == 0)) {
	if (tbuf[0] == '+')
	  hostlist[ii].f = 1;  /* allow */
	else
	  hostlist[ii].f = -1; /* deny */

	hostlist[ii].host_[0] = '\0';  /* any host */
	if (tbuf[0] == '+')
	  write_log("allow from the rest");
	else
	  write_log("deny from the rest");
	if (++ii == N_HOST) {
	  n_host = ii;
	  write_log("host control table full"); 
	}
      } else if (n_host == 0 && (tbuf[0] == '+' || tbuf[0] == '-')) {
	if (sscanf(tbuf + 1, "%s", host_name) > 0) {  /* hostname */
	  /*  printf("%s\n", host_name); */
	  memset(&hints, 0, sizeof(hints));
	  /*  hints.ai_flags = AI_PASSIVE; */
	  hints.ai_family = AF_UNSPEC;
	  hints.ai_socktype = SOCK_DGRAM;
	  hints.ai_protocol = IPPROTO_UDP;
	  gai_error = getaddrinfo(host_name, NULL, &hints, &res);
	  if (gai_error) {
	    (void)snprintf(tb, sizeof(tb),
			   "getaddrinfo : %s : %s",
			   host_name, gai_strerror(gai_error));
	    write_log(tb);
	    continue;
	  }
	  for (ai = res; ai != NULL; ai = ai->ai_next) {
	    (void)getnameinfo(ai->ai_addr, ai->ai_addrlen,
			      hostlist[ii].host_, sizeof(hostlist[ii].host_),
			      sbuf, sizeof(sbuf),
			      NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
	    if (tbuf[0] == '+') {
	      hostlist[ii].f = 1;  /* allow */
	      (void)snprintf(tb, sizeof(tb),
			     "allow from host %s", hostlist[ii].host_);
	    } else {
	      hostlist[ii].f = -1; /* deny */
	      (void)snprintf(tb, sizeof(tb),
			     "deny from host %s", hostlist[ii].host_);
	    }
	    write_log(tb);

	    if(++ii == N_HOST) {
	      n_host = ii;
	      write_log("host control table full"); 
	      break;
	    }
	  }
	  if (res != NULL)
	    freeaddrinfo(res);
	}
      } else {   /* channel control part */
	(void)sscanf(tbuf, "%x", &k);
	k &= 0xffff;
	if (negate_channel)
	  ch_table[k] = 0;
	else
	  ch_table[k] = 1;
      }
    }  /* while (fgets(tbuf, sizeof(tbuf), fp) != NULL) */
    (void)fclose(fp);
	
    if (ii > 0 && n_host == 0)
      n_host = ii;
    (void)snprintf(tb, sizeof(tb),
		   "%d host rules", n_host);
    write_log(tb);

    j = 0;
    if (negate_channel) {
      for (i = 0; i < WINCHNUM; i++)
	if (ch_table[i] == 0)
	  j++;
      if (j == WINCHNUM)
	(void)snprintf(tb, sizeof(tb), "-all channels");
      else
	(void)snprintf(tb, sizeof(tb), "-%d channels", j);
    } else {
      for (i = 0; i < WINCHNUM; i++)
	if (ch_table[i] == 1)
	  j++;
      if (j == WINCHNUM)
	(void)snprintf(tb, sizeof(tb), "all channels");
      else
	(void)snprintf(tb, sizeof(tb), "%d channels", j);
    }
    write_log(tb);
  } else {   /* if (chfile[0] != '\0') */
    for(i = 0; i < WINCHNUM; i++)
      /* If there is no chfile, get all channel */
      ch_table[i] = 1;
    n_host = 0;
    write_log("all channels");
  }   /* if (chfile[0] != '\0') */

  time(&ltime);
  j = ltime - ltime_p;
  k = j / 2;
  if (ht[0].host[0] != '\0') {
    (void)snprintf(tb, sizeof(tb),
		   "statistics in %d s (pkts, B, pkts/s, B/s)", j);
    write_log(tb);
  }
  for (i = 0; i < N_HOST; i++) {  /* print statistics for hosts */
    if (ht[i].host[0] == '\0')
      break;
    (void)snprintf(tb, sizeof(tb),
		   "src %s:%s   %u %u %u %u",
		   ht[i].host, ht[i].port, ht[i].n_packets, ht[i].n_bytes,
		   (ht[i].n_packets + k) / j, (ht[i].n_bytes + k) / j);
    write_log(tb);
    ht[i].n_packets = ht[i].n_bytes = 0;
  }
  ltime_p = ltime;

  signal(SIGHUP, (void *)read_chfile);
}
