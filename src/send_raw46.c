/* $Id: send_raw46.c,v 1.7 2014/09/25 10:37:11 uehira Exp $ */
/*
    program "send_raw/send_mon.c"   1/24/94 - 1/25/94,5/25/94 urabe
                                    6/15/94 - 6/16/94
            "sendt_raw/sendt_mon.c" 12/6/94 - 12/6/94
                                    3/15/95-3/26/95 write_log(), read_chfile()
                                    2/29/96  exit if chfile does not open
                                    5/22/96-5/25/96  merged packet
                                    5/24/96  negated channel list
                                    5/28/96  usleep by select
                                    5/31/96  bug fixed
                                    10/22/96 timezone correction
                                    97.6.23  SNDBUF->65535
                                    97.8.5   fgets/sscanf
                                    97.12.23 SNDBUF->65535/50000
                                             logging of requesting host
                                             LITTLE_ENDIAN
                                    98.4.15  eliminate interval control
                                    98.5.11  bugs in resend fixed
                                    98.6.23  shift_sec (only for -DSUNOS4)
                                    99.2.4   moved signal(HUP) to read_chfile()
                                    99.4.20  byte-order-free
                                    99.6.11  setsockopt(SO_BROADCAST)
                                    99.12.3  mktime()<-timelocal()
                                  2000.3.13  "all" mode / options -amrt
                                  2000.4.17 deleted definition of usleep()
                                  2000.4.24 strerror()
                                  2001.8.19 send interval control
                                  2001.11.14 strerror(),ntohs()
                                  2002.1.7  option '-i' to specify multicast IF
                                  2002.1.8  option '-b' for max IP packet size
                                  2002.1.11 bug fixed for '-r'/'-m'
                                  2002.1.12 variable mon deleted
                                  2002.1.12 function shift_hour() deleted
                                  2002.2.28 NW* x10, print nwloop on HUP
                                  2002.3.2  eobsize(auto)
               2002.5.2  improved send interval control (slptime/atm)
               2002.5.2  i<1000 -> 1000000
               2002.5.3  NBUF 128->250
               2002.5.15 standby mode (-w SHMKEY) / packet statistics on HUP
               2002.9.19 added option -T to set TTL (-T ttl)
               2002.11.29 added option -p to set source port (-p src_port)
               2003.4.8 added option -1 for 1 packet/sec mode
               2004.10.26 daemon mode (Uehira)
	       2004.11.26 some systems (exp. Linux), select(2) changes timeout value
	       2005.2.18 option -f for use and write list of requested chs
               2005.2.20 added fclose() in read_chfile()
               2005.9.24 don't resend packet more than once
               2006.9.2 no resend mode (-R)
               2010.9.18 64bit clean? (Uehira)
               2013.9.17 source IP address can be specified by -i
               2013.9.18 send keep-alive packet when '-k sec_keepal'
               2013.9.18 added -W option : send when data flow in SHM (cf.-w)
	       2014.5.13 added -4, -6 option : force family type IPv4 or IPv6. (Uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else				/* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else				/* !HAVE_SYS_TIME_H */
#include <time.h>
#endif				/* !HAVE_SYS_TIME_H */
#endif				/* !TIME_WITH_SYS_TIME */

#include "daemon_mode.h"
#include "winlib.h"
#include "udpu.h"

/* #define DEBUG       0 */
#define DEBUG0      0
#define DEBUG1      0
#define DEBUG2      0		/* -f */
#define TEST_RESEND 0
#define MTU       1500		/* (max of UDP data size) = MTU - IPV[46]U */
#define IPV4U       28          /* IPv4 header + UDP header (20 + 8) */
#define IPV6U       48          /* IPv6 header + UDP header (40 + 8) */
#define RSIZE    10000
/* #define SR_MON      5 */
#define NBUF      250
#define SLPLIMIT  100
#define REQ_TIMO  10		/* timeout (sec) for request */

static const char rcsid[] =
"$Id: send_raw46.c,v 1.7 2014/09/25 10:37:11 uehira Exp $";

