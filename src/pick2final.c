/* $Id: pick2final.c,v 1.4 2004/11/24 06:43:42 uehira Exp $ */
/* pick2final.c */
/* 8/22/91, 5/22/92, 7/9/92, 97.10.3 urabe */
/* input (stdin)   : a list of pick file names (ls -l) */
/* output (stdout) : first line of HYPOMH output + owner of pick file */

/* 2004/11/24  get owner information from pickfile (uehira) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <string.h>

#include "subst_func.h"

main()
  {
  FILE *fp;
  int flag;
  char tbuf[1024],fname[256],buf[1024],owner[20],item[10][256],diag[256];
  int i;
  while(fgets(tbuf,sizeof(tbuf),stdin))
    {
    i=sscanf(tbuf,"%255s%255s%255s%255s%255s%255s%255s%255s%255s%255s",
	     item[0],item[1],item[2],item[3],item[4],
	     item[5],item[6],item[7],item[8],item[9]);
    /*  strcpy(owner,item[2]);   */
    strcpy(fname,item[i-1]);
    if((fp=fopen(fname,"r"))==NULL) continue;
    flag=1;
    while(fgets(buf,sizeof(buf),fp))
      {
      if(flag && strncmp(buf,"#p",2)==0)
        {
        *diag=0;
	*owner=0;
        sscanf(buf+3,"%255s%255s%19s",item[0],diag,owner);
        flag=0;
        }
      if(strncmp(buf,"#f",2)==0)
        {
        sscanf(buf+3,"%255s%255s%255s%255s%255s%255s%255s%255s%255s%255s",
          item[0],item[1],item[2],item[3],item[4],
          item[5],item[6],item[7],item[8],item[9]);
        fprintf(stdout,"%s %s %s %s %s %s %s %s %s %s %s %s\n",
          item[0],item[1],item[2],item[3],item[4],
          item[5],item[6],item[7],item[8],item[9],owner,diag);
        break;
        }
      }
    fclose(fp);
    }
  }
