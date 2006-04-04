/* $Id: pick2finalb_SEVO.c,v 1.1.2.1 2006/04/04 07:47:24 uehira Exp $ */
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
#include  <string.h>
#include  <ctype.h>

#include "subst_func.h"

#define SWAPF(a) *(long *)&(a)=(((*(long *)&(a))<<24)|\
  ((*(long *)&(a))<<8)&0xff0000|((*(long *)&(a))>>8)&0xff00|\
  ((*(long *)&(a))>>24)&0xff)  

adj_sec(tm,se,tmc,sec)
  int *tm,*tmc;
  double *se,*sec;
  {
  int i,j;
  for(i=0;i<5;i++) tmc[i]=tm[i];
  if((*sec=(*se))<0.0)
    {
    tmc[5]=0;
    i=(int)(-(*sec));
    if((double)i==(-(*sec))) i--;
    i++;
    for(j=0;j<i;j++) {tmc[5]--;adj_time(tmc);}
    *sec+=(double)(tmc[5]+i);
    }
  else tmc[5]=(int)(*sec);
  tmc[6]=(int)(*sec*1000.0)%1000;
  }

adj_time(tm)
  int *tm;
  {
  if(tm[5]==60)
    {
    tm[5]=0;
    if(++tm[4]==60)
      {
      tm[4]=0;
      if(++tm[3]==24)
        {
        tm[3]=0;
        tm[2]++;
        switch(tm[1])
          {
          case 2:
            if(tm[0]%4==0) {if(tm[2]==30) {tm[2]=1;tm[1]++;}break;}
            else {if(tm[2]==29) {tm[2]=1;tm[1]++;}break;}
          case 4:
          case 6:
          case 9:
          case 11:  if(tm[2]==31) {tm[2]=1;tm[1]++;}break;
          default:  if(tm[2]==32) {tm[2]=1;tm[1]++;}break;
          }
        if(tm[1]==13)
          {
          tm[1]=1;
          if(++tm[0]==100) tm[0]=0;
          }
        }
      }
    }
  else if(tm[5]==-1)
    {
    tm[5]=59;
    if(--tm[4]==-1)
      {
      tm[4]=59;
      if(--tm[3]==-1)
        {
        tm[3]=23;
        if(--tm[2]==0)
          {
          switch(--tm[1])
            {
            case 2: if(tm[0]%4==0) tm[2]=29;else tm[2]=28;break;
            case 4:
            case 6:
            case 9:
            case 11:  tm[2]=30;break;
            default:  tm[2]=31;break;
            }
          if(tm[1]==0)
            {
            tm[1]=12;
            if(--tm[0]==-1) tm[0]=99;
            }
          }
        }
      }
    }
  return 0;
  }

main()
  {
  FILE *fp;
  int flag;
  char tbuf[1024],fname[256],buf[1024],owner[20],diag[256],diagsave[256],
    item[256],tb[10][256];
  char *ptr;
  unsigned int ye,mo,da,ho,mi;
  int i,tm[6],tmc[7];
  double se,sec,mag;
  int stnum,stsnum;
  float prms,srms,inide;
  struct {
    char time[8]; /* Y,M,D,h,m,s,s10,mag10 */
    float alat,along,dep;
    char diag[4],owner[4];
    } d;      /* 28 bytes / event */

  while(fgets(tbuf,sizeof(tbuf),stdin))
    {
      i=sscanf(tbuf,"%255s%255s%255s%255s%255s%255s%255s%255s%255s%255s",
	       tb[0],tb[1],tb[2],tb[3],tb[4],tb[5],tb[6],tb[7],tb[8],tb[9]);
    /*  strcpy(owner,tb[2]); */
    strcpy(fname,tb[i-1]);
    if((fp=fopen(fname,"r"))==NULL) continue;
    flag=1;
    while(fgets(buf,sizeof(buf),fp))
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
        d.time[0]=(unsigned char)tmc[0];
        d.time[1]=(unsigned char)tmc[1];
        d.time[2]=(unsigned char)tmc[2];
        d.time[3]=(unsigned char)tmc[3];
        d.time[4]=(unsigned char)tmc[4];
        d.time[5]=(unsigned char)((int)sec);
        d.time[6]=(unsigned char)(((int)(sec*10.0))%10);
        d.time[7]=(int)(mag*10.0+0.5);
        *d.diag=(*d.owner)=0;
        if(*diagsave) strncpy(d.diag,diagsave,4);
        if(*owner) strncpy(d.owner,owner,4);
        i=1;if(*(char *)&i)
          {
          SWAPF(d.alat);
          SWAPF(d.along);   
          SWAPF(d.dep);
          }
	/* Modify for SEVO after Dr. Kodo Umakoshi */
	fgets(buf,255,fp); fgets(buf,255,fp); fgets(buf,255,fp);
	fgets(buf,255,fp);
	sscanf(buf+3,"%d", &stnum);
	sscanf(buf+25,"%d", &stsnum);
	sscanf(buf+43,"%f", &inide);
	for (i=1; i<=stnum; i++)
	  fgets(buf,255,fp);
	fgets(buf,255,fp);
	sscanf(buf+3,"%f%f", &prms,&srms);
	/*  fprintf(stderr, "%s: prms=%lf, stnum=%d, srms=%lf, inide=%lf\n", */
	/*  		fname, prms, stnum, srms, inide); */
	if (strncmp(owner,"auto",4) != 0)
	  fwrite(&d,sizeof(d),1,stdout);
	else if (prms<=1.0 && stnum>=5 && stsnum>= 1 && srms<=5.0 && inide<=5.0)
	  fwrite(&d,sizeof(d),1,stdout);
        break;
        }
      }
    fclose(fp);
    }
  }