static int      sock, raw, tow, all, n_ch, negate_channel, mtu, nbuf, slptime, no_resend;
static ssize_t  psize[NBUF];
static unsigned long n_packets, n_bytes;	/* 64bit ok */
static uint8_w *sbuf[NBUF], ch_table[WIN_CHMAX], rbuf[RSIZE], ch_req[WIN_CHMAX], ch_req_tmp[WIN_CHMAX],
                pbuf[RSIZE];
/* sbuf[NBUF][mtu-28+8] ; +8 for overrun by "size" and "time" */
static char    *chfile, *file_req;
static int     daemon_mode;

char           *progname, *logfile;
int            syslog_mode, exit_status;

/* prototypes */
static int      get_packet(int bufno, uint8_w no);
static void     read_chfile(void);
static void     recv_pkts(int, struct sockaddr *, uint8_w *, int *,
			  int, int *, int *, time_t *, int *, socklen_t);
static void     usage(void);
int             main(int, char *[]);

static int
get_packet(int bufno, uint8_w no)
/* bufno :  present(next) bufno */
/* no    :  packet no. to find */
{
  int             i;

  if ((i = bufno - 1) < 0)
    i = nbuf - 1;
  while (i != bufno && psize[i] > 0) {
    if (sbuf[i][0] == no)
      return (i);
    else if (sbuf[i][1] == no)
      return (-2);		/* already resent */
    if (--i < 0)
      i = nbuf - 1;
  }
  return (-1);			/* not found */
}

static void
read_chfile(void)
{
  FILE           *fp;
  int             i, j, k;
  char            tbuf[1024];
  static time_t   ltime, ltime_p;
  time_t          j_timt, k_timt;

  if (chfile != NULL) {
    if ((fp = fopen(chfile, "r")) != NULL) {
#if DEBUG
      fprintf(stderr, "ch_file=%s\n", chfile);
#endif
      if (negate_channel)
	memset(ch_table, 1, WIN_CHMAX);
      else
	memset(ch_table, 0, WIN_CHMAX);
      i = j = 0;
      while (fgets(tbuf, sizeof(tbuf), fp)) {
	if (*tbuf == '#' || sscanf(tbuf, "%x", &k) < 0)
	  continue;
	k &= 0xffff;
#if DEBUG
	fprintf(stderr, " %04X", k);
#endif
	if (negate_channel) {
	  if (ch_table[k] == 1) {
	    ch_table[k] = 0;
	    j++;
	  }
	} else {
	  if (ch_table[k] == 0) {
	    ch_table[k] = 1;
	    j++;
	  }
	}
	i++;
      }
#if DEBUG
      fprintf(stderr, "\n");
#endif
      n_ch = j;
      if (negate_channel)
	snprintf(tbuf, sizeof(tbuf), "-%d channels", n_ch);
      else
	snprintf(tbuf, sizeof(tbuf), "%d channels", n_ch);
      write_log(tbuf);
      fclose(fp);
    } else {
#if DEBUG
      fprintf(stderr, "ch_file '%s' not open\n", chfile);
#endif
      snprintf(tbuf, sizeof(tbuf), "channel list file '%s' not open", chfile);
      write_log(tbuf);
      write_log("end");
      close(sock);
      exit(1);
    }
  } else {
    memset(ch_table, 1, WIN_CHMAX);
    /* n_ch=i; *//* What for????? Nonsense??? (Uehira) */
    write_log("all channels");
  }
  snprintf(tbuf, sizeof(tbuf), "slptime=%d (LIMIT=%d)", slptime, SLPLIMIT);
  write_log(tbuf);

  time(&ltime);
  j_timt = ltime - ltime_p;
  if (j_timt != 0) {
    k_timt = j_timt / 2;
    if (ltime_p) {
      snprintf(tbuf, sizeof(tbuf),
	       "%lu pkts %lu B in %ld s ( %ld pkts/s %ld B/s ) ",
	       n_packets, n_bytes, (long) j_timt,
	       (n_packets + k_timt) / j_timt, (n_bytes + k_timt) / j_timt);
      write_log(tbuf);
      n_packets = n_bytes = 0;
    }
    ltime_p = ltime;
  }
  signal(SIGHUP, (void *) read_chfile);
}

