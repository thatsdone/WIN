/* $Id: win_log.h,v 1.2 2004/11/30 14:16:30 uehira Exp $ */

#ifndef _WIN_LOG_H_
#define _WIN_LOG_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

void write_log(char *);
void err_sys(char *);
void end_program(void);

#endif  /* !_WIN_LOG_H_*/
