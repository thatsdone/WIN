/* $Id: file_older.c,v 1.3 2002/01/13 06:57:50 uehira Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "subst_func.h"

/*
	echo [file2] | file_older [file1]
	echo [file2] [file1] | file_older
	file_older [file1] [file2]
		if file2 is older than file1, returns 0, otherwise 1.
		file_older prints name of older file
*/
main(argc,argv)
	int argc;
	char *argv[];
	{
	time_t time1,time2;
	char path1[100],path2[100];
	struct stat buf;
	if(argc<2)
		{
		scanf("%s%s",path2,path1);
		}
	else if(argc<3)
		{
		scanf("%s",path2);
		strcpy(path1,argv[1]);
		}
	else
		{
		strcpy(path1,argv[1]);
		strcpy(path2,argv[2]);
		}
	if(stat(path1,&buf)<0)
		{
/*		fprintf(stderr,"'%s' not exists\n",path1);*/
		printf("%s\n",path1);
		exit(1);	/* returns 'false' */
		}
	time1=buf.st_mtime;
	if(stat(path2,&buf)<0)
		{
/*		fprintf(stderr,"'%s' not exists\n",path2);*/
		printf("%s\n",path2);
		exit(0);	/* returns 'true' */
		}
	time2=buf.st_mtime;
	if(time1>time2)
		{
/*		fprintf(stderr,"'%s'(%d) > '%s'(%d)\n",path1,time1,path2,time2);*/
		printf("%s\n",path2);
		exit(0);	/* returns 'true' */
		}
	else if(time1<time2)
		{
/*		fprintf(stderr,"'%s'(%d) < '%s'(%d)\n",path1,time1,path2,time2);*/
		printf("%s\n",path1);
		exit(1);	/* returns 'false' */
		}
	else
		{
/*		fprintf(stderr,"'%s'(%d) = '%s'(%d)\n",path1,time1,path2,time2);*/
		printf("\n");
		exit(1);	/* returns 'false' */
		}
	}