static void
recv_pkts(int sock, struct sockaddr * to_addr, uint8_w * no,
	  int *bufno, int standby, int *seq_exp, int *n_seq_exp,
	  time_t * time_req, int *req_timo, socklen_t to_addrlen)
{
  FILE           *fp;
  int             i, j, k, bufno_f, seq, n_seq, n_ch_req;
  ssize_t         len, re;
  socklen_t       fromlen;
  struct timeval  timeout;
  struct sockaddr_storage  ss;
  struct sockaddr *from_addr = (struct sockaddr *)&ss;
  /* struct sockaddr_in from_addr; */
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port No. */
  uint8_w         no_f;
  char            tbuf[1024];

  for (;;) {			/* process resend or send request packets */
    k = 1 << sock;
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(sock + 1, (fd_set *) & k, NULL, NULL, &timeout) > 0) {
      fromlen = sizeof(ss);
      len = recvfrom(sock, rbuf, RSIZE, 0, from_addr, &fromlen);
      if (getnameinfo(from_addr, fromlen,
		      host_, sizeof(host_), port_, sizeof(port_),
		      NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV))
	err_sys("getnameinfo");
#if DEBUG1
      fprintf(stderr, "len=%d : ", len);
      for (k = 0; k < 20; k++)
	fprintf(stderr, "%02X", rbuf[k]);
      fprintf(stderr, "\n");
#endif
      if (!no_resend && len == 1 && (bufno_f = get_packet(*bufno, no_f = (*rbuf))) >= 0) {	/* resend request packet */
	memcpy(sbuf[*bufno], sbuf[bufno_f], psize[*bufno] = psize[bufno_f]);
	sbuf[*bufno][0] = *no;	/* packet no. */
	sbuf[*bufno][1] = no_f;	/* old packet no. */
	if (!standby) {
	  re = sendto(sock, sbuf[*bufno], psize[*bufno], 0,
		      to_addr, to_addrlen);
	  if (re >= 0) {
	    n_packets++;
	    n_bytes += re;
	  }
	}
#if DEBUG1
	fprintf(stderr, "%5d>  ", re);
	for (k = 0; k < 20; k++)
	  fprintf(stderr, "%02X", sbuf[*bufno][k]);
	fprintf(stderr, "\n");
#endif
	snprintf(tbuf, sizeof(tbuf), "resend for %s:%s #%d as #%d, %zd B",
		 host_, port_, no_f, *no, re);
	write_log(tbuf);
	if (++(*bufno) == nbuf)
	  *bufno = 0;
	(*no)++;
      } else if (len >= 8 && strncmp((char *) rbuf, "REQ", 4) == 0) {
	/* send request packet (channels list) */
	seq = mkuint2(&rbuf[4]);
	n_seq = mkuint2(&rbuf[6]);
	j = 0;			/* change flag */
	n_ch_req = 0;
	if (len == 8 && seq == 0 && n_seq == 0) {	/* all channels */
	  for (i = 0; i < WIN_CHMAX; i++) {
	    if (ch_req[i] == 0) {
	      ch_req[i] = 1;
	      j = 1;
	    }
	    n_ch_req++;
	  }
	} else {		/* must be len>0 and seq>=0 and n_seq>0 */
	  if (*n_seq_exp == 0)
	    *n_seq_exp = n_seq;
	  if (seq == (*seq_exp)) {
	    for (i = 8; i < len; i += 2)
	      ch_req_tmp[mkuint2(rbuf + i)] = 1;
	    if (seq == n_seq) {
	      for (i = 0; i < WIN_CHMAX; i++) {
		if (ch_req[i] != ch_req_tmp[i]) {
		  ch_req[i] = ch_req_tmp[i];
		  j = 1;
		}
		ch_req_tmp[i] = 0;
		if (ch_req[i])
		  n_ch_req++;
	      }
	      *seq_exp = 1;
	    } else
	      (*seq_exp)++;
	  } else {		/* was not as expected */
	    *seq_exp = 1;
	  }
	}
#if DEBUG2
	printf("received channel list from %s:%s (%d): %s %d/%d\n",
	       host_, port_, n_ch_req, rbuf, seq, n_seq);
#endif
	if (seq == n_seq) {
	  time(time_req);
	  *req_timo = 0;
	}
	if (j) {
	  if ((fp = fopen(file_req, "w")) == NULL) {
	    fprintf(stderr, "requested channels file '%s' not open.\n",
		    file_req);
	    exit(1);
	  }
	  for (i = 0; i < WIN_CHMAX; i++)
	    if (ch_req[i])
	      fprintf(fp, "%04X\n", i);
	  fclose(fp);
	  snprintf(tbuf, sizeof(tbuf), "req < %s:%s (%d) > %s",
		   host_, port_, n_ch_req, file_req);
	  write_log(tbuf);
	}
      }
    } else
      break;
  }				/* process resend or send request packets */
}

