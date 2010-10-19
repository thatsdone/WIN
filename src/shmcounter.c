/* $Id: shmcounter.c,v 1.1.2.4 2010/10/19 04:30:04 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

#include "winlib.h"

static const char rcsid[] =
  "$Id: shmcounter.c,v 1.1.2.4 2010/10/19 04:30:04 uehira Exp $";

static char *progname;

/* protptypes */
static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  key_t shm_key_in;
  struct Shm  *shm_in;
  unsigned long  c_save;
  int  c_save_flag;
  size_t  pl_save;
  time_t  now;
  int  c;

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  while ((c = getopt(argc, argv, "h")) != -1) {
    switch (c) {
    case 'h':
    default:
      usage();
      exit(1);
    }
  }
  argc -= optind;
  argv += optind;
  
  if (argc < 1) {
    usage();
    exit(1);
  }
  
  c_save_flag = 0;
  pl_save = 0;

  shm_key_in = (key_t)atol(argv[0]);
  shm_in = Shm_read_offline(shm_key_in);
  
  printf("pl                  p                   r                   c\n");
  for (;;) {
    if (c_save_flag && c_save == shm_in->c) {
      usleep(1);
      continue;
    }
    if (pl_save == 0)
      pl_save = shm_in->pl;
    printf("\r%-20zu%-20zu%-20zu%-20lu",
	   shm_in->pl, shm_in->p, shm_in->r, shm_in->c);
    if (pl_save != shm_in->pl) {
      now = time(NULL);
      printf("\npl diff! : %zu --> %zu : %s",
	     pl_save, shm_in->pl, ctime(&now));
      pl_save = shm_in->pl;
    }
    (void)fflush(stdout);
    c_save = shm_in->c;
    if (c_save_flag == 0)
      c_save_flag = 1;
  }  /* for (;;) */
}

static void
usage()
{
  
  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr, "usage : %s shm_key\n", progname);
}
