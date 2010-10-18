/* $Id: shmcounter.c,v 1.1.2.1 2010/10/18 14:33:51 uehira Exp $ */

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
  "$Id: shmcounter.c,v 1.1.2.1 2010/10/18 14:33:51 uehira Exp $";

static char *progname;

/* protptypes */
static void usage(void);
int main(int, char *[]);

int
main(int argc, char *argv[])
{
  key_t shm_key_in;
  struct Shm  *shm_in;
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
  
  shm_key_in = (key_t)atol(argv[0]);
  shm_in = Shm_read_offline(shm_key_in);
  
  printf("pl\t\tp\t\tr\t\tc\n");
  for (;;) {
    printf("\r");
    printf("%zu\t%zu\t%zu\t%lu",
	   shm_in->pl, shm_in->p, shm_in->r, shm_in->c);
    /* usleep(1); */
  }

  exit(0);
}

static void
usage()
{
  
  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr, "usage : %s shmin\n", progname);
}
