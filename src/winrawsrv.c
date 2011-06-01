/* $Id: winrawsrv.c,v 1.5 2011/06/01 11:09:22 uehira Exp $ */

/* winrawsrv.c -- raw data request server */

/*
 * Copyright (c) 2006 -
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *   2006-05-02  Initial version.
 *   2008-02-12  STAT : output file size of raw data described in LATEST.
 *                    : bump protocol version up to 1.0.   
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <syslog.h>

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif
#include "daemon_mode.h"
#include "winlib.h"
#include "winraw_bp.h"

#define DEBUG1       0

#define RAW_OLDEST   "OLDEST"
#define RAW_LATEST   "LATEST"
#define RAW_COUNT    "COUNT"
#define RAW_MAX      "MAX"

#define MAXMSG       1024
#define FNAMEMAX     1024

static const char rcsid[] =
  "$Id: winrawsrv.c,v 1.5 2011/06/01 11:09:22 uehira Exp $";

char *progname, *logfile;
int  syslog_mode, exit_status;

static int  daemon_mode;
static char  fmt[8];
static char  msg[MAXMSG + 1];


/* prototypes */
static void usage(void);
static ssize_t writen(int, const void *, size_t);
static int do_request(const char [], const char []);
static uint32_t select_raw(uint8_t [], off_t, uint8_t [], int8_t[]);
static uint32_t mkbsize(const uint8_t []);
int main(int, char *[]);