static void
usage(void)
{

  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  if (daemon_mode)
    fprintf(stderr,
	    " usage : '%s (-1amRrt) (-b [mtu]) (-f [req_file]) (-h [h])\\\n\
   (-i [interface]) (-k [s]) (-s [s]) (-p [src_port]) (-W/w [key]) (-T [ttl])\\\n\
   [shmkey] [dest] [port] ([chfile]/- ([logfile]))'\n"
	    ,progname);
  else
    fprintf(stderr,
	    " usage : '%s (-1aDmRrt) (-b [mtu]) (-f [req_file]) (-h [h])\\\n\
   (-i [interface]) (-k [s]) (-s [s]) (-p [src_port]) (-W/w [key]) (-T [ttl])\\\n\
   [shmkey] [dest] [port] ([chfile]/- ([logfile]))'\n"
	    ,progname);
}


int
main(int argc, char *argv[])
{
  FILE           *fp;
  time_t          watch, time_req, ltime, keepal, keepal_1;
  key_t           shm_key, shw_key;
  uint16_w        uni;
  int             i, j, k, aa, bb, ii, jj, bufno, hours_shift, sec_shift,
                  c, nw, eobsize, eobsize_count, atm, np_keepal, sec_keepal,
                  standby, ttl, single, seq_exp, n_seq_exp, req_timo,
                  send_when_flow;
  ssize_t         re;		/* 64bit */
  uint32_w        size, size2, gs;	/* 64bit */
  unsigned long   c_save, c_save_w;	/* 64bit */
  size_t          shp;		/* 64bit */
  /* struct sockaddr_in to_addr; */
  struct sockaddr_storage  ss;
  struct sockaddr *to_addr = (struct sockaddr *)&ss;
  socklen_t       to_addrlen;
  WIN_ch          ch;
  char           *host_port, *src_port;
  uint8_w        *ptr, *ptr1, *ptr_save, *ptr_lim, *ptw, *ptw_size, no,
                  pbuf2[10];
  char           *host_name;
  char            tbuf[1024];
  struct Shm     *shm, *shw;
  char            interface[256];	/* network interface */
  size_t          mss;
  int             family;   /* family type: AF_UNSPEC, AF_INET, AF_INET6 */

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  tow = all = hours_shift = sec_shift = sec_keepal = 0;
  daemon_mode = syslog_mode = 0;
  exit_status = EXIT_SUCCESS;
  family = AF_UNSPEC;

  if (strcmp(progname, "send_raw46") == 0)
    raw = 1;
  else if (strcmp(progname, "send_raw46d") == 0) {
    raw = 1;
    daemon_mode = 1;
  } else if (strcmp(progname, "send_mon46") == 0)
    raw = 0;
  else if (strcmp(progname, "send_mon46d") == 0) {
    raw = 0;
    daemon_mode = 1;
  } else if (strcmp(progname, "sendt_raw46") == 0) {
    raw = 1;
    tow = 1;
  } else if (strcmp(progname, "sendt_raw46d") == 0) {
    raw = 1;
    tow = 1;
    daemon_mode = 1;
  } else if (strcmp(progname, "sendt_mon46") == 0) {
    raw = 0;
    tow = 1;
  } else if (strcmp(progname, "sendt_mon46d") == 0) {
    raw = 0;
    tow = 1;
    daemon_mode = 1;
  } else
    exit(1);

  *interface = 0;
  file_req = NULL;
  mtu = MTU;
  ttl = 1;
  shw_key = (-1);
  standby = 0;
  src_port = NULL;
  single = 0;
  while ((c = getopt(argc, argv, "146ab:Df:h:i:k:mp:Rrs:tW:w:T:")) != -1) {
    switch (c) {
    case '1':		/* "single" mode */
      single = 1;
      break;
    case '4':   /* IPv4 only */
      family = AF_INET;
      break;
    case '6':   /* IPv6 only */
      family = AF_INET6;
      break;
    case 'a':		/* "all" mode */
      all = tow = 1;
      break;
    case 'b':		/* maximum size of IP packet in bytes (or MTU) */
      mtu = atoi(optarg);
      break;
    case 'D':
      daemon_mode = 1;		/* daemon mode */
      break;
    case 'f':			/* requested channels list file */
      file_req = optarg;
      if ((fp = fopen(file_req, "a")) == NULL) {
	fprintf(stderr, "requested channels file '%s' not open.\n", file_req);
	exit(1);
      }
      fclose(fp);
      break;
    case 'h':			/* time to shift, in hours */
      hours_shift = atoi(optarg);
      break;
    case 'i':	/* interface (ordinary IP address) for send */
      /* strcpy(interface,optarg); */
      if (snprintf(interface, sizeof(interface),
		   "%s", optarg) >= sizeof(interface)) {
	fprintf(stderr, "Buffer overrun.\n");
	exit(1);
      }
      break;
    case 'k':   /* send keep-alive packet every sec_keepal sec */
      sec_keepal = atoi(optarg);
      if (sec_keepal <= 0)
	sec_keepal = 1;
      break;
    case 'm':			/* "mon" mode */
      raw = 0;
      break;
    case 'p':			/* source port */
      src_port = optarg;
      break;
    case 'R':			/* no resend mode */
      no_resend = 1;
      break;
    case 'r':			/* "raw" mode */
      raw = 1;
      break;
    case 's':			/* time to shift, in seconds */
      sec_shift = atoi(optarg);
      break;
    case 't':			/* "tow" mode */
      tow = 1;
      write_log("tow");
      break;
    case 'W':                   /* shm key to watch (send when data flow) */
      shw_key = atol(optarg);
      send_when_flow = 1;
      break;
    case 'w':                   /* shm key to watch (send when no data flow) */
      shw_key = atol(optarg);
      send_when_flow = 0;
      break;
    case 'T':			/* ttl for MCAST */
      ttl = atoi(optarg);
      break;
    default:
      fprintf(stderr, " option -%c unknown\n", c);
      usage();
      exit(1);
    }
  }
  optind--;
  if (argc < 4 + optind) {
    usage();
    exit(1);
  }
  shm_key = atol(argv[1 + optind]);
  /* strcpy(host_name,argv[2+optind]); */
  host_name = argv[2 + optind];
  host_port = argv[3 + optind];
  chfile = NULL;
  if (argc > 4 + optind) {
    if (strcmp("-", argv[4 + optind]) == 0)
      chfile = NULL;
    else {
      if (argv[4 + optind][0] == '-') {
	chfile = argv[4 + optind] + 1;
	negate_channel = 1;
      } else {
	chfile = argv[4 + optind];
	negate_channel = 0;
      }
    }
  }
  if (argc > 5 + optind)
    logfile = argv[5 + optind];
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
  read_chfile();
  if (hours_shift != 0) {
    snprintf(tbuf, sizeof(tbuf), "hours shift = %d", hours_shift);
    write_log(tbuf);
  }
  if (sec_shift != 0) {
    snprintf(tbuf, sizeof(tbuf), "secs shift = %d", sec_shift);
    write_log(tbuf);
  }
  /* shared memory */
  shm = Shm_read(shm_key, "start");

  if (shw_key > 0) {
    shw = Shm_read(shw_key, "watch");
  }

  /* destination host/port */
  if (*interface)
    sock = udp_dest(host_name, host_port,
		    to_addr, &to_addrlen, src_port, family, interface);
  else
    sock = udp_dest(host_name, host_port,
		    to_addr, &to_addrlen, src_port, family, NULL);
  if (sock < 0)
    err_sys("udp_dest");
  /* set MSS (Max Segment Size) */
  if (to_addr->sa_family == AF_INET)        /* IPv4 */
    mss = mtu - IPV4U;
  else if (to_addr->sa_family == AF_INET6)  /* IPv6 */
    mss = mtu - IPV6U;
  else
    err_sys("MTU & MSS : unkown family");
    
  if (file_req != NULL) {
    for (j = DEFAULT_RCVBUF; j >= 16; j -= 4) {
      i = j * 1024;
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &i, sizeof(i)) >= 0)
	break;
    }
    if (j < 16)
      err_sys("SO_RCVBUF setsockopt error");
    snprintf(tbuf, sizeof(tbuf), "RCVBUF size=%d", j * 1024);
    write_log(tbuf);
  }

  /* set multicast options */
  if (judge_mcast(to_addr)) {
    if (*interface)
      mcast_set_outopt(sock, interface, ttl);
    else
      mcast_set_outopt(sock, NULL, ttl);
  }

  signal(SIGPIPE, (void *) end_program);
  signal(SIGINT, (void *) end_program);
  signal(SIGTERM, (void *) end_program);
  for (i = 0; i < NBUF; i++) {	/* allocate buffers */
    psize[i] = (-1);
    if ((sbuf[i] = (uint8_w *) win_xmalloc(mss + 8)) == NULL) {
      snprintf(tbuf, sizeof(tbuf), "malloc failed. nbuf=%d\n", i);
      write_log(tbuf);
      break;
    }
  }
  nbuf = i;
  no = bufno = n_packets = n_bytes = np_keepal = 0;
  if (file_req != NULL) {
    memset(ch_req, 0, WIN_CHMAX);
    memset(ch_req_tmp, 0, WIN_CHMAX);
  } else
    memset(ch_req, 1, WIN_CHMAX);
  keepal_1 = time(NULL);

