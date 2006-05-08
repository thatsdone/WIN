/* $Id: daemon_mode.h,v 1.2 2006/05/08 04:02:30 uehira Exp $ */

/*
 * Copyright (c) 2004
 *   Uehira Kenji / All Rights Reserved.
 */

#ifndef _DAEMON_MODE_H_
#define _DAEMON_MODE_H_

void daemon_init(const char *, int, int);
void daemon_inetd(const char *, int, int);

#endif   /* _DAEMON_MODE_H_ */
