/* $Id: chtbl_ck.c,v 1.2.2.1 2011/08/02 13:21:12 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "winlib.h"

static const char  rcsid[] =
   "$Id: chtbl_ck.c,v 1.2.2.1 2011/08/02 13:21:12 uehira Exp $";


static void print_usage(void);

int
main(int argc, char *argv[])
{
  FILE  *fp;
  char  *tblfile;
  char  buf[1024];
  int   *sys_ch_list;
  char  name[WIN_STANAME_LEN];
  int i,j,jj,k,kk,sys_ch,height,dum1,dum2;
  float north,east,stcp,stcs,dm1,dm2,sens,to,h,g,adc;
  char  comp[WIN_STACOMP_LEN], unit[7], sname[WIN_STANAME_LEN];
  struct ch_check_list {
    char  code[WIN_STANAME_LEN];
    float north;   /* latitude */
    float east;    /* longitude */
    int   height;  /* height */
    float stcp;    /* station correction of P */
    float stcs;    /* station correction of S */
  } *chcheck;
  int  chnum;
  int  itemnum;
  int  stnum;
  int  old_ch_flag;
  int  mm, nn;


  if (argc < 2) {
    print_usage();
    exit(1);
  }
  tblfile = argv[1];

  if ((fp = fopen(tblfile, "r")) == NULL) {
    (void)fprintf(stderr, "%s: %s\n", tblfile, strerror(errno));
    exit(1);
  }

  /* first, count lines */
  chnum = 0;
  while(fgets(buf, sizeof(buf), fp) != NULL) {
    if (buf[0] == '#')
      continue;
    chnum++;
  }
  /* rewind */
  if (fseek(fp, 0L, SEEK_SET) != 0) {
    (void)fprintf(stderr, "%s: %s\n", tblfile, strerror(errno));
    exit(1);
  }

  /* allocate memory */
  if ((sys_ch_list = MALLOC(int, chnum)) == NULL) {
    (void)fprintf(stderr, "sys_ch_list: %s\n", strerror(errno));
    exit(1);
  }
  if ((chcheck = MALLOC(struct ch_check_list, chnum)) == NULL){
    (void)fprintf(stderr, "chcheck: %s\n", strerror(errno));
    exit(1);
  }

  for (nn = 0; nn < chnum; ++nn) {
      /* chcheck[nn].north = chcheck[nn].east = 400.0; */
      chcheck[nn].height = 10000000;
      chcheck[nn].stcp = chcheck[nn].stcs = 1.1e+38;  /* big value */
  }
  nn = 0;
  stnum = 0;

  
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    if(buf[0] == '#') continue;
    sscanf(buf, "%"STRING(WIN_STANAME)"s", name);
    if (strlen(name) < 3) { /* old channel table format */
      itemnum = 
	sscanf(buf,
	       "%x%x%d%d%"STRING(WIN_STANAME)"s%"STRING(WIN_STACOMP)"s%d%"STRING(WIN_STANAME)"s%f%6s%f%f%f%f%f%f%d%f%f",
	       &i, &j, &dum1, &dum2, name, comp, &k, sname, &sens, unit,
	       &to, &h, &g, &adc, &north, &east, &height, &stcp, &stcs);
      if (itemnum <= 0)
	printf("There is a blank line.\n");
      else if (!(itemnum == 14 || 17 <= itemnum))
	printf("invalid item number: %s", buf);
      sys_ch = (i << 8) + j;
      old_ch_flag = 1;
    } else {
      itemnum =
	sscanf(buf,
	       "%x%d%d%"STRING(WIN_STANAME)"s%"STRING(WIN_STACOMP)"s%d%"STRING(WIN_STANAME)"s%f%6s%f%f%f%f%f%f%d%f%f",
	       &sys_ch, &dum1, &dum2, name, comp, &k, sname, &sens, unit,
	       &to, &h, &g, &adc, &north, &east, &height, &stcp, &stcs);
      old_ch_flag = 0;
      if (itemnum <= 0)
	printf("There is a blank line.\n");
      else if (!(itemnum == 13 || 16 <= itemnum))
	printf("invalid item number: %s", buf);
    }

      /** check station proper parameters **/
      for (mm = 0; mm < stnum; ++mm) {
	if (strcmp(name, chcheck[mm].code) == 0)
	  break;
      }
      if (mm == stnum) {
	strcpy(chcheck[mm].code, name);
	stnum++;
      }
      /* position check */
      if ((old_ch_flag && itemnum >= 17) || (!old_ch_flag && itemnum >= 16)) {
	if (chcheck[mm].height == 10000000) {
	  chcheck[mm].north = north;
	  chcheck[mm].east = east;
	  chcheck[mm].height = height;
	} else {
	  if (chcheck[mm].north != north || chcheck[mm].east != east || chcheck[mm].height != height)
	    printf("Inconsistent position information: %s[%s]\n",
		   chcheck[mm].code, comp);
	}
      }

      /* station correction of P check */
      if ((old_ch_flag && itemnum >= 18) || (!old_ch_flag && itemnum >= 17)) {
	/* printf("%s\t%f\n", text_buf, stcp); */
	if (chcheck[mm].stcp > 1.0e+38)
	  chcheck[mm].stcp = stcp;
	else
	  if (chcheck[mm].stcp != stcp)
	    printf("Inconsistent station correction of P : %s\n",
		   chcheck[mm].code);
      }
      
      /* station correction of S check */
      if ((old_ch_flag && itemnum >= 19) || (!old_ch_flag && itemnum >= 18)) {
	/* printf("%s\t%f\n", text_buf, stcs); */
	if (chcheck[mm].stcs > 1.0e+38)
	  chcheck[mm].stcs = stcs;
	else
	  if (chcheck[mm].stcs != stcs)
	    printf("Inconsistent station correction of S : %s\n",
		   chcheck[mm].code);
      }

      /** check duplicated channel number **/
      sys_ch_list[nn] = sys_ch;
      for (mm = 0; mm < nn; ++mm)
	if (sys_ch_list[mm] == sys_ch_list[nn])
	  printf("Duplicate sys_ch list in channels table: %X\n",
		sys_ch_list[mm]);
      nn++;
  }
  FREE(chcheck);
  FREE(sys_ch_list);
  (void)fclose(fp);
  exit(0);
}

static void
print_usage(void)
{

  WIN_version();
  (void)fprintf(stderr, "%s\n", rcsid);
  fprintf(stderr,"usage: chtbl_ck file\n");
  }
