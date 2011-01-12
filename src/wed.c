/* $Id: wed.c,v 1.5.4.3.2.7.2.1 2011/01/12 16:57:07 uehira Exp $ */
/* program "wed.c"
	"wed" edits a win format data file by time range and channles
	6/26/91,7/13/92,3/11/93,4/20/94,8/5/94,12/8/94   urabe
	2/5/97    Little Endian (uehira)
	2/5/97    High sampling rate (uehira)
        98.6.26 yo20000
        99.4.19 byte-order-free
        2000.4.17   wabort
        2002.8.5    ignore illegal lines in ch file
        2003.10.29 exit()->exit(0)
        2006.9.21 added comment on mklong()
	2010.9.16 64bit clean (Uehira)
	2010.9.17 replace read_data() with read_onesec_win2() (Uehira)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "winlib.h"

#define	DEBUG1 0

static const char  rcsid[] =
  "$Id: wed.c,v 1.5.4.3.2.7.2.1 2011/01/12 16:57:07 uehira Exp $";

static uint8_w  *buf=NULL, *outbuf;
static size_t  cbufsiz;  /* buffer size of buf[] & outbuf[] */
static int  leng, dec_start[6], dec_end[6], dec_now[6], nch;
static WIN_ch  sysch[WIN_CHMAX];
static FILE  *f_param;

/* prototypes */
static void wabort(void);
static void get_one_record(void);
static WIN_bs select_ch(WIN_ch *, int, uint8_w *, uint8_w *);
int main(int, char *[]);

static void
wabort(void)
{

  exit(0);
}

static void
get_one_record(void)
{
  int  i;
  size_t  outsize;

  for (i = 0; i < 6; i++)
    dec_end[i] = dec_start[i];
  for (i = leng - 1; i > 0; i--) {
    dec_end[5]++;
    adj_time(dec_end);
  }
#if DEBUG
  fprintf(stderr,"start:%02d%02d%02d %02d%02d%02d\n",
	  dec_start[0],dec_start[1],dec_start[2],
	  dec_start[3],dec_start[4],dec_start[5]);
  fprintf(stderr,"end  :%02d%02d%02d %02d%02d%02d\n",
	  dec_end[0],dec_end[1],dec_end[2],
	  dec_end[3],dec_end[4],dec_end[5]);
#endif
  /* read first block */
  do {
    if (read_onesec_win2(stdin, &buf, &outbuf, &cbufsiz) == 0) {
      exit(1);
    }
    bcd_dec(dec_now, buf + 4);
#if DEBUG
    fprintf(stderr, "\r%02x%02x%02x %02x%02x%02x  %5d", buf[4], buf[5],
	    buf[6], buf[7], buf[8], buf[9], *(int *)buf);
    fflush(stderr);
    /*
     * fprintf(stderr,"%02x%02x%02x %02x%02x%02x  %5d\n",buf[4],buf[5],
     * buf[6],buf[7],buf[8],buf[9],*(int *)buf);
     */
#endif
  } while (time_cmp(dec_start, dec_now, 6) > 0);

  while (1) {
    outsize = (size_t)select_ch(sysch, nch, buf, outbuf);
    if (outsize > 10 || leng >= 0)
      /* write one sec */
      if (fwrite(outbuf, 1, outsize, stdout) == 0)
	exit(1);
    if (leng >= 0 && time_cmp(dec_now, dec_end, 6) == 0)
      break;
    /* read one sec */
    if (read_onesec_win2(stdin, &buf, &outbuf, &cbufsiz) == 0)
      break;
    bcd_dec(dec_now, buf + 4);
    if (leng >= 0 && time_cmp(dec_now, dec_end, 6) > 0)
      break;
#if DEBUG
    fprintf(stderr, "\rnow  :%02x%02x%02x %02x%02x%02x", buf[4], buf[5],
	    buf[6], buf[7], buf[8], buf[9]);
    fflush(stderr);
#endif
  }
#if DEBUG
  fprintf(stderr, " : done\n");
#endif
}

static WIN_bs
select_ch(WIN_ch *sys_ch, int n_ch, uint8_w *old_buf, uint8_w *new_buf)
{
  int  i, j;
  uint32_w  gsize;
  WIN_bs  new_size;
  uint8_w  *ptr, *new_ptr, *ptr_lim;
  WIN_ch  chtmp;

  ptr_lim = old_buf + mkuint4(old_buf);
  ptr = old_buf + 4;
  new_ptr = new_buf + 4;
  for (i = 0; i < 6; i++)
    *new_ptr++ = (*ptr++);
  new_size = 10;
  do {
    gsize = get_sysch(ptr, &chtmp);
#if DEBUG1
    fprintf(stderr, "chtmp=%04hX\n", chtmp);
#endif
    for (j = 0; j < n_ch; j++)
      if (chtmp == sys_ch[j])
	break;
    if (n_ch < 0 || j < n_ch) {
      new_size += gsize;
      while (gsize-- > 0)
	*new_ptr++ = (*ptr++);
    } else
      ptr += gsize;
  } while (ptr < ptr_lim);
  new_buf[0] = new_size >> 24;
  new_buf[1] = new_size >> 16;
  new_buf[2] = new_size >> 8;
  new_buf[3] = new_size;
  return (new_size);
}

int
main(int argc, char *argv[])
{
  char  tb[100];
  int  k;

  signal(SIGINT, (void *)wabort);
  signal(SIGTERM, (void *)wabort);

  if (argc < 4) {
    WIN_version();
    fprintf(stderr, "%s\n", rcsid);
    fprintf(stderr, " usage of 'wed' :\n");
    fprintf(stderr, "   'wed [YYMMDD] [hhmmss] [len(s)] ([ch list file]) <[in_file] >[out_file]'\n");
    fprintf(stderr, " example of channel list file :\n");
    fprintf(stderr, "   '01d 01e 01f 100 101 102 119 11a 11b'\n");
    exit(0);
  }
  if (argc > 4) {
    if ((f_param = fopen(argv[4], "r")) == NULL) {
      perror("fopen");
      exit(1);
    }
    nch = 0;
    while (fscanf(f_param, "%99s", tb) != EOF) {
      if (sscanf(tb, "%x", &k) != 1)
	continue;
      sysch[nch] = k & 0xffff;
#if DEBUG
      fprintf(stderr, " %04hX", sysch[nch]);
#endif
      nch++;
    }
#if DEBUG
    fprintf(stderr, "\n  <- %d chs according to '%s'\n", nch - 1, argv[4]);
#endif
    fclose(f_param);
  } else
    nch = (-1);
  sscanf(argv[1], "%2d%2d%2d", dec_start, dec_start + 1, dec_start + 2);
  sscanf(argv[2], "%2d%2d%2d", dec_start + 3, dec_start + 4, dec_start + 5);
  sscanf(argv[3], "%d", &leng);
  get_one_record();
  exit(0);
}
