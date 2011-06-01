/* $Id: winlib_log.c,v 1.2 2011/06/01 11:09:22 uehira Exp $ */

/*-
 * winlib.c  (Uehira Kenji)
 *  win system library
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define DIRNAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define DIRNAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

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

struct Shm *
Shm_read(key_t shmkey, char *msg)
{
  struct Shm  *shm;
  int  shmid, aflag;
  char  tbuf[1024];

  if ((shmid = shmget(shmkey, 0, 0)) == -1) {
    (void)snprintf(tbuf, sizeof(tbuf), "shmget %s", msg);
    err_sys(tbuf);
  }

  aflag = SHM_RDONLY;  /* Read-Only */
  /* aflag = 0;  original */

  if ((shm = (struct Shm *)shmat(shmid, NULL, aflag)) == (struct Shm *)-1) {
    (void)snprintf(tbuf, sizeof(tbuf), "shmat %s", msg);
    err_sys(tbuf);
  }

  (void)snprintf(tbuf, sizeof(tbuf),
		 "Shm_read : %s : key=%ld id=%d (%p)",
		 msg, shmkey, shmid, shm);
  write_log(tbuf);

  return (shm);
}

struct Shm *
Shm_create(key_t shmkey, size_t shmsize, char *msg)
{
  struct Shm  *shm;
  int  shmid, oflag;
  char  tbuf[1024];

  oflag = IPC_CREAT;
  oflag |= SHM_R |  SHM_W;  /* user permission */
  oflag |= (SHM_R>>3);      /* group permission */
  oflag |= (SHM_R>>6);      /* other permission */
  /* oflag |= (SHM_R>>3) | (SHM_W>>3);  group permission */
  /* oflag |= (SHM_R>>6) | (SHM_W>>6);  other permission */

  if ((shmid = shmget(shmkey, shmsize, oflag)) == -1) {
    (void)snprintf(tbuf, sizeof(tbuf), "shmget %s", msg);
    err_sys(tbuf);
  }

  if ((shm = (struct Shm *)shmat(shmid, NULL, 0)) == (struct Shm *)-1) {
    (void)snprintf(tbuf, sizeof(tbuf), "shmat %s", msg);
    err_sys(tbuf);
  }

  (void)snprintf(tbuf, sizeof(tbuf),
		 "Shm_create : %s : key=%ld id=%d size=%zu (%p)",
		 msg, shmkey, shmid, shmsize, shm);
  write_log(tbuf);

  return (shm);
}

sa_family_t
sockfd_to_family(int sockfd)
{
  struct sockaddr_storage  ss;
  struct sockaddr *sa =  (struct sockaddr *)&ss;
  socklen_t  len;

  len = sizeof(ss);
  if (getsockname(sockfd, sa, &len) < 0)
    err_sys("getsockname");

  return (sa->sa_family);
}
