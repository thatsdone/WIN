/* $Id: find_picks.c,v 1.4 2006/03/18 11:22:16 uehira Exp $

/* find_picks */
/* search for pick files in pick dir */
/* from win.c            2000.7.31 urabe */
/* 2005.8.10 urabe bug in strncmp2() fixed : 0-6 > 7-9 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

#include "subst_func.h"

#define LINELEN 256
#define NAMLEN 256

strncmp2(s1,s2,i)
char *s1,*s2;             
int i; 
{
  if((*s1>='0' && *s1<='5') && (*s2<='9' && *s2>='6')) return 1;
  else if((*s1<='9' && *s1>='7') && (*s2>='0' && *s2<='6')) return -1;
  else return strncmp(s1,s2,i);
}

main()
  {
  FILE *fp;
  struct dirent *dir_ent;
  DIR *dir_ptr;
  char text_buf[LINELEN],name1[NAMLEN],name2[NAMLEN],datafile[NAMLEN],
    filename[NAMLEN],namebuf[NAMLEN],pickdir[NAMLEN];

  fprintf(stdout,"PICKS OK\n");
  fflush(stdout);
  if(fgets(text_buf,LINELEN,stdin))
    {
    sscanf(text_buf,"%s%s%s%s",name1,name2,datafile,pickdir);

    if((dir_ptr=opendir(pickdir))==NULL)
      {
      fprintf(stderr,"directory '%s' not open\n",pickdir);
      return 0;
      }
    while((dir_ent=readdir(dir_ptr))!=NULL)
      {
      if(*dir_ent->d_name=='.') continue; /* skip "." & ".." */
    /* pick file name must be in the time range of data file */
      if(strncmp2(dir_ent->d_name,name1,13)<0 ||
        strncmp2(dir_ent->d_name,name2,13)>0) continue;
    /* read the first line */
      sprintf(filename,"%s/%s",pickdir,dir_ent->d_name);
      if((fp=fopen(filename,"r"))==NULL) continue;
      *text_buf=0;
      fgets(text_buf,LINELEN,fp);
      fclose(fp);
    /* first line must be "#p [data file name] ..." */
      sscanf(text_buf,"%s%s",filename,namebuf);
      if(strcmp(filename,"#p") || strcmp(namebuf,datafile)) continue;

      printf("%s\n",dir_ent->d_name);
      }
    closedir(dir_ptr);
    }
  }
