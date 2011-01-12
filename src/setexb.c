/* $Id: setexb.c,v 1.5.8.4 2011/01/12 15:44:30 uehira Exp $ */
/*-
  program "setexb.c"
    2/27/90, 3/8/93,1/17/94,5/27/94  urabe
    2001.6.22  add options '-p' and '-?'  uehira
    2010.2.16  64bit clean?  (uehira)
-*/

#ifdef HAVE_CONFIG_H
#include        "config.h"
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/types.h>
#include        <unistd.h>

#include "winlib.h"
/* #include        "subst_func.h" */

/* #define         N_EXABYTE	8 */
#define         DEFAULT_PARAM_FILE  "wtape.prm"
#define         WIN_FILENAME_MAX 1024

static const char rcsid[] = 
  "$Id: setexb.c,v 1.5.8.4 2011/01/12 15:44:30 uehira Exp $";

static int  exb_status[N_EXABYTE], n_exb;
static char exb_name[N_EXABYTE][20],
  raw_dir[WIN_FILENAME_MAX], raw_dir1[WIN_FILENAME_MAX],
  param_name[WIN_FILENAME_MAX];

/* prototypes */
static void read_units(char *);
static void write_units(char *);
static void init_param(void);
static void usage(void);
int main(int, char *[]);

static void
read_units(char *file)
{
  FILE  *fp;
  char  tb[WIN_FILENAME_MAX], tb1[WIN_FILENAME_MAX + 50];
  int   i;

  for (i = 0; i < n_exb; i++)
    exb_status[i] = 0;
  sprintf(tb, "%s/%s", raw_dir1, file);
  if ((fp = fopen(tb, "r")) == NULL) {
    sprintf(tb1, "wtape: %s", tb);
    perror(tb1);
    for (i = 0; i < n_exb; i++)
      exb_status[i] = 1;
  } else
    while (read_param_line(fp, tb, sizeof(tb)) == 0) {
      sscanf(tb, "%d", &i);
      if (i < n_exb && i >= 0)
	exb_status[i] = 1;
    }
  fclose(fp);
}

static void
write_units(char *file)
{
  FILE *fp;
  char tb[WIN_FILENAME_MAX];
  int i;

  sprintf(tb, "%s/%s", raw_dir1, file);
  fp = fopen(tb, "w+");
  for (i = 0; i < n_exb; i++)
    if (exb_status[i])
      fprintf(fp, "%d\n", i);
  fclose(fp);
}

static void
init_param(void)
{
  char tb[WIN_FILENAME_MAX], *ptr;
  FILE *fp;

  if ((fp = fopen(param_name, "r")) == NULL) {
    fprintf(stderr, "parameter file '%s' not found\007\n",
	    param_name);
    usage();
    exit(1);
  }
  read_param_line(fp, tb, sizeof(tb));
  if ((ptr = strchr(tb, ':')) == 0) {
    sscanf(tb, "%s", raw_dir);
    sscanf(tb, "%s", raw_dir1);
  } else {
    *ptr = 0;
    sscanf(tb, "%s", raw_dir);
    sscanf(ptr + 1, "%s", raw_dir1);
  }
  read_param_line(fp, tb, sizeof(tb));
  for (n_exb = 0; n_exb < N_EXABYTE; n_exb++) {
    if (read_param_line(fp, tb, sizeof(tb)))
      break;
    sscanf(tb, "%s", exb_name[n_exb]);
  }

  /* read exabyte mask file $raw_dir/UNITS */
  read_units(WTAPE_UNITS);
  write_units(WTAPE__UNITS);
}

static void
usage(void)
{

  WIN_version();
  fprintf(stderr,"%s\n",rcsid);
  fprintf(stderr,"usage: setexb (-p [param file])\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  int i, ch;

  snprintf(param_name, sizeof(param_name), "%s", DEFAULT_PARAM_FILE);
  while ((ch = getopt(argc, argv, "p:?")) != -1) {
    switch (ch) {
    case 'p':			/* parameter file */
      strcpy(param_name, optarg);
      break;
    case '?':
    default:
      usage();
    }
  }
  argc -= optind;
  argv += optind;

  init_param();
  printf("**** EXABYTES *****\n");
  printf("***  unit  use  ***\n");
  for (i = 0; i < n_exb; i++)
    printf("       %d    %d\n", i, exb_status[i]);
  printf("*******************\n");

  exit(0);
}
