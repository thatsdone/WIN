/* $Id: find_picks.c,v 1.2.2.8 2010/12/28 12:55:41 uehira Exp $ */

/* find_picks */
/* search for pick files in pick dir */
/* from win.c            2000.7.31 urabe */
/* 2005.8.10 urabe bug in strncmp2() fixed : 0-6 > 7-9 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#include "winlib.h"

#define LINELEN 256
#define NAMLEN 256

static const char rcsid[] =
   "$Id: find_picks.c,v 1.2.2.8 2010/12/28 12:55:41 uehira Exp $";

/* prototypes */
int main(void);

int
main()
  {
  FILE *fp;
  struct dirent *dir_ent;
  DIR *dir_ptr;
  char text_buf[LINELEN],name1[NAMLEN],name2[NAMLEN],datafile[NAMLEN],
    filename[NAMLEN],namebuf[NAMLEN],pickdir[NAMLEN];

  fprintf(stdout,"PICKS OK\n");
  fflush(stdout);
  if(fgets(text_buf,LINELEN,stdin) != NULL)
    {
    sscanf(text_buf,"%s%s%s%s",name1,name2,datafile,pickdir);

    if((dir_ptr=opendir(pickdir))==NULL)
      {
      fprintf(stderr,"directory '%s' not open\n",pickdir);
      exit(1);
      }
    while((dir_ent=readdir(dir_ptr))!=NULL)
      {
      if(*dir_ent->d_name=='.') continue; /* skip "." & ".." */
    /* pick file name must be in the time range of data file */
      if(strncmp2(dir_ent->d_name,name1,13)<0 ||
        strncmp2(dir_ent->d_name,name2,13)>0) continue;
    /* read the first line */
      snprintf(filename,sizeof(filename),"%s/%s",pickdir,dir_ent->d_name);
      if((fp=fopen(filename,"r"))==NULL) continue;
      *text_buf=0;
      fgets(text_buf,LINELEN,fp);
      fclose(fp);
    /* first line must be "#p [data file name] ..." */
      sscanf(text_buf,"%s%s",filename,namebuf);
      if(strcmp(filename,"#p") || strcmp(namebuf,datafile)) continue;

      fprintf(stdout,"%s\n",dir_ent->d_name);
      }
    closedir(dir_ptr);
    }
  exit(0);
  }
