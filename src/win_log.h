/* $Id: win_log.h,v 1.1 2004/11/11 10:50:30 uehira Exp $ */

#ifndef _WIN_LOG_H_
#define _WIN_LOG_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

void write_log(char *);
void err_sys(char *);

#endif  /* !_WIN_LOG_H_*/
