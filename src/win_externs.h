/* $Id: win_externs.h,v 1.3 2004/11/30 14:16:30 uehira Exp $ */

#ifndef _WIN_EXTERNS_H_
#define _WIN_EXTERNS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern char *progname;    /* program name */
extern char *logfile;     /* logfile name */

extern int  syslog_mode;  /* syslog mode flag */

extern int  exit_status;  /* exit status */

#endif  /* !_WIN_EXTERNS_H_*/
