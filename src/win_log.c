/* $Id: win_log.c,v 1.2 2004/11/30 14:16:30 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
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

#include "win_externs.h"
#include "win_log.h"
#include "subst_func.h"

static void get_time(int []);

void
write_log(char *ptr)
{
  FILE *fp;
  int tm[6];

  if (syslog_mode)
    syslog(LOG_NOTICE, "%s", ptr);
  else  {
    if (logfile != NULL) {
      fp = fopen(logfile, "a");
      if (fp == NULL)
	return;
    }
    else
      fp = stdout;
    get_time(tm);
    (void)fprintf(fp, "%02d%02d%02d.%02d%02d%02d %s %s\n",
		  tm[0], tm[1], tm[2], tm[3], tm[4], tm[5], progname, ptr);
    if (logfile != NULL)
      (void)fclose(fp);
  }
}

void
err_sys(char *ptr)
{

  if (syslog_mode)
    syslog(LOG_NOTICE, "%s", ptr);
  else {
    perror(ptr);
    write_log(ptr);
  }
  if (strerror(errno))
    write_log((char *)strerror(errno));

  exit_status = EXIT_FAILURE;
  end_program();
}

void
end_program(void)
{

  write_log("end");
  if (syslog_mode)
    closelog();
  /*  printf("%d\n", status); */
  exit(exit_status);
}

static void
get_time(int rt[])
{
  struct tm  *nt;
  time_t  ltime;

  (void)time(&ltime);
  nt = localtime(&ltime);
  rt[0] = nt->tm_year % 100;
  rt[1] = nt->tm_mon + 1;
  rt[2] = nt->tm_mday;
  rt[3] = nt->tm_hour;
  rt[4] = nt->tm_min;
  rt[5] = nt->tm_sec;  
}
