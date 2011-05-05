/* $Id: winrawreq.c,v 1.1.2.4.2.6.2.2 2011/05/05 04:15:59 uehira Exp $ */

/* winrawreq.c -- raw data request client */

/*
 * Copyright (c) 2007 -
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *   2007-07-31 -- 2007-09-14  Initial version.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif
#include "daemon_mode.h"
#include "tcpu.h"
#include "udpu.h"
#include "winlib.h"
#include "winraw_bp.h"

#define DEBUG1       0
#define DEBUG_NET    0

#define N_SERV       100     /* max. number of server list */
#define MTU          1500     /* default MTU */
#define IPV4U          28     /* IPv4 header + UDP header (20 + 8) */
#define IPV6U          48     /* IPv6 header + UDP header (40 + 8) */
        
#define MAXMSG       1025

static const char rcsid[] =
  "$Id: winrawreq.c,v 1.1.2.4.2.6.2.2 2011/05/05 04:15:59 uehira Exp $";


char *progname, *logfile;
int  syslog_mode, exit_status;

static int  daemon_mode;
static char *srvlist_file;    /* server list file name */
static char msg[MAXMSG];      /* log message */
static int  srvnum;           /* number of server */
struct {
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
} static srvlist[N_SERV];     /* hostname and port of server */
static int8_t ch_indx[WIN_CHMAX];

static int  stat_check;   /* stat_check */
static int  para_mode;   /* parallel mode */
static int  nflag;   /* nofile output */
static int  oflag;   /* network output mode */
static char ohost[NI_MAXHOST], oport[NI_MAXSERV];  /* output host & port */
static size_t  mtu;

struct reqlist {
  /*  char rawname[12]; */
  int8_t sec[MIN_SEC];
  int8_t indx;
};

/* prototypes */
static void usage(void);
static void read_srvlist(void);
static int mk_reqlist(const char *, const int8_t [], struct reqlist *, char *);
static int do_get_data(const char *, const char *, const char *,
		       const int8_t [], const int);