reset:
  while (shm->r == (-1))
    sleep(1);
  if (shw_key > 0) {
    c_save_w = shw->c;
    watch = time(NULL);
    standby = 1;
    snprintf(tbuf, sizeof(tbuf),
	     "watching shm_key=%ld - enter standby", shw_key);
    write_log(tbuf);
    sleep(1);
  }
  c_save = shm->c;
  size = mkuint4(ptr_save = shm->d + (shp = shm->r));
  slptime = 1;

  if (mkuint4(ptr_save + size - 4) == size)
    eobsize = 1;
  else
    eobsize = 0;
  eobsize_count = eobsize;
  snprintf(tbuf, sizeof(tbuf), "eobsize=%d", eobsize);
  write_log(tbuf);
  seq_exp = 1;
  n_seq_exp = 0;
  req_timo = 0;

  ptw = pbuf;
  ptw += 2;
  if (!all)
    *ptw++ = 0xA0;

  /* begin main loop */
  for (;;) {
    if (shp + size > shm->pl)
      shp = 0;			/* advance pointer */
    else
      shp += size;
    nw = atm = 0;
    while (shm->p == shp) {
      recv_pkts(sock, to_addr, &no, &bufno, standby, &seq_exp, &n_seq_exp,
		&time_req, &req_timo, to_addrlen);
      if ((sec_keepal > 0) && ((keepal= time(NULL)) >= keepal_1 + sec_keepal)) {
	/* no packets sent in last sec_keepal sec */
        if (np_keepal == n_packets) {
	  /* send a keep-alive packet */
          pbuf2[0] = no; /* packet no. */
          pbuf2[1] = no; /* packet no.(2) */
          pbuf2[2] = 0xff; /* dummy */
          memcpy(sbuf[bufno], pbuf2, 3);
          if (++bufno == nbuf)
	    bufno = 0;
          no++;
	  re = sendto(sock, pbuf2, 3, 0, to_addr, to_addrlen);
          if (re >= 0) {
	    n_packets++;
	    n_bytes += re;
	  }
	}
        keepal_1 = keepal;
        np_keepal = n_packets;
      }
      usleep(10000);
      nw++;
    }
    if (nw > 1 && slptime < SLPLIMIT)
      slptime *= 2;
    else if (nw == 0 && slptime > 1)
      slptime /= 2;
    i = shm->c - c_save;
    if (!(i < 1000000 && i >= 0) || mkuint4(ptr_save) != size) {
      /* previous block has been destroyed */
      write_log("reset");
      goto reset;
    }
    c_save = shm->c;

    if (shw_key > 0) {		/* check standby state */
      if(send_when_flow) {      /* with option -W */
	if (standby) {		/* at standby becase master was working */
	  if (shw->c != c_save_w) {	/* data flow */
	    standby = 0;
            c_save_w = shw->c;
            watch = time(NULL);
            snprintf(tbuf, sizeof(tbuf),
		     "shm_key=%ld working - begin sending", shw_key);
            write_log(tbuf);
	  }
	} else if (time(NULL) > watch + 2) {
          if (shw->c == c_save_w) { /* no data flow */
            standby=1;
	    snprintf(tbuf, sizeof(tbuf),
		     "shm_key=%ld stopped - enter standby", shw_key);
	    write_log(tbuf);
	  }
	  watch = time(NULL);
	  c_save_w = shw->c;
	}
      } else {	/* with option -w */
        if (standby) { /* at standby becase master was working */
          if (time(NULL) > watch + 2) {
            if (shw->c == c_save_w) { /* no data flow */
              standby = 0;
              snprintf(tbuf, sizeof(tbuf),
		       "shm_key=%ld stopped - begin sending", shw_key);
              write_log(tbuf);
	    }
            watch = time(NULL);
            c_save_w = shw->c;
	  }
	} else if (shw->c != c_save_w) {  /* data flow */
          standby = 1;
          c_save_w = shw->c;
          watch = time(NULL);
          snprintf(tbuf, sizeof(tbuf),
		   "shm_key=%ld working - enter standby", shw_key);
          write_log(tbuf);
	}
      }
    }
    size = mkuint4(ptr_save = ptr = shm->d + shp);

    if (size == mkuint4(shm->d + shp + size - 4)) {
      if (++eobsize_count == 0)
	eobsize_count = 1;
    } else
      eobsize_count = 0;
    if (eobsize && eobsize_count == 0)
      goto reset;
    if (!eobsize && eobsize_count > 3)
      goto reset;
    if (eobsize)
      size2 = size - 4;
    else
      size2 = size;

    ptr_lim = ptr + size2;
    ptr += 4;
    if (tow)
      ptr += 4;

    if (all) {
      if (2 + size2 - 8 > RSIZE)
	continue;		/* too large a block */
      memcpy(ptw, ptr, size2 - 8);
      ptw += size2 - 8;
      for (atm += (slptime - 1); atm >= 100; atm -= 100)
	usleep(10000);
      pbuf[0] = no;		/* packet no. */
      pbuf[1] = no;		/* packet no.(2) */
      memcpy(sbuf[bufno], pbuf, psize[bufno] = ptw - pbuf);
      if (++bufno == nbuf)
	bufno = 0;
      no++;
      if (!standby) {
	re = sendto(sock, pbuf, ptw - pbuf, 0, to_addr, to_addrlen);
	if (re >= 0) {
	  n_packets++;
	  n_bytes += re;
	}
      }
#if DEBUG1
      fprintf(stderr, "%5d>  ", re);
      for (k = 0; k < 20; k++)
	fprintf(stderr, "%02X", pbuf[k]);
      fprintf(stderr, "\n");
#endif
      ptw = pbuf;
      ptw += 2;
      continue;
    }
    ptw_size = ptw;
    ptw += 2;			/* size */
    for (i = 0; i < 6; i++)
      *ptw++ = (*ptr++);	/* time */
    if (hours_shift)
      shift_sec(ptw - 6, hours_shift * 3600);
    if (sec_shift)
      shift_sec(ptw - 6, sec_shift);
#if DEBUG0
    for (i = 2; i < 8; i++)
      fprintf(stderr, "%02X", ptw_size[i]);
    fprintf(stderr, " : %d %d\n", size2, ptw_size - pbuf);
#endif
    /* send data packets */
    ii = jj = re = 0;
    while (ptr < ptr_lim) {
      /* obtain gs(group size) and advance ptr by gs */
      if (raw) {
	gs = get_sysch(ptr1 = ptr, &ch);
	ptr += gs;
      } else {			/* mon */
	ch = mkuint2(ptr1 = ptr);
	ptr += 2;
	for (i = 0; i < SR_MON; i++) {
	  aa = (*(ptr++));
	  bb = aa & 3;
	  if (bb)
	    for (k = 0; k < bb * 2; k++)
	      ptr++;
	  else
	    ptr++;
	}
	gs = ptr - ptr1;
      }
      /* add gs of data to buffer (after sending packet if it is full) */
      if (ch_table[ch] && ch_req[ch] && gs + 11 <= mss) {
	if (ptw + gs - pbuf > mss) {
	  if (jj == 0)
	    ptw -= 8;
      send_packet:
	  for (atm += (slptime - 1); atm >= 100; atm -= 100)
	    usleep(10000);
	  pbuf[0] = no;		/* packet no. */
	  pbuf[1] = no;		/* packet no.(2) */
	  memcpy(sbuf[bufno], pbuf, psize[bufno] = ptw - pbuf);
	  if (++bufno == nbuf)
	    bufno = 0;
	  no++;
#if TEST_RESEND
	  if (no % 10 != 9) {
#endif
	    if (!standby) {
	      re = sendto(sock, pbuf, ptw - pbuf, 0, to_addr, to_addrlen);
	      if (re >= 0) {
		n_packets++;
		n_bytes += re;
	      }
	    }
#if DEBUG1
	    fprintf(stderr, "%5d>  ", re);
	    for (k = 0; k < 20; k++)
	      fprintf(stderr, "%02X", pbuf[k]);
	    fprintf(stderr, "\n");
#endif
#if TEST_RESEND
	  }
#endif

	  recv_pkts(sock, to_addr, &no, &bufno, standby, &seq_exp, &n_seq_exp,
		    &time_req, &req_timo, to_addrlen);

	  time(&ltime);
	  if (file_req != NULL
	      && time_req && !req_timo && ltime - time_req > REQ_TIMO) {
	    memset(ch_req, 0, WIN_CHMAX);
	    req_timo = 1;
	    write_log("request timeout");
	  }
	  ptw = pbuf;
	  for (k = 2; k < 8; k++)
	    ptw[3 + k] = ptw_size[k];	/* copy TS */
	  ptw += 2;		/* pnos */
	  *ptw++ = 0xA0;
	  ptw_size = ptw;
	  ptw += 2;		/* size */
	  ptw += 6;		/* time */
	}
	memcpy(ptw, ptr1, gs);
	ptw += gs;
	uni = ptw - ptw_size;
	ptw_size[0] = uni >> 8;
	ptw_size[1] = uni;
	jj++;
      }
      ii++;
      if (single && ptr >= ptr_lim && ptw - pbuf > 11) {
	gs = 0;
	goto send_packet;
      }
    }
#if DEBUG0
    fprintf(stderr, "\007");
    fprintf(stderr, " %d/%d %d\n", jj, ii, ptw - ptw_size);	/* nch_sent/nch */
#endif
    if ((uni = ptw - ptw_size) > 8) {
      ptw_size[0] = uni >> 8;
      ptw_size[1] = uni;
    } else
      ptw = ptw_size;
  }  /* for (;;) */
}