int
main(int argc, char *argv[])
{
  struct sockaddr_storage  ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  socklen_t   fromlen;
  char  host_[NI_MAXHOST];  /* host address */
  char  port_[NI_MAXSERV];  /* port No. */
  FILE  *fp;
  char  LATEST[MAXMSG + 1], OLDEST[MAXMSG + 1], COUNT[MAXMSG + 1],
    MAX[MAXMSG + 1];
  char  *rdirname;
  char  olfname[FNAMEMAX], lafname[FNAMEMAX], cnfname[FNAMEMAX],
    mxfname[FNAMEMAX];
  char  wrbp_buf[WRBP_CLEN], cmd[WRBP_CLEN];
  int             c;
  ssize_t   recvnum;
  struct stat  fi;
  char   lrawfname[FNAMEMAX];
  off_t  lasize;  /* raw file size of LATEST */

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;
  logfile = NULL;

  if (snprintf(fmt, sizeof(fmt), "%%%ds", MAXMSG) > sizeof(fmt))
    err_sys("Buffer overrun\n");

  /* read option */
  while ((c = getopt(argc, argv, "D")) != -1)
    switch (c) {
    case 'D':
      daemon_mode = 1;  /* daemon mode */
      break;
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  /* set raw directory */
  if (argc < 1)
    usage();
  rdirname = argv[0];

  /* OLDEST */
  if (snprintf(olfname, sizeof(olfname), "%s/%s", rdirname, RAW_OLDEST)
      > sizeof(olfname))
    err_sys("Buffer overrun\n");
  /* LATEST */
  if (snprintf(lafname, sizeof(lafname), "%s/%s", rdirname, RAW_LATEST)
      > sizeof(lafname))
    err_sys("Buffer overrun\n");
  /* COUNT */
  if (snprintf(cnfname, sizeof(cnfname), "%s/%s", rdirname, RAW_COUNT)
      > sizeof(cnfname))
    err_sys("Buffer overrun\n");
  /* MAX */
  if (snprintf(mxfname, sizeof(mxfname), "%s/%s", rdirname, RAW_MAX)
      > sizeof(mxfname))
    err_sys("Buffer overrun\n");

  /*----------------------------------------------------------------------*/
  if (daemon_mode) {  /* daemon mode */
    /* This mode is not supported */
    exit(EXIT_FAILURE);
  } else {           /* foregroud mode or inetd mode */
    syslog_mode = 1;
    daemon_inetd(progname, LOG_USER, syslog_mode);

    /* logging peer name */
    memset(&ss, 0, sizeof(ss));   /* need not ? */
    fromlen = sizeof(ss);
    if (getpeername(0, sa, &fromlen)) {
      (void)snprintf(msg, sizeof(msg), "getpeername: %s", 
		     (char *)strerror(errno));
      err_sys(msg);
    }
    (void)getnameinfo(sa, fromlen, host_, sizeof(host_),
		      port_, sizeof(port_), NI_NUMERICHOST | NI_NUMERICSERV);
    (void)snprintf(msg, sizeof(msg), "connected from %s:%s", host_, port_);
    write_log(msg);

    /* send welcome messgage */
    memset(&ss, 0, sizeof(ss));
    fromlen = sizeof(ss);
    if (getsockname(0, sa, &fromlen)) {
      (void)snprintf(msg, sizeof(msg), "getsockname: %s", 
		     (char *)strerror(errno));
      err_sys(msg);
    }
    (void)getnameinfo(sa, fromlen, host_, sizeof(host_),
		      port_, sizeof(port_), NI_NUMERICHOST | NI_NUMERICSERV);
    (void)snprintf(wrbp_buf, WRBP_CLEN,
		   "Welcome to WIN raw_data backup server (%s:%s, version %s)",
		   host_, port_, WRBP_VERSION);
    (void)writen(1, wrbp_buf, WRBP_CLEN);

    /********** wait request : main loop ***********/
    for (;;) {
      recvnum = recv(0, wrbp_buf, WRBP_CLEN, MSG_WAITALL);
      if (recvnum != WRBP_CLEN)
	break;
#if DEBUG
      write_log(wrbp_buf);
#endif
      if (sscanf(wrbp_buf, "%s", cmd) < 1)
	continue;

      if (strcmp(cmd, WRBP_QUIT) == 0)           /*-- QUIT --*/
	break;
      else if (strcmp(cmd, WRBP_STAT) == 0) {    /*-- STAT --*/
	/* OLDEST */
	if ((fp = fopen(olfname, "r")) == NULL) {
	  (void)snprintf(msg, sizeof(msg), "%s: %s", olfname,
			 (char *)strerror(errno));
	  write_log(msg);
	  (void)snprintf(OLDEST, sizeof(OLDEST), "%s", "NONE");
	} else {
	  (void)fscanf(fp, fmt, &OLDEST);
	  (void)fclose(fp);
	}
	/* LATEST */
	if ((fp = fopen(lafname, "r")) == NULL) {
	  (void)snprintf(msg, sizeof(msg), "%s: %s", lafname,
			 (char *)strerror(errno));
	  write_log(msg);
	  (void)snprintf(LATEST, sizeof(LATEST), "%s", "NONE");
	} else {
	  (void)fscanf(fp, fmt, &LATEST);
	  (void)fclose(fp);

	  /* raw file size of LATET */
	  if (snprintf(lrawfname, sizeof(lrawfname), "%s/%s", rdirname, LATEST)
	      > sizeof(lrawfname))
	    write_log("Buffer overrun");
	  if (stat(lrawfname, &fi)) {
	    (void)snprintf(msg, sizeof(msg), "%s: stat: %s",
			   lafname, (char *)strerror(errno));
	    write_log(msg);
	    lasize = -1;
	  } else
	    lasize = fi.st_size;
	}
	/* COUNT */
	if ((fp = fopen(cnfname, "r")) == NULL) {
	  (void)snprintf(msg, sizeof(msg), "%s: %s", cnfname,
			 (char *)strerror(errno));
	  write_log(msg);
	  (void)snprintf(COUNT, sizeof(COUNT), "%s", "NONE");
	} else {
	  (void)fscanf(fp, fmt, &COUNT);
	  (void)fclose(fp);
	}
	/* MAX */
	if ((fp = fopen(mxfname, "r")) == NULL) {
	  (void)snprintf(msg, sizeof(msg), "%s: %s", mxfname,
			 (char *)strerror(errno));
	  write_log(msg);
	  (void)snprintf(MAX, sizeof(MAX), "%s", "NONE");
	} else {
	  (void)fgets(MAX, sizeof(MAX),fp);
	  MAX[strlen(MAX) - 1] = '\0';  /* remove "\n" */
	  (void)fclose(fp);
	}
	/* reply */
	(void)snprintf(wrbp_buf, WRBP_CLEN,
		       "%s : %s=%s %s=%s (%d) %s=%s %s=%s",
		       rdirname, RAW_OLDEST, OLDEST, RAW_LATEST, LATEST,
		       (int)lasize, RAW_COUNT, COUNT, RAW_MAX, MAX); 
	(void)writen(1, wrbp_buf, WRBP_CLEN);
      }	else if (strcmp(cmd, WRBP_REQ) == 0) {  /*-- REQ --*/
	if (do_request(wrbp_buf + strlen(cmd) + 1, rdirname)) {
	  (void)snprintf(wrbp_buf, WRBP_CLEN, "%s %s", WRBP_SIZE, WRBP_ERR);
	  (void)writen(1, wrbp_buf, WRBP_CLEN);
	}
      }
    }  /*** for (;;) ***/

    /* close TCP connection */
    if (close(0))
      err_sys("close");
  }  /* if (daemon_mode) */

  write_log("end");
  exit(exit_status);
}

/* print usage & exit */
static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s rawdir\n", progname);
  exit(1);
}

