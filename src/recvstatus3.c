/* $Id: recvstatus3.c,v 1.8.2.1.2.3 2010/09/29 16:06:34 uehira Exp $ */

/* 
 * recvstatus3 :
 *   receive A8/A9 packets from Datamark LS-8000SH of LS8TEL14/16 firmware
 */

/*
 * 2005-04-26  Initial version.
 * 2005-05-18  close security hall.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
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
#include "udpu.h"
#include "winlib.h"
#include "ls8tel.h"

#define MAXMSG       1025
#define PATHMAX      1024

static const char rcsid[] =
  "$Id: recvstatus3.c,v 1.8.2.1.2.3 2010/09/29 16:06:34 uehira Exp $";

char *progname, *logfile;
int  daemon_mode, syslog_mode;
int  exit_status;


static void usage(void);
static int dir_check(char *);

int main(int, char *[]);

int
main(int argc, char *argv[])
{
  struct conntable  *ct, *ct_top = NULL;
  char *input_port;
  struct sockaddr_storage  ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  socklen_t   fromlen;
  ssize_t     psize;
  int  maxsoc;
  fd_set  rset;
  FILE  *fp;
  int  sockbuf;
  char  *dirtop, dirname[PATHMAX], filename[PATHMAX], *ptname;
  size_t  dsize;
  char  msg[MAXMSG];
  unsigned char  rbuf[MAXMSG], *ptr;
  int  c, chnum;
#if DEBUG
  int  i = 0;
  char host_[NI_MAXHOST];  /* host address */
  char port_[NI_MAXSERV];  /* port No. */
#endif

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  exit_status = EXIT_SUCCESS;
  daemon_mode = syslog_mode = 0;
  if (strcmp(progname, "recvstatus3d") == 0)
    daemon_mode = 1;
  
  sockbuf = DEFAULT_SNDBUF;  /* default socket buffer size in KB */

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

  /* check make directory */
  dirtop = argv[1];
#if HAVE_MKDTEMP
  (void)snprintf(dirname, sizeof(dirname), "%s/LS8000SH_XXXXXX", dirtop);
  if ((ptname = mkdtemp(dirname)) == NULL) {
    (void)fprintf(stderr, "%s: %s\n", strerror(errno), dirname);
    usage();
  }
  /*  (void)fprintf(stderr, "%s\n", ptname); */
  (void)rmdir(ptname);
#else
  (void)fprintf(stderr, "Warning: This program is not safe. Be careful.\n");
  (void)snprintf(dirname, sizeof(dirname),
		 "%s/LS8000SH_%u", dirtop, (unsigned int)getpid());
  if (mkdir(dirname, 0755)) {
    (void)fprintf(stderr, "%s: %s\n", strerror(errno), dirname);
    usage();
  }
  (void)rmdir(dirname);