static int network_output(uint8_t *, uint32_t);
static int reprint_reqfile(const int8_t, const int8_t [],
			   const char *, const int);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  static struct reqlist  reql[N_SERV], *reqlp[N_SERV];
  char  rawname[1024];
  int  nreql;
  int  reprint_flag, unlink_flag;
  pid_t  pid;
  int  status;
  int  i;
  int  c;
  char *ptr;

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;
  stat_check = 0;
  para_mode = 1;
  oflag = 0;
  mtu = MTU;
  nflag = 0;
  reprint_flag = unlink_flag = 0;

  /* read option(s) */
  while ((c = getopt(argc, argv, "b:Dno:rstu")) != -1)
    switch (c) {
    case 'b':
      /* maximum size of IP packet in bytes (or MTU) for network output */
      mtu = (size_t)atol(optarg);
      break;
    case 'n':  /* nofile output mode */
      nflag = 1;
      break;
    case 'D':  /* daemon mode */
      daemon_mode = 1;
      break;
    case 'o':  /* network output */
      oflag = 1;
      if ((ptr = strrchr(optarg, ':')) == NULL) {
	(void)fprintf(stderr," option -o requires 'host:port' pair !\n");
	usage();
      } else {
	*ptr = '\0';
	ptr++;
	(void)strncpy(ohost, optarg, sizeof(ohost));
	(void)strncpy(oport, ptr, sizeof(oport));
      }
      break;
    case 'r':  /* re-print request file */
      reprint_flag = 1;
      break;
    case 's':  /* sequential mode */
      para_mode = 0;
      break;
    case 't':  /* STAT check */
      stat_check = 1;
      break;
    case 'u':  /* unlink request file after job done */
      unlink_flag = 1;
      break;
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  if (argc < 2)
    usage();

  /* logfile */
  if (argc > 2)
    logfile = argv[2];
  else {
    logfile = NULL;
    if (daemon_mode)
      syslog_mode = 1;
  }

  if (para_mode)
    write_log("Parallel mode");
  else
    write_log("Sequential mode");

  if (!nflag)
    write_log("Data output to file(s)");
  if (oflag) {
    (void)snprintf(msg, sizeof(msg), "Data output to %s:%s (MTU=%zu)",
		   ohost, oport, mtu);
    write_log(msg);
  }

  /* set filename in which is listed server name */
  srvlist_file = argv[0];
  read_srvlist();

  /* make server list */
#if DEBUG1
  nreql = mk_reqlist(argv[1], ch_indx, reql, rawname);
  fprintf(stderr, "%s nreql = %d\n", rawname, nreql);
#else
  (void)mk_reqlist(argv[1], ch_indx, reql, rawname);
#endif

  nreql = 0;
  for (i = 0; i < N_SERV; ++i) {
    if (reql[i].indx < 0)
      continue;
    reqlp[nreql] = &reql[i];

#if DEBUG1
    fprintf(stderr, "%d %s_%s: ", i, srvlist[i].host, srvlist[i].port); 
    fprintf(stderr, "REC %s ", rawname);
    for (j = 0; j < MIN_SEC; ++j)
      fprintf(stderr, "%d", (int)reql[i].sec[j]);
    fprintf(stderr, "\n");
#endif

    nreql++;
  }

#if DEBUG1
  fprintf(stderr, "nreql = %d\n", nreql);
  for (i = 0; i < nreql; ++i) {
    fprintf(stderr, "%d ", reqlp[i]->indx);
    fprintf(stderr, "%s_%s: ",
	   srvlist[reqlp[i]->indx].host, srvlist[reqlp[i]->indx].port); 
    fprintf(stderr, "REC %s ", rawname);
    for (j = 0; j < MIN_SEC; ++j)
      fprintf(stderr, "%d", (int)reql[reqlp[i]->indx].sec[j]);
    fprintf(stderr, "\n");
  }
#endif

  /* get data from server(s) */
  if (para_mode) {  /* fork child process */
    for (i = 0; i < nreql; ++i) {
      pid = fork();
      if (pid == -1) {  /* error */
	(void)snprintf(msg, sizeof(msg), "%s", (char *)strerror(errno));
	write_log(msg);
      } else if (pid == 0) {  /* child */
	status = do_get_data(srvlist[reqlp[i]->indx].host,
			     srvlist[reqlp[i]->indx].port,
			     rawname, reql[reqlp[i]->indx].sec, i);
	if (status < 0) {
	  (void)snprintf(msg, sizeof(msg),
			 "%s %s:%s could not connect.",
			 rawname, srvlist[reqlp[i]->indx].host,
			 srvlist[reqlp[i]->indx].port);
	  write_log(msg);
	} else if (status > 0) {
	  (void)snprintf(msg, sizeof(msg),
			 "%s %s:%s request could not finish.",
			 rawname, srvlist[reqlp[i]->indx].host,
			 srvlist[reqlp[i]->indx].port);
	  write_log(msg);
	}

	/* re-print request file */
	if (status && reprint_flag)
	  (void)reprint_reqfile(reqlp[i]->indx, ch_indx, argv[1], i);

	(void)snprintf(msg, sizeof(msg), "%d child ps done", i);
	write_log(msg);

	exit(0);
      }
    }
    
    while (wait(NULL) > 0)  /* wait for finish all child process */
      ;
    if (errno != ECHILD)
      err_sys("wait error");
  } else {  /* do request sequentially */
    for (i = 0; i < nreql; ++i) {
      status = do_get_data(srvlist[reqlp[i]->indx].host,
			   srvlist[reqlp[i]->indx].port,
			   rawname, reql[reqlp[i]->indx].sec, i);
      if (status < 0) {
	(void)snprintf(msg, sizeof(msg),
		       "%s %s:%s could not connect.",
		       rawname, srvlist[reqlp[i]->indx].host,
		       srvlist[reqlp[i]->indx].port);
	write_log(msg);
      } else if (status > 0) {
	(void)snprintf(msg, sizeof(msg),
		       "%s %s:%s request could not finish.",
		       rawname, srvlist[reqlp[i]->indx].host,
		       srvlist[reqlp[i]->indx].port);
	write_log(msg);
      }

      /* re-print request file */
      if (status && reprint_flag)
	(void)reprint_reqfile(reqlp[i]->indx, ch_indx, argv[1], i);
    }  /* for (i = 0; i < nreql; ++i) */
  }  /* if (para_mode) */

  /* unlink request file */
  if (unlink_flag) {
    if (unlink(argv[1]))
      (void)snprintf(msg, sizeof(msg), "unlink %s : %s",
		     argv[1], (char *)strerror(errno));
    else
      (void)snprintf(msg, sizeof(msg), "unlink %s", argv[1]);
    write_log(msg);
  }

  end_program();
}


