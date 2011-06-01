/* $Id: pick2finalb.c,v 1.5 2011/06/01 11:09:21 uehira Exp $ */
/* pick2finalb.c */
/* 8/22/91, 5/22/92, 7/9/92, 8/19/92, 5/25/93, 6/1/93 urabe */
/* 97.10.3 FreeBSD */
/* 99.4.19 byte-order-free */
/* input (stdin)   : a list of pick file names (ls -l) */
/* output (stdout) : binary format of hypo data */

/* 2004/11/24  get owner information from pickfile (uehira) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <ctype.h>

#include "winlib.h"

static const char rcsid[] =
   "$Id: pick2finalb.c,v 1.5 2011/06/01 11:09:21 uehira Exp $";

/* prototypes */
int main(void);

int
main(void)
  {
  FILE *fp;
  int flag;
  char tbuf[1024],fname[256],buf[1024],owner[20],diag[256],diagsave[256],
    item[256],tb[10][256];
  char *ptr;
  int i,tm[6],tmc[7];
  double se,sec,mag;
  /* struct { */
  /*   char time[8]; /\* Y,M,D,h,m,s,s10,mag10 *\/ */
  /*   float alat,along,dep; */
  /*   char diag[4],owner[4]; */
  /*   } d; */
  struct FinalB  d;      /* 28 bytes / event */

  while(fgets(tbuf,sizeof(tbuf),stdin) != NULL)
    {
      i=sscanf(tbuf,"%255s%255s%255s%255s%255s%255s%255s%255s%255s%255s",
	       tb[0],tb[1],tb[2],tb[3],tb[4],tb[5],tb[6],tb[7],tb[8],tb[9]);
    /*  strcpy(owner,tb[2]); */
    strcpy(fname,tb[i-1]);
    if((fp=fopen(fname,"r"))==NULL) continue;
    flag=1;
    while(fgets(buf,sizeof(buf),fp) != NULL)
      {
      if(flag && strncmp(buf,"#p",2)==0)
        {
        *diag=0;
	*owner=0;
        sscanf(buf+3,"%255s%255s%19s",item,diag,owner);
	strcpy(diagsave,diag);
	for(ptr=diag;*ptr;ptr++) *ptr=toupper(*ptr);
        if(strcmp(diag,"NOISE")==0) break;
        flag=0;
        }
      if(strncmp(buf,"#f",2)==0)
        {
        sscanf(buf+3,"%d%d%d%d%d%lf%f%f%f%lf",
          &tm[0],&tm[1],&tm[2],&tm[3],&tm[4],&se,
          &d.alat,&d.along,&d.dep,&mag);
        adj_sec(tm,&se,tmc,&sec);
        d.time[0]=(int8_w)tmc[0];
        d.time[1]=(int8_w)tmc[1];
        d.time[2]=(int8_w)tmc[2];
        d.time[3]=(int8_w)tmc[3];
        d.time[4]=(int8_w)tmc[4];
        d.time[5]=(int8_w)((int)sec);
        d.time[6]=(int8_w)(((int)(sec*10.0))%10);
        d.time[7]=(int8_w)((int)(mag*10.0+0.5));
        *d.diag=(*d.owner)=0;
        if(*diagsave) strncpy(d.diag,diagsave,4);
        if(*owner) strncpy(d.owner,owner,4);
/*         i=1;if(*(char *)&i) */
/*           { */
/*           SWAPF(d.alat); */
/*           SWAPF(d.along);    */
/*           SWAPF(d.dep); */
/*           } */
/*         fwrite(&d,sizeof(d),1,stdout); */
	(void)FinalB_write(d, stdout);
        break;
        }
      }
    fclose(fp);
    }
  exit(0);
  }
