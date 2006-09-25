/* $Id: win_log.c,v 1.2.4.1 2006/09/25 15:01:00 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <syslog.h>

#include "win_externs.h"
#include "winlib.h"
#include "win_log.h"
#include "subst_func.h"

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