/* re-print request */
static int
reprint_reqfile(const int8_t srv_indx, const int8_t chindx[],
		const char *reqfile, const int id)
{
  FILE  *fp, *fpout;
  char  tbuf[2048], outname[2048];
  int   ch;

  /* output file name */
  if (snprintf(outname, sizeof(outname),
	       "%s.%d.req", reqfile, id) >= sizeof(outname)) {
    write_log("buffer overrun : re-print request file");
    return (1);
  }
  if ((fpout = fopen(outname, "w")) == NULL) {
    (void)snprintf(msg, sizeof(msg), "re-print request file '%s' : %s",
		   outname, strerror(errno));
    write_log(msg);
    return (1);
  }
  (void)snprintf(msg, sizeof(msg), "%d Re-print request in '%s'",
		 id, outname);
  write_log(msg);

  fp = fopen(reqfile, "r");   /* don't check again */

  /*** read parameter file ***/
  /* raw data name */
  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')   /* skip comment */
      continue;
    break;
  }
  (void)fprintf(fpout, "# This file was automatically made by %s\n",progname);
  (void)fprintf(fpout, "%s", tbuf);

  /* channel part */
  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')   /* skip comment */
      continue;

    (void)sscanf(tbuf, "%x", &ch);
    if (srv_indx == chindx[ch])
        (void)fprintf(fpout, "%s", tbuf);
  }

  (void)fclose(fpout);
  (void)fclose(fp);
  
  return (0);
}


/* Get data from server
 *  return  0 : normal end
 *         -1 : cannnot connect to server.
 *          1 : can connect to server, but error occurred.
 */
static int
do_get_data(const char *host, const char *port, const char *fname,
	    const int8_t sec[], const int id)
{
  struct sockaddr_storage  ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  socklen_t   salen;
  int   socknum;
  ssize_t  readnum, sendnum;
  FILE    *fpsockr, *fpsockw;
  char  buf[WRBP_CLEN], wrbp_buf[WRBP_CLEN];   /* command buffer */
  int   num;
  uint32_t  rsize;
  uint8_t   rsize_r[4], *rawbuf;
  FILE    *fp;
  char    outname[1024];
  int     status;
#if DEBUG
  char  host_[NI_MAXHOST];  /* host address */
  char  port_[NI_MAXSERV];  /* port No. */
#endif

  status = 0;
  memset(&ss, 0, sizeof(ss));   /* need not ? */
  if ((socknum = tcp_connect(host, port, sa, &salen)) < 0)
    return (-1);
  fpsockr = fdopen(socknum, "r");
  fpsockw = fdopen(socknum, "w");
  if ((fpsockr == NULL) || (fpsockw == NULL)) {
    write_log("fdopen");
    return (-1);
  }
  /* no bufferring mode */
  setvbuf(fpsockw, NULL, _IONBF, 0);
  /*  setvbuf(fpsockr, NULL, _IONBF, 0); */
#if DEBUG
  /* print info */
  (void)getnameinfo(sa, salen, host_, sizeof(host_),
		    port_, sizeof(port_), NI_NUMERICHOST | NI_NUMERICSERV);
  (void)snprintf(msg, sizeof(msg), "%d connected to %s:%s", id, host_, port_);
  write_log(msg);
#endif

  /***** read welcome message *****/
  readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
  (void)snprintf(msg, sizeof(msg), "%d %s", id, wrbp_buf);
  write_log(msg);
#if DEBUG
  (void)snprintf(msg, sizeof(msg), "%d num = %zd %s",
		 id, readnum, (char *)strerror(errno));
  write_log(msg);
#endif


  /***** STAT check *****/
  if (stat_check) {
    (void)snprintf(wrbp_buf, WRBP_CLEN, "%s", WRBP_STAT);
    sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw);
    readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
    (void)snprintf(msg, sizeof(msg), "%d %s", id, wrbp_buf);
    write_log(msg);
#if DEBUG
    (void)snprintf(msg, sizeof(msg), "%d num2 = %zd %s",
		   id, readnum, (char *)strerror(errno));
    write_log(msg);
#endif
  }

  /*++++ REQUEST +++*/
  (void)snprintf(wrbp_buf, WRBP_CLEN, "%s %s", WRBP_REQ, fname);
  (void)memcpy(wrbp_buf + strlen(wrbp_buf) + 1, sec, MIN_SEC);
  sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw);
  readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
