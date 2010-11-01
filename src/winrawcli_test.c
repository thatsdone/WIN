/* $Id: winrawcli_test.c,v 1.1.4.2.2.5 2010/11/01 13:16:02 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
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
#include "tcpu.h"

#define MAXMSG       1025

static const char rcsid[] =
  "$Id: winrawcli_test.c,v 1.1.4.2.2.5 2010/11/01 13:16:02 uehira Exp $";

char *progname, *logfile;
int  syslog_mode, exit_status;

static int  daemon_mode;

/* prototypes */
static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  struct sockaddr_storage  ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  socklen_t   salen;
  char  msg[MAXMSG], buf[MAXMSG];
  char  wrbp_buf[WRBP_CLEN];
  int   socknum;
  int   c, i;
  ssize_t  readnum, sendnum;
  int8_t  sec_rec[MIN_SEC];
  uint32_t  rsize;
  uint8_t   rsize_r[4], *rawbuf;
  int     num;
  FILE    *fpsockr, *fpsockw;
#if DEBUG
  char  host_[NI_MAXHOST];  /* host address */
  char  port_[NI_MAXSERV];  /* port No. */
#endif

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;
  logfile = NULL;

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

  if (argc < 2)
    usage();

  /* connect to server */
  memset(&ss, 0, sizeof(ss));   /* need not ? */
  socknum = tcp_connect(argv[0], argv[1], sa, &salen);
  fpsockr = fdopen(socknum, "r");
  fpsockw = fdopen(socknum, "w");
  if ((fpsockr == NULL) || (fpsockw == NULL))
    err_sys("fdopen");
  /*  setvbuf(fpsockr, NULL, _IONBF, 0); */
  setvbuf(fpsockw, NULL, _IONBF, 0);
#if DEBUG
  /* print info */
  (void)getnameinfo(sa, salen, host_, sizeof(host_),
		    port_, sizeof(port_), NI_NUMERICHOST | NI_NUMERICSERV);
  (void)snprintf(msg, sizeof(msg), "connected to %s:%s", host_, port_);
  write_log(msg);
#endif

  /***** read welcome message *****/
  readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
  /*  readnum = recv(socknum, wrbp_buf, sizeof(wrbp_buf), MSG_WAITALL); */
  write_log(wrbp_buf);
  (void)snprintf(msg, sizeof(msg), "num = %zd %s",
		 readnum, (char *)strerror(errno));
  write_log(msg);

  /***** STAT check *****/
  (void)snprintf(wrbp_buf, WRBP_CLEN, "%s", WRBP_STAT);
  sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw);
  /*  sendnum = send(socknum, wrbp_buf, WRBP_CLEN, 0); */
  /*  sleep (5); */
  readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
  /*  readnum = recv(socknum, wrbp_buf, WRBP_CLEN, MSG_WAITALL); */
  write_log(wrbp_buf);
  (void)snprintf(msg, sizeof(msg), "num2 = %zd %s",
		 readnum, (char *)strerror(errno));
  write_log(msg);

  /***** REQ check *****/
  for (i = 0; i < 60; ++i) {
    if ( i % 70 == 7)
      sec_rec[i] = 0;
    else
      sec_rec[i] = 1;
  }
  for (i = 0; i < 60; ++i) {
    (void)snprintf(wrbp_buf, WRBP_CLEN, "%s 06050216.%02d", WRBP_REQ, i);
    memcpy(wrbp_buf + strlen(wrbp_buf) + 1, sec_rec, MIN_SEC);
    sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw);
    readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
    /*  sendnum = send(socknum, wrbp_buf, WRBP_CLEN, 0); */
    /*  readnum = recv(socknum, wrbp_buf, WRBP_CLEN, MSG_WAITALL); */
    num = snprintf(buf, sizeof(buf), "%s %s", WRBP_SIZE, WRBP_OK);
    if (strcmp(wrbp_buf, buf) == 0) {   /* SIZE OK */
      memcpy(rsize_r, wrbp_buf + num + 1, 4);
      /*  memcpy(&rsize, wrbp_buf + num + 1, 4); */
      rsize = (((uint32_t)rsize_r[0] << 24) & 0xff000000) +
	(((uint32_t)rsize_r[1] << 16) & 0xff0000) +
	(((uint32_t)rsize_r[2] <<  8) & 0xff00) +
	((uint32_t)rsize_r[3] & 0xff); 
      (void)snprintf(msg, sizeof(msg), "%s %d", wrbp_buf, rsize);
      write_log(msg);
      
      if ((rawbuf = (uint8_t *)malloc((size_t)rsize)) == NULL) {
	(void)snprintf(msg, sizeof(msg), "malloc: %s",
		       (char *)strerror(errno));
	err_sys(msg);
      }

      /*  fpsock = fdopen(socknum, "r"); */
      readnum = fread(rawbuf, 1, rsize, fpsockr);
      /*  readnum = recv(socknum, rawbuf, rsize, MSG_WAITALL); */
      /*  readnum = recv(socknum, rawbuf, rsize, 0); */
      (void)snprintf(msg, sizeof(msg), "Get data size: %zd", readnum);
      write_log(msg);
      if (readnum != rsize)
	err_sys("read raw data");
      
      fwrite(rawbuf, 1, rsize, stdout);
      
      free(rawbuf);
    } else  /* SIZE ERR */
      write_log(wrbp_buf);
  }
  
  /***** QUIT *****/
  (void)snprintf(wrbp_buf, WRBP_CLEN, "%s", WRBP_QUIT);
  sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw);
  /*  sendnum = send(socknum, wrbp_buf, WRBP_CLEN, 0); */

  /*  sleep (5); */


  
  exit(exit_status);
}

/* print usage & exit */
static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s hostname port\n", progname);
  exit(1);
}
