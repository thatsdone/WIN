/* $Id: shmcounter.c,v 1.1.2.5 2010/10/19 09:27:34 uehira Exp $ */

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
  "$Id: shmcounter.c,v 1.1.2.5 2010/10/19 09:27:34 uehira Exp $";

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
  size_t  pl_save, p_save;
  int  c_save_flag, p_save_flag;
  time_t  now;
  int  a_opt, p_opt;
  int  c;

  if ((progname = strrchr(argv[0], '/')) != NULL)
    progname++;
  else
    progname = argv[0];

  a_opt = p_opt = 0;

  while ((c = getopt(argc, argv, "ahp")) != -1) {
    switch (c) {
    case 'a':
      a_opt = 1;  /* one around */
      break;
    case 'p':
      p_opt = 1;  /* print if pl change */
      break;
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
  
  p_save_flag = c_save_flag = 0;
  pl_save = 0;

  shm_key_in = (key_t)atol(argv[0]);
  shm_in = Shm_read_offline(shm_key_in);
  
  printf("pl                  p                   r                   c\n");

  /* begin loop */
  for (;;) {
    if (c_save_flag && c_save == shm_in->c) {
      usleep(2);
      continue;
    }
    if (pl_save == 0)
      pl_save = shm_in->pl;

    /* output informations */
    printf("\r%-20zu%-20zu%-20zu%-20lu",
	   shm_in->pl, shm_in->p, shm_in->r, shm_in->c);
    /* if pl change */
    if (p_opt && pl_save != shm_in->pl) {
      now = time(NULL);
      printf("\npl diff! : %zu --> %zu : %s",
	     pl_save, shm_in->pl, ctime(&now));
      pl_save = shm_in->pl;
    }
    /* one around */
    if (a_opt && p_save_flag && p_save > shm_in->p) {
      now = time(NULL);
      printf("\none around : %s", ctime(&now));
    }
    (void)fflush(stdout);

    /* save c */
    c_save = shm_in->c;
    if (c_save_flag == 0)
      c_save_flag = 1;
    
    /* save p */
    p_save = shm_in->p;
    if (p_save_flag == 0)
      p_save_flag = 1;
  }  /* for (;;) */
}

static void
usage()
{
  
  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr, "usage : %s [-ap] shm_key\n", progname);
  fprintf(stderr, "     options : -a : print time if one around.\n");
  fprintf(stderr, "             : -p : print time if 'pl' changed.\n");
}