#if DEBUG
  (void)snprintf(msg, sizeof(msg), "%d num3 = %zd %s",
		 id, readnum, (char *)strerror(errno));
  write_log(msg);
#endif
  if (readnum != WRBP_CLEN) {
    (void)snprintf(msg, sizeof(msg),
		   "%d SIZE packet len. invalid: %zd %s",
		   id, readnum, (char *)strerror(errno));
    write_log(msg);
    (void)fclose(fpsockw);
    (void)fclose(fpsockr);
    return (1);
  }

  num = snprintf(buf, sizeof(buf), "%s %s", WRBP_SIZE, WRBP_OK);
  if (strcmp(wrbp_buf, buf) == 0) {   /* SIZE OK */
    (void)memcpy(rsize_r, wrbp_buf + num + 1, 4);
    rsize = (((uint32_t)rsize_r[0] << 24) & 0xff000000) +
      (((uint32_t)rsize_r[1] << 16) & 0xff0000) +
      (((uint32_t)rsize_r[2] <<  8) & 0xff00) +
      ((uint32_t)rsize_r[3] & 0xff); 
    (void)snprintf(msg, sizeof(msg), "%d %s %d", id, wrbp_buf, rsize);
    write_log(msg);
    
    if ((rawbuf = MALLOC(uint8_t, rsize)) == NULL) {
      (void)snprintf(msg, sizeof(msg), "malloc: %s",
		     (char *)strerror(errno));
      write_log(msg);
      status = 1;
    } else {  /* malloc ok */
      readnum = fread(rawbuf, 1, rsize, fpsockr);
      (void)snprintf(msg, sizeof(msg), "%d Get data size: %zd", id, readnum);
      write_log(msg);
      if (readnum != rsize) {
	write_log("read raw data");
	status = 1;
      } else {  /* data size ok */
	if (oflag) {  /* output to network */
	  if (network_output(rawbuf, rsize))
	    status = 1;
	}
	if (!nflag) {     /* output to file(s) */
	  (void)snprintf(outname, sizeof(outname),
			 "%s-%s-%s", fname, host, port);
	  (void)snprintf(msg, sizeof(msg), "%d %s", id, outname);
	  write_log(msg);
	  if ((fp = fopen(outname, "w")) == NULL) {
	    (void)snprintf(msg, sizeof(msg), "%d %s", id, strerror(errno));
	    write_log(msg);
	    status = 1;
	  } else {
	    if (fwrite(rawbuf, 1, rsize, fp) != rsize)   /* output data */
	      status = 1;
	    (void)fclose(fp);
	  }
	}  /* !oflag */
      }
      FREE(rawbuf);
    }
  } else { /* SIZE ERR */
    write_log(wrbp_buf);
    status = 1;
  }

  /***** QUIT *****/
  /*  (void)snprintf(wrbp_buf, WRBP_CLEN, "%s", WRBP_QUIT); */
  /*  sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw); */
  (void)fclose(fpsockw);
  (void)fclose(fpsockr);

  return (status);
}

