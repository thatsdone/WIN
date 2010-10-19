/* $Id: shmcounter.c,v 1.1.2.3 2010/10/19 04:03:29 uehira Exp $ */

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

#include "winlib.h"

static const char rcsid[] =
  "$Id: shmcounter.c,v 1.1.2.3 2010/10/19 04:03:29 uehira Exp $";

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
      printf("\npl diff! : %zu --> %zu\n", pl_save, shm_in->pl);
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