static ssize_t
writen(int fd, const void *vptr, size_t n)
{
  size_t      nleft;
  ssize_t     nwritten;
  const char  *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (errno == EINTR)
	nwritten = 0;    /* re-call write() */
      else
	return (-1);     /* error */
    }
    nleft -= nwritten;
    ptr += nwritten;
  }

  return (n);
}

static int
do_request(const char req[], const char rawdir[])
{
  struct stat  fi;
  off_t        rawsize;
  char         wrbp_buf[WRBP_CLEN];
  char         rawpath[FNAMEMAX + 1], rawfile[FNAMEMAX + 1];
  int8_t       sec_req[MIN_SEC];
  uint32_t     send_size;
  uint8_t      ssize_a[4];
  uint8_t      *rawbuf, *rawbuf1;
  uint8_t      *sptr;
  FILE         *fp;
  int          sec_count;
  int          i, num;

  rawbuf = rawbuf1 = NULL;

  /* set rawdata file name */
  if (sscanf(req, fmt, rawfile) < 1)
    return (-1);
  if (snprintf(rawpath, sizeof(rawpath),
	       "%s/%s", rawdir, rawfile) >= FNAMEMAX)
    return (-1);
#if DEBUG
  /*  write_log(rawfile); */
  write_log(rawpath);
#endif

  (void)memcpy(sec_req, req + strlen(req) + 1, MIN_SEC);
#if DEBUG1
  for (i = 0; i < MIN_SEC; ++i) {
    (void)snprintf(msg, sizeof(msg), "%d %d", i, sec_req[i]);
    write_log(msg);
  }
#endif
  
  sec_count = 0;
  for (i = 0; i < MIN_SEC; ++i)
    if (sec_req[i])
      sec_count++;
  if (sec_count == 0) {
    write_log("request sec count 0");
    return (-1);
  }

  /* get rawfile size */
  if (stat(rawpath, &fi)) {
    (void)snprintf(msg, sizeof(msg), "%s: stat: %s",
		   rawpath, (char *)strerror(errno));
    write_log(msg);
    return (-1);
  }
  rawsize = fi.st_size;
#if DEBUG
  (void)snprintf(msg, sizeof(msg), "%s %lld", rawpath, rawsize);
  write_log(msg);
#endif
  if (rawsize == 0) {
    (void)snprintf(msg, sizeof(msg), "%s size is 0 byte", rawpath);
    write_log(msg);
    return (-1);
  }

  /* prepare buffer for rawfile */
  if ((rawbuf = MALLOC(uint8_t, rawsize)) == NULL) {
    (void)snprintf(msg, sizeof(msg), "malloc: %s", (char *)strerror(errno));
    write_log(msg);
    return (-1);
  }

  /* open rawfile */
  if ((fp = fopen(rawpath, "r")) == NULL) {
    (void)snprintf(msg, sizeof(msg), "%s: %s",
		   rawpath, (char *)strerror(errno));
    write_log(msg);
    FREE(rawbuf);
    return (-1);
  }

  /* read rawfile */
  if (fread(rawbuf, 1, (size_t)rawsize, fp) < (size_t)rawsize) {
    write_log("fread: few data");
    (void)fclose(fp);
    FREE(rawbuf);
    return (-1);
  }	  
  (void)fclose(fp);

  /* select data */
  if (sec_count == MIN_SEC) {  /* in case of all data */
    sptr = rawbuf;
    send_size = (size_t)rawsize;
  } else {
    if ((rawbuf1 = MALLOC(uint8_t, rawsize)) == NULL) {
      (void)snprintf(msg, sizeof(msg), "malloc: %s",
		     (char *)strerror(errno));
      write_log(msg);
      /* if cannot malloc rawbuf1, send entire data */
      sptr = rawbuf;
      send_size = (size_t)rawsize;
    } else {
      send_size = select_raw(rawbuf, rawsize, rawbuf1, sec_req);
      if (send_size == 0) {
	(void)snprintf(msg, sizeof(msg), "%s size is 0 byte", rawpath);
	write_log(msg);
	FREE(rawbuf);
	FREE(rawbuf1);
	return (-1);
      }

      sptr = rawbuf1;
    }
  }
  
  /* reply file size */
  ssize_a[0] = (uint8_t)(send_size >> 24);
  ssize_a[1] = (uint8_t)(send_size >> 16);
  ssize_a[2] = (uint8_t)(send_size >>  8);
  ssize_a[3] = (uint8_t)send_size;
  num = snprintf(wrbp_buf, WRBP_CLEN, "%s %s", WRBP_SIZE, WRBP_OK);
  memcpy(wrbp_buf + num + 1, ssize_a, 4);
  if (writen(1, wrbp_buf, WRBP_CLEN) != WRBP_CLEN) {
    (void)snprintf(msg, sizeof(msg), "sendnum = %d, %s",
		   num, (char *)strerror(errno));
    write_log(msg);
  }

  /***** send raw file *****/
  num = writen(1, sptr, (size_t)send_size);
  /*  num = send(1, rawbuf, (size_t)send_size, 0); */
  /*  num = write(1, rawbuf, (size_t)send_size); */
  /*  num = send(1, rawbuf, 14000, 0); */
  /*  num = fwrite(rawbuf, 1, send_size, stdout); fflush(stdout); */
  if (num != send_size) {
    (void)snprintf(msg, sizeof(msg), "sendnum = %d, %s",
		   num, (char *)strerror(errno));
    write_log(msg);
  }
  
  FREE(rawbuf);
  FREE(rawbuf1);

  return (0);
}

static uint32_t
select_raw(uint8_t src[], off_t srcsize, uint8_t dest[], int8_t seclist[])
{
  uint32_t  rsize, bsize;
  uint8_t   *ptr, *ptw, *ptr_limit;
  int       sec;

  rsize = 0;
  ptr = src;
  ptr_limit = src + srcsize;
  ptw = dest;

  while (ptr < ptr_limit) {
    bsize = mkbsize(ptr);
    sec = b2d[ptr[9]];
    if ((sec < 0) || (59 < sec)) {
      write_log("Invalid sec");
      continue;
    }
    if (seclist[sec]) {
      rsize += bsize;
      (void)memcpy(ptw, ptr, (size_t)bsize);
      ptw += bsize;
    }
    ptr += bsize;
  }  /* while (ptr < ptr_limit) */

  return (rsize);
}

static uint32_t
mkbsize(const uint8_t ptr[])
{
  uint32_t   a;

  a = ((ptr[0] << 24) & 0xff000000) + ((ptr[1] << 16) & 0xff0000) +
    ((ptr[2] <<  8) & 0xff00) + (ptr[3] & 0xff); 

  return (a);
}
