/* $Id: wshift.c,v 1.1.2.2 2014/01/09 00:42:55 uehira Exp $ */

/*-
 * Bit shift waveform data.
 *  offline version of "raw_shift.c".
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "winlib.h"

#define  MAX_SR  HEADER_5B

static const char  rcsid[] =
   "$Id: wshift.c,v 1.1.2.2 2014/01/09 00:42:55 uehira Exp $";

static char  *progname;
static char  *chfile;
static int   negate_channel;
static uint8_w ch_table[WIN_CHMAX];

/* prototypes */
int main(int, char *[]);
static void read_chfile(void);
static void usage(void);

int
main(int argc, char *argv[])
{
  int      i;
  int   bits_shift;
  FILE  *fp_in, *fp_out;
  int   rest;
  uint8_w  *dbuf = NULL, *ptr, *ptr_limit;;
  size_t   dbuf_siz;  /* dbuf buffer size */
  WIN_bs   dsize;     /* WIN data size of dbuf[] */
  uint8_w  *outbuf, *ptw;
  uint32_w  gs, gs1;
  WIN_ch   ch;
  WIN_sr   sr1;
  WIN_ch   ch1;
  WIN_bs   new_size;
  static int32_w  buf1[MAX_SR], buf2[MAX_SR];

  /* get program name */
  if ((progname = strrchr(argv[0], '/')) == NULL)
    progname = argv[0];
  else
    progname++;

  if (argc < 2)
    usage();

  bits_shift = atoi(argv[1]);
  if ((bits_shift < 0) || (bits_shift > 32)) {
    (void)fprintf(stderr, "Invalid bits shift.\n");
    /* usage(); */
    exit(1);
  }
  (void)fprintf(stderr, "bits_shift = %d\n", bits_shift);

  fp_in = stdin;
  fp_out = stdout;

  chfile = NULL;
  rest = 1;
  /*   printf("%s\n", argv[2]); */
  if (argc > 2) {
    if (strcmp("-", argv[2]) == 0)
      chfile = NULL;
    else {
      if (argv[2][0] == '-') {
        chfile = argv[2] + 1;
        negate_channel = 1;
      } else if (argv[2][0] == '+') {
	chfile = argv[2] + 1;
        negate_channel = 0;
      } else {
        chfile = argv[2];
        rest = negate_channel = 0;
      }
    }
  }    
  /* printf("%s\n", chfile); */

  read_chfile();

  /* read data loop */
  while ((dsize = read_onesec_win2(fp_in, &dbuf, &outbuf, &dbuf_siz)) != 0) {
    ptr = dbuf + WIN_BSLEN;
    ptr_limit = dbuf + dsize;
    ptw = outbuf + WIN_BSLEN;

    /* copy time stamp */
    for (i = 0; i < WIN_TM_LEN; i++)
      *ptw++ = (*ptr++);  /* YMDhms */

    do {   /* loop for ch's */
      gs = get_sysch(ptr, &ch);
      if (ch_table[ch]) {
#if DEBUG
        (void)fprintf(stderr," %u", gs);
#endif
	(void)win2fix(ptr, buf1, &ch1, &sr1);
	for (i = 0; i < sr1; i++)
	  buf2[i] = buf1[i] >> bits_shift;
	gs1 = winform(buf2, ptw, sr1, ch1);
	ptw += gs1;
#if DEBUG
	(void)fprintf(stderr,"->%u ",gs1);
#endif
      } else if (rest == 1) {
	memcpy(ptw, ptr, gs);
	ptw += gs;
      }
      ptr += gs;
    } while (ptr < ptr_limit);

    /* block size */
    new_size = (WIN_bs)(ptw - outbuf);
    outbuf[0] = new_size >> 24;
    outbuf[1] = new_size >> 16;
    outbuf[2] = new_size >> 8;
    outbuf[3] = new_size;

    if (fwrite(outbuf, 1, new_size, fp_out) != new_size) {
      (void)fprintf(stderr, "output error!!\n!");
      exit(1);
    }
  }

  exit(0);
}

static void
read_chfile(void) {
  FILE  *fp;
  int   n_ch;
  char  tbuf[2048];
  int   i, j, k;
  
  if (chfile != NULL) {
    if ((fp = fopen(chfile, "r" )) != NULL) {
#if DEBUG
      (void)fprintf(stderr,"ch_file=%s\n",chfile);
#endif
      if (negate_channel)
	for (i = 0; i < WIN_CHMAX; i++)
	  ch_table[i] = 1;
      else
	for (i = 0; i < WIN_CHMAX; i++)
	  ch_table[i] = 0;
      j = 0;
      while (fgets(tbuf, sizeof(tbuf), fp)) {
        if (*tbuf=='#' || sscanf(tbuf, "%x", &k) < 0)
	  continue;
        k &= 0xffff;
#if DEBUG
        (void)fprintf(stderr," %04X",k);
#endif
        if (negate_channel) {
          if (ch_table[k] == 1) {
            ch_table[k] = 0;
            j++;
	  }
	} else {
          if (ch_table[k] == 0) {
            ch_table[k] = 1;
            j++;
	  }
	}
      }
#if DEBUG
      (void)fprintf(stderr,"\n");
#endif
      n_ch = j;
      if (negate_channel)
	(void)fprintf(stderr, "-%d channels\n", n_ch);
      else
	(void)fprintf(stderr, "%d channels\n", n_ch);
      fclose(fp);
    } else {
      (void)fprintf(stderr, "%s: %s\n", chfile, strerror(errno));
      exit(1);
    }
  } else {
    for (i = 0; i < WIN_CHMAX; i++)
      ch_table[i] = 1;
    n_ch = i;
    (void)fprintf(stderr, "all channels.\n");
  }
}


static void
usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  (void)fprintf(stderr, "Usage : %s bits (-/[ch_file]/-[ch_file]/+[ch_file]) < raw_in > raw_out\n", progname);
  exit(1);
}
