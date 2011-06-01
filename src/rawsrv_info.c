/* $Id: rawsrv_info.c,v 1.2 2011/06/01 11:09:21 uehira Exp $ */

/* rawsrv_info.c -- print information about raw-data server */

/*
 * Copyright (c) 2007 -
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *  2010-10-13  64bit check.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "tcpu.h"
#include "winlib.h"
#include "winraw_bp.h"
#include "subst_func.h"

#define N_SERV       100     /* max. number of server list */

#define MAXMSG       1025

static const char rcsid[] =
  "$Id: rawsrv_info.c,v 1.2 2011/06/01 11:09:21 uehira Exp $";

char *progname, *logfile;
int  syslog_mode, exit_status;

static int daemon_mode;
static char *srvlist_file;    /* server list file name */
static int  srvnum;           /* number of server */
struct {
  char host[NI_MAXHOST];
  char port[NI_MAXSERV];
} static srvlist[N_SERV];     /* hostname and port of server */
static char msg[MAXMSG];      /* log message */


/* prototypes */
static void usage(void);
static void read_srvlist(void);
static int get_info(const char *, const char *);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  int   i, c;

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  exit_status = EXIT_SUCCESS;
  syslog_mode = 1;
  daemon_mode = 0;
  logfile = NULL;
  
  /* read option(s) */
  while ((c = getopt(argc, argv, "h")) != -1)
    switch (c) {
    case 'h':
      usage();
      break;
    default:
      usage();
    }
  argc -= optind;
  argv += optind;
  
  if (argc < 1)
    usage();

  /* set filename in which is listed server name */
  srvlist_file = argv[0];
  read_srvlist();

#if DEBUG
  for (i = 0; i < srvnum; ++i)
    (void)printf("%s %s \n", srvlist[i].host, srvlist[i].port);
#endif

  for (i = 0; i < srvnum; ++i)
    if (get_info(srvlist[i].host, srvlist[i].port))
      (void)printf("%s:%s  %s\n", srvlist[i].host, srvlist[i].port,
		   (char *)strerror(errno));

  exit(exit_status);
}

/* Get data from server
 *  return  0 : normal end
 *         -1 : cannnot connect to server.
 *          1 : can connect to server, but error occurred.
 */
static int
get_info(const char *host, const char *port)
{
  struct sockaddr_storage  ss;
  struct sockaddr *sa = (struct sockaddr *)&ss;
  socklen_t   salen;
  int   socknum;
  ssize_t  readnum, sendnum;
  FILE    *fpsockr, *fpsockw;
  char  wrbp_buf[WRBP_CLEN];   /* command buffer */
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
  (void)snprintf(msg, sizeof(msg), "connected to %s:%s", host_, port_);
  write_log(msg);
#endif

  /***** read welcome message *****/
  readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
#if DEBUG
  (void)snprintf(msg, sizeof(msg), "%s", wrbp_buf);
  write_log(msg);
  (void)snprintf(msg, sizeof(msg), "num = %zd %s",
		 readnum, (char *)strerror(errno));
  write_log(msg);
#endif


  /***** STAT check *****/
  (void)snprintf(wrbp_buf, WRBP_CLEN, "%s", WRBP_STAT);
  sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw);
  readnum = fread(wrbp_buf, 1, WRBP_CLEN, fpsockr);
  if (readnum != WRBP_CLEN) {
    (void)snprintf(msg, sizeof(msg),
		   "SIZE packet len. invalid: %zd %s",
		   readnum, (char *)strerror(errno));
    write_log(msg);
    (void)fclose(fpsockw);
    (void)fclose(fpsockr);
    return (1);
  }
  (void)printf("%s:%s\t%s\n", host, port, wrbp_buf);

#if DEBUG
  (void)snprintf(msg, sizeof(msg), "%s", wrbp_buf);
  write_log(msg);
  (void)snprintf(msg, sizeof(msg), "num2 = %zd %s",
		 readnum, (char *)strerror(errno));
  write_log(msg);
#endif

  /***** QUIT *****/
  /*  (void)snprintf(wrbp_buf, WRBP_CLEN, "%s", WRBP_QUIT); */
  /*  sendnum = fwrite(wrbp_buf, 1, WRBP_CLEN, fpsockw); */
  (void)fclose(fpsockw);
  (void)fclose(fpsockr);

  return (status);
}

/* read server list file */
static void
read_srvlist(void)
{
  FILE  *fp;
  char  tbuf[2048];
  char  *field_p, *htmp, *ptmp;
  int   i;

  if ((fp = fopen(srvlist_file, "r")) == NULL) {
    (void)snprintf(msg, sizeof(msg), "server list file '%s' : %s",
		   srvlist_file, strerror(errno));
    err_sys(msg);
  }

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
  }  /* while (fgets(tbuf, sizeof(tbuf), fp) != NULL) */
  (void)fclose(fp);
}

/* print usage & exit */
static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr,
		"Usage : %s srvlist\n",
		progname);
  (void)fprintf(stderr,
		"  options : -h : print this message.    \n");
  exit(1);
}
