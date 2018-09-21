/* $Id: wsr.c,v 1.1.2.2 2018/09/21 06:41:10 uehira Exp $ */
/*
 * program "wsr.c"
 *   "wsr" select same sampling rate data.
 *   2018.9.18 copy from wch.c and modified. (Uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <signal.h>
#include  <unistd.h>

#include "winlib.h"

static const char rcsid[] =
  "$Id: wsr.c,v 1.1.2.2 2018/09/21 06:41:10 uehira Exp $";

static uint8_w *buf = NULL, *outbuf;
static size_t  cbufsiz; /* buffer size of buf[] & outbuf[] */
static WIN_sr  s_sr;
static int     vflag, nflag;

/* prototypes */
static void wabort(void);
static WIN_bs select_ch(WIN_sr , uint8_w *, uint8_w *);
static void get_one_record(void);
static void usage(void);
int main(int, char *[]);

static void
wabort(void)
{

  exit(0);
}

static WIN_bs 
select_ch(WIN_sr ssr, uint8_w *old_buf, uint8_w *new_buf)
{
  int       i, ss;
  WIN_bs    size, new_size;
  uint8_w   *ptr, *new_ptr, *ptr_lim, *time_ptr;
  uint32_w  gsize;
  WIN_ch    ch;
  WIN_sr    sr;

  size = mkuint4(old_buf);
  ptr_lim = old_buf + size;
  ptr = old_buf + WIN_BSLEN;
  time_ptr = new_ptr = new_buf + WIN_BSLEN;
  for (i = 0; i < 6; i++)
    *new_ptr++ = (*ptr++);
  new_size = 10;
  do {
    gsize = win_chheader_info(ptr, &ch, &sr, &ss);

    if (ssr == sr) {
      new_size += gsize;
      while (gsize-- > 0)
	*new_ptr++ = (*ptr++);
    } else {
      ptr += gsize;
      if (vflag) {
	if (nflag)
	  (void)fprintf(stdout, "%02X%02X%02X %02X%02X%02X %04X  %d Hz\n",
			time_ptr[0], time_ptr[1], time_ptr[2], 
			time_ptr[3], time_ptr[4], time_ptr[5], 
			ch, sr);
	else
	  (void)fprintf(stderr, "%02X%02X%02X %02X%02X%02X %04X  %d Hz\n",
			time_ptr[0], time_ptr[1], time_ptr[2], 
			time_ptr[3], time_ptr[4], time_ptr[5], 
			ch, sr);
      }
    }
  } while (ptr < ptr_lim);
  new_buf[0] = new_size >> 24;
  new_buf[1] = new_size >> 16;
  new_buf[2] = new_size >> 8;
  new_buf[3] = new_size;

  return (new_size);
}

static void
get_one_record(void)
{

  while (read_onesec_win2(stdin, &buf, &outbuf, &cbufsiz) > 0) {
    /* read one sec */
    if (select_ch(s_sr, buf, outbuf) > 10)
      /* write one sec */
      if (!nflag)
	if (fwrite(outbuf, 1, mkuint4(outbuf), stdout) == 0)
	  exit(1);
#if DEBUG
    fprintf(stderr, "in:%d B out:%d B\n", mkuint4(buf), mkuint4(outbuf));
#endif
  }
#if DEBUG
  fprintf(stderr, " : done\n");
#endif
}

static void
usage(void)
{
  
  WIN_version();
  fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr," usage of 'wsr' :\n");
  fprintf(stderr,"   'wsr [-nv] sampling_rate  < [in_file] > [out_file]'\n");
}

int
main(int argc, char *argv[])
{
  int  ch;
  
  signal(SIGINT, (void *)wabort);
  signal(SIGTERM, (void *)wabort);

  /* clear flags */
  nflag = vflag = 0;
  while ((ch = getopt(argc, argv, "nv")) != -1) {
    switch (ch) {
    case 'n' :
      nflag = 1;
      vflag = 1;
      break;
    case 'v' :
      vflag = 1;
      break;
    default :
      usage();
      exit (1);
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 1) {
    usage();
    exit(1);
  }

  s_sr = (WIN_sr)atoi(argv[0]);
  if (vflag)
    (void)fprintf(stderr, "SR = %d [Hz]\n", s_sr);
  if (nflag)
    (void)fprintf(stdout, "SR = %d [Hz]\n", s_sr);

  get_one_record();

  exit(0);
}