/* network output */
static int
network_output(uint8_t *rawbuf, uint32_t rsize)
{
  struct sockaddr_storage  oss;
  struct sockaddr *osa = (struct sockaddr *)&oss;
  socklen_t   osalen;
  int   osock;
  size_t   mss;
  ssize_t  re;
  uint8_t  *sbuf, pnum;
  uint8_t  *ptr, *ptr1, *ptr_limit, *ptr_limit2;
  uint8_t  *ptw, *ptw_size;
  uint32_t  size, gh, sr, gs, uni;
  int    i, jj;
  int    status;
#if DEBUG_NET
  FILE  *fp;
  char  oname[1024];
#endif

  status = 0;
  memset(&oss, 0, sizeof(oss));  /* need not ? */
  if ((osock = udp_dest(ohost, oport, osa, &osalen, NULL)) < 0) {
    write_log("udp_dest");
    return (-1);
  }
  /* set MSS (Max Segment Size) */
  if (osa->sa_family == AF_INET)   /* IPv4 */
    mss = mtu - IPV4U;
  else if (osa->sa_family == AF_INET6)   /* IPv6 */
    mss = mtu - IPV6U;
  else {
    write_log("MTU & MSS");
    return (-1);
  }
#if DEBUG1
  fprintf(stderr, "IP=%d\n", osa->sa_family);
  fprintf(stderr, "MTU=%d MSU=%d\n", mtu, mss);
#endif

  if ((sbuf = MALLOC(uint8_t, mss + 8)) == NULL) {
    /* +8 for overrun by "size" and "time" */
    write_log("malloc");
    return (-1);
  }

  sbuf[2] = 0xA0;  /* packet ID */

  ptw = sbuf + 3;
  pnum = 0;
  ptr = rawbuf;
  ptr_limit = rawbuf + rsize;

  while (ptr < ptr_limit) {
    size = mkuint4(ptr);
    ptr_limit2 = ptr + size;
    size -= 2;  /* blocksize for UDP/IP packet */
    ptr += 4;
    if (size <= mss - (ptw - sbuf)) {
      ptw[0] = (uint8_t)((size >> 8) & 0xff);
      ptw[1] = (uint8_t)(size & 0xff);
      ptw += 2;
      size -= 2;
      (void)memcpy(ptw, ptr, size);
      ptw += size;
      ptr += size;
    } else {
      ptw_size = ptw;
      ptw += 2;
      for (i = 0; i < 6; i++)  /* time */
	*ptw++ = (*ptr++);
      
      jj = 0;
      while (ptr < ptr_limit2) {
	/* not high sampling version */
	gh = mkuint4(ptr);
	sr = gh & 0xfff;
	if ((gh >> 12) & 0xf)
	  gs = ((gh >> 12) & 0xf) * (sr - 1) + 8;
	else
	  gs = (sr >> 1) + 8;
	ptr1 = ptr;
	ptr += gs;

	if (gs + 11 > mss) {
	  write_log("gs too big, or MTA too small");
	  status = 1;
	  continue;
	}
	/* add gs of data to buffer (after sending packet if it is full) */
	if (ptw + gs - sbuf > mss) {
	  if (jj == 0)
	    ptw -= 8;
	  sbuf[0] = sbuf[1] = pnum;  /* packet no. */
	  /*  sbuf[2] = 0xA0; */
	  
	  usleep(1000);
	  /*  sleep(1); */
	  re = sendto(osock, sbuf, ptw - sbuf, 0, osa, osalen);
	  if (re != ptw - sbuf) {
	    write_log("sendto");
	    status = 1;
	  }
#if DEBUG_NET
	  (void)snprintf(oname, sizeof(oname), "NETOUT-%03d", pnum);
	  fp = fopen(oname, "w");
	  fwrite(sbuf, 1, ptw - sbuf, fp);
	  fclose(fp);
	  fprintf(stderr, "pmun=%d\n", pnum);
#endif	    
	  pnum++;
	  
	  ptw = sbuf + 3;
	  for (i = 2; i < 8; ++i)
	    ptw[i] = ptw_size[i];  /* copy TS */
	  ptw_size = ptw;
	  ptw += 2;  /* size */
	  ptw += 6;  /* TS */
	}  /* if (ptw + gs - sbuf > mss) */
	
	(void)memcpy(ptw, ptr1, gs);
	ptw += gs;
	uni = ptw - ptw_size;
	ptw_size[0] = (uint8_t)((uni >> 8) & 0xff);
	ptw_size[1] = (uint8_t)(uni & 0xff);
	jj++;
      }  /* while (ptr < prt_limit2) */
    }  /* if (size <= mss - (ptw - sbuf)) */
  }  /* while (ptr < ptr_limit) */
  
  if (ptw - sbuf > 11) {
    sbuf[0] = sbuf[1] = pnum;  /* packet no. */
    /*  sbuf[2] = 0xA0; */
    re = sendto(osock, sbuf, ptw - sbuf, 0, osa, osalen);
    if (re != ptw - sbuf) {
      write_log("sendto");
      status = 1;
    }
#if DEBUG_NET
    (void)snprintf(oname, sizeof(oname), "NETOUT-%03d", pnum);
    fp = fopen(oname, "w");
    fwrite(sbuf, 1, ptw - sbuf, fp);
    fclose(fp);
    fprintf(stderr, "pmun=%d end\n", pnum);
#endif	    
  }
  
  FREE(sbuf);
  (void)close(osock);

  return (status);
}

/* make request list */
static int
mk_reqlist(const char *fname, const int8_t chindx[],
	   struct reqlist *reql, char *rawname)
{
  FILE  *fp;
  char  tbuf[1024], *ptr;
  int   sec;
  int   i, j, k;
  int   status;
  int8_t  index;