#endif

  /* input port: number or service name */
  input_port = argv[0];

  /* logfile */
  if (argc > 2) {
    logfile = argv[2];
    fp = fopen(logfile, "a");
    if (fp == NULL) {
      (void)fprintf(stderr, "logfile '%s': %s\n", logfile, strerror(errno));
      exit(1);
    }
    (void)fclose(fp);
  } else {
    logfile = NULL;
    if (daemon_mode)
      syslog_mode = 1;
  }

  /* daemon mode */
  if (daemon_mode) {
    daemon_init(progname, LOG_USER, syslog_mode);
    umask(022);
  }

  (void)snprintf(msg, sizeof(msg), "start in_port=%s, dirname=%s",
		 input_port, argv[1]);
  write_log(msg);

  /* 'in' port of localhost */
  if ((ct_top = udp_accept(input_port, &maxsoc, sockbuf)) == NULL)
    err_sys("udp_accept");
  maxsoc++;

  FD_ZERO(&rset);

  /***** main loop *****/
  for (;;) {
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

      memset(&ss, 0, sizeof(ss));   /* need not ? */
      fromlen = sizeof(ss);
      psize = recvfrom(ct->soc, rbuf, sizeof(rbuf), 0, sa, &fromlen);
      if (psize < 0)
	err_sys("main: recvfrom");
#if DEBUG
      (void)getnameinfo(sa, fromlen,
			host_, sizeof(host_), port_, sizeof(port_),
			NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
      (void)printf("%s : %s : %ld byte(s)\n", host_, port_, psize);
#endif

#if DEBUG
      (void)snprintf(filename, sizeof(filename), "LS8000_PACKET.%04d", i);
      if ((fp = fopen(filename, "w")) == NULL)
	err_sys("fopen");
      (void)fwrite(rbuf, 1, psize, fp);
      (void)fclose(fp);
      i++;
#endif
      if (rbuf[LS8_PID] != 0xA8 && rbuf[LS8_PID] != 0xA9)
	continue;

      /* Logger address */
      ptr = rbuf + LS8_PHDER_LEN + LS8_A89_ADDR;
      chnum = (ptr[0] << 8) + ptr[1];
#if DEBUG
      printf("address: %04X\n", chnum);
#endif

      /* check directory */
      if (snprintf(dirname, sizeof(dirname), "%s/LS8_%04X",
		   dirtop, chnum) >= sizeof(dirname))
	err_sys("snprintf");
      if (dir_check(dirname) < 0)
	err_sys("check_dir");

      /* check sub-directory */
      if (rbuf[LS8_PID] == 0xA8) {
	if (snprintf(dirname, sizeof(dirname), "%s/LS8_%04X/%s",
		     dirtop, chnum, A8_DIR) >= sizeof(dirname))
	  err_sys("snprintf");
      } else if (rbuf[LS8_PID] == 0xA9) {
	if (snprintf(dirname, sizeof(dirname), "%s/LS8_%04X/%s",
		     dirtop, chnum, A9_DIR) >= sizeof(dirname))
	  err_sys("snprintf");
      }
      if (dir_check(dirname) < 0)
	err_sys("check_dir");

      /* set file name */
      ptr = rbuf + LS8_PHDER_LEN + LS8_A89_TIME;
      if (snprintf(filename, sizeof(filename), "%s/%02x%02x%02x.%02x%02x%02x",
		   dirname, ptr[0],  ptr[1],
		   ptr[2],  ptr[3],  ptr[4],  ptr[5])
	  >= sizeof(filename))
	err_sys("snprintf");

      /* save data body */
      if ((fp = fopen(filename, "w")) == NULL)
	err_sys("fopen");
      ptr = rbuf + LS8_PHDER_LEN;
      if (rbuf[LS8_PID] == 0xA8) {
	dsize = fwrite(ptr, 1, LS8_A8_DLEN, fp);
	if (dsize != LS8_A8_DLEN) {
	  (void)snprintf(msg, sizeof(msg),
			 "strange A8 packet: %ld bytes\n", dsize);
	  write_log(msg);
	}
      } else if (rbuf[LS8_PID] == 0xA9) {
	dsize = fwrite(ptr, 1, LS8_A9_DLEN, fp);
	if (dsize != LS8_A9_DLEN) {
	  (void)snprintf(msg, sizeof(msg),
			 "strange A9 packet: %ld bytes\n", dsize);
	  write_log(msg);
	}
      }
      (void)fclose(fp);
    }  /* for (ct = ct_top; ct != NULL; ct = ct->next) (read data) */
  }  /* for (;;) (main loop) */
}

/* check dir exists or not. If doesn't, make it.
 * return : 1: make dir, 0: dir already exists, -1: error */
static int
dir_check(char *path)

{
  struct stat sb;

  if (stat(path, &sb) < 0) {
    if (errno == ENOENT) {  /* if no such dir, make dir */
      if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
	return (-1);
      else
	return (1);
    } else
      return (-1);
  }
  else if (!S_ISDIR(sb.st_mode))
    return (-1);  /* path exists, but not directory */  

  return (0);
}

static void
usage(void)
{

  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage of %s :\n", progname);
  if (daemon_mode)
    (void)fprintf(stderr,
		  "  %s [in_port] [dir] (logfile)\n",
		  progname);
  else
    (void)fprintf(stderr,
		  "  %s (-D) [in_port] [dir] (logfile)\n",
		  progname);

  exit(1);
}
