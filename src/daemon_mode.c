/* $Id: daemon_mode.c,v 1.2.2.2 2010/12/28 12:55:41 uehira Exp $ */

/*-
 * Daemon mode utility
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>


#define MAXFD  64


static pid_t Fork(void);

void
daemon_init(const char *pname, int facility, int use_syslog)
{
  int    i;
  pid_t  pid;

  if ((pid = Fork()) != 0)
    exit(0);   /* parent terminates */

  /* 1st child continues */
  if (setsid() == -1) {   /* become session leader */
    (void)fprintf(stderr, "%s", strerror(errno));
    exit(1);
  }

  signal(SIGHUP, SIG_IGN);   /* ignore HUNGUP signal */

  if ((pid = Fork()) != 0)
    exit(0);   /* 1st child terminates */

  /* 2nd child continues */
  /* (void)chdir("/"); */   /* change working directory */
  
  (void)umask(0);     /* clear our file mode creation mask */

  for (i = 0; i < MAXFD; ++i)
    (void)close(i);

  if (use_syslog)
    openlog(pname, LOG_PID, facility);
}

void
daemon_inetd(const char *pname, int facility, int use_syslog)
{

  if (use_syslog)
    openlog(pname, LOG_PID, facility);
}

/* fork(2) wrapper routine */
static pid_t
Fork(void)
{
  pid_t	pid;

  if ((pid = fork()) == -1) {
    (void)fprintf(stderr, "%s", strerror(errno));
    exit(1);
  }

  return (pid);
}