  if ((fp = fopen(fname, "r")) == NULL) {
    (void)snprintf(msg, sizeof(msg), "request file '%s'", fname);
    err_sys(msg);
  }
  
  /* init */
  status = 0;
  for (i = 0; i < N_SERV; ++i) {
    reql[i].indx = -1;
    for (j = 0; j < MIN_SEC; ++j)
      reql[i].sec[j] = 0;
  }
  
  /*** read parameter file ***/
  /* raw data name */
  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')   /* skip comment */
      continue;
    sscanf(tbuf, "%s", rawname);
    break;
  }
  /*  fprintf(stderr, "%s\n", rawname); */

  /* channel part */
  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')   /* skip comment */
      continue;

    if ((ptr = strtok(tbuf, " \t\n")) == NULL)
      continue;
    (void)sscanf(ptr, "%x", &k);
    if (chindx[k] < 0) {
      (void)snprintf(msg, sizeof(msg),
		     "%04X is not appeared in server file", k);
      write_log(msg);
      continue;
    }
    index = chindx[k];
    reql[index].indx = index;
#if DEBUG1
    fprintf(stderr, "%d %d\n", (int)index, (int)reql[index].indx);
#endif

    /* request second array */
    while ((ptr = strtok(NULL, " \t\n")) != NULL) {
      (void)sscanf(ptr, "%d", &sec);
      /*  fprintf(stderr, "%d ", sec); */
      if (reql[index].sec[sec] == 0)
	reql[index].sec[sec] = 1;
    }
    status++;
  }
  (void)fclose(fp);

  return (status);
}

/* read server list file */
static void
read_srvlist(void)
{
  FILE  *fp;
  char  tbuf[2048];
  char  *field_p, *htmp, *ptmp;
  int   hpindx;
  int   i, k;

  if ((fp = fopen(srvlist_file, "r")) == NULL) {
    (void)snprintf(msg, sizeof(msg), "server list file '%s' : %s",
		   srvlist_file, strerror(errno));
    err_sys(msg);
  }

  /* initialize */
  srvnum = 0;
  for (i = 0; i < WIN_CHMAX; ++i)
    ch_indx[i] = -1;

  /*** read parameter file ***/
  while (fgets(tbuf, sizeof(tbuf), fp) != NULL) {
    if (tbuf[0] == '#')   /* skip comment */
      continue;

    /* read server host name & port name */
    field_p = htmp = strtok(tbuf, " \t\n");
    /*  fprintf(stderr, "%s\n", field_p); */
    if ((ptmp = strrchr(field_p, ':')) == NULL)
      err_sys("server list file format error");
    *ptmp = '\0';
    ptmp++;
    /*  fprintf(stderr, "host=%s port=%s\n", htmp, ptmp); */

    /* set server list */
    for (i = 0; i < srvnum; ++i)
      if ((strcmp(htmp, srvlist[i].host) == 0) &&
	  (strcmp(ptmp, srvlist[i].port) == 0))
	break;
    if (i == N_SERV) {  /* table full */
      write_log("server table full");
      break;
    }
    if (i == srvnum) {
      (void)strcpy(srvlist[i].host, htmp);
      (void)strcpy(srvlist[i].port, ptmp);
      srvnum++;
    }
    hpindx = i;

    /*  fprintf(stderr, "srvnum=%d, hpindx=%d\n", srvnum, hpindx); */

    /* channel number */
    while ((field_p = strtok(NULL, " \t\n")) != NULL) {
      (void)sscanf(field_p, "%x", &k);
      /*  fprintf(stderr, "%s %d\n", field_p, k); */
      ch_indx[k] = (int8_t)hpindx;
    }
  }  /* while (fgets(tbuf, sizeof(tbuf), fp) != NULL) */
  (void)fclose(fp);

  /*  (void)signal(SIGHUP, (void *)read_srvlist); */

#if DEBUG1
  for (i = 0; i < WIN_CHMAX; ++i) {
    if (ch_indx[i] < 0)
      continue;
    /*  fprintf(stderr, "%04X  %d\n", i, ch_indx[i]); */
    fprintf(stderr, "%04X  %s %s\n",
	   i, srvlist[ch_indx[i]].host, srvlist[ch_indx[i]].port);

  }
#endif
}

/* print usage & exit */
static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr,
		"Usage : %s [-nrstu] [-o host:port [-b MTU]] srvlist request [logfile]\n",
		progname);
  exit(EXIT_FAILURE);
}
