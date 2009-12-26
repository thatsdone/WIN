/* $Id: winlib_log.c,v 1.1.2.1 2009/12/26 00:56:59 uehira Exp $ */

/*-
 * winlib.c  (Uehira Kenji)
 *  win system library
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <dirent.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <syslog.h>

#include "winlib.h"

int
find_oldest(char *path, char *oldst)
{				/* returns N of files */
  int		  i;
  struct dirent  *dir_ent;
  DIR            *dir_ptr;

  /* find the oldest file */
  if ((dir_ptr = opendir(path)) == NULL)
    err_sys("opendir");
  i = 0;
  while ((dir_ent = readdir(dir_ptr)) != NULL) {
    if (*dir_ent->d_name == '.')
      continue;
    if (!isdigit(*dir_ent->d_name))
      continue;
    if (i++ == 0 || strcmp2(dir_ent->d_name, oldst) < 0)
      strcpy(oldst, dir_ent->d_name);
  }
#if DEBUG
  printf("%d files in %s, oldest=%s\n", i, path, oldst);
#endif
  (void)closedir(dir_ptr);

  return (i);
}

