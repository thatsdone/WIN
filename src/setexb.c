/* $Id: setexb.c,v 1.3.2.1 2001/11/02 11:43:39 uehira Exp $ */
/* program "setexb.c"
	2/27/90, 3/8/93,1/17/94,5/27/94  urabe
        2001.6.22  add options '-p' and '-?'  uehira */

#ifdef HAVE_CONFIG_H
#include        "config.h"
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/types.h>
#include        <unistd.h>

#include        "subst_func.h"

#define         N_EXABYTE	8
#define         DEFAULT_PARAM_FILE  "wtape.prm"
#ifndef         FILENAME_MAX
#define         FILENAME_MAX 1024
#endif

	int exb_status[N_EXABYTE],n_exb;
	char exb_name[N_EXABYTE][20],
	  raw_dir[FILENAME_MAX],raw_dir1[FILENAME_MAX],
	  param_name[FILENAME_MAX];

read_param(f_param,textbuf)
        FILE *f_param;
        unsigned char *textbuf;
        {
        do      {
                if(fgets(textbuf,(FILENAME_MAX<<1)+32,f_param)==NULL) return 1;
                } while(*textbuf=='#');
        return 0;
        }

read_units(file)
        char *file;
        {
        FILE *fp;
        char tb[FILENAME_MAX],tb1[FILENAME_MAX+50];
        int i;
        for(i=0;i<n_exb;i++) exb_status[i]=0;
        sprintf(tb,"%s/%s",raw_dir1,file);
        if((fp=fopen(tb,"r"))==NULL)
                {
                sprintf(tb1,"wtape: %s",tb);
                perror(tb1);
                for(i=0;i<n_exb;i++) exb_status[i]=1;
                }
        else while(read_param(fp,tb)==0)
                {
                sscanf(tb,"%d",&i);
                if(i<n_exb && i>=0) exb_status[i]=1;
                }
        fclose(fp);
        }

write_units(file)
        char *file;
        {
        FILE *fp;
        char tb[FILENAME_MAX];
        int i;
        sprintf(tb,"%s/%s",raw_dir1,file);
        fp=fopen(tb,"w+");
        for(i=0;i<n_exb;i++) if(exb_status[i]) fprintf(fp,"%d\n",i);
        fclose(fp);
        }

init_param()
        {
	char tb[(FILENAME_MAX<<1)+32],*ptr;
        FILE *fp;

        if((fp=fopen(param_name,"r"))==NULL)
                {
                fprintf(stderr,"parameter file '%s' not found\007\n",
			param_name);
		usage();
                exit(1);
                }
        read_param(fp,tb);
		if((ptr=strchr(tb,':'))==0)
			{
			sscanf(tb,"%s",raw_dir);
			sscanf(tb,"%s",raw_dir1);
			}
		else
			{
			*ptr=0;
			sscanf(tb,"%s",raw_dir);
			sscanf(ptr+1,"%s",raw_dir1);
			}
        read_param(fp,tb);
        for(n_exb=0;n_exb<N_EXABYTE;n_exb++)
                {
                if(read_param(fp,tb)) break;
                sscanf(tb,"%s",exb_name[n_exb]);
                }

/* read exabyte mask file $raw_dir/UNITS */
        read_units("UNITS");
        write_units("_UNITS");
        }

usage()
{

  fprintf(stderr,"usage: setexb (-p [param file])\n");
  exit(1);
}

main(argc,argv)
     int argc;
     char *argv[];
	{
	int i,ch;
	extern int optind;
	extern char *optarg;

	sprintf(param_name,"%s",DEFAULT_PARAM_FILE);
	while((ch=getopt(argc,argv,"p:?"))!=-1)
	  {
	    switch (ch)
	      {
	      case 'p':   /* parameter file */
		strcpy(param_name,optarg);
		break;
	      case '?':
	      default:
		usage();
	      }
	  }
	argc-=optind;
	argv+=optind;

	init_param();
	printf("**** EXABYTES *****\n");
	printf("***  unit  use  ***\n");
	for(i=0;i<n_exb;i++)
		printf("       %d    %d\n",i,exb_status[i]);
	printf("*******************\n");
	}
