/* $Log: find_picks.c,v $
/* Revision 1.1  2000/08/10 07:40:57  urabe
/* find_picks : pick file searching server for win
/* wtime      : time shift tool for WIN format file
/* */
/* find_picks */
/* search for pick files in pick dir */
/* from win.c            2000.7.31 urabe */
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#define LINELEN 256
#define NAMLEN 256

strncmp2(s1,s2,i)
char *s1,*s2;             
int i; 
{
  if(*s1=='0' && *s2=='9') return 1;
  else if(*s1=='9' && *s2=='0') return -1;
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