/* tky2jgd2ch2.c (tky2jgd_c3.c) */
/* 2005.1.11  urabe */
/* replace latitude and longitude in WIN channel table */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
main(argc,argv)
  int argc;
  char **argv;
  {
  unsigned char c[30][1024],lat1[20],lon1[20],lat2[20],lon2[20],
    s1[1024],s2[1024],tb[1024],tb2[1024];
  int i;
  FILE *fp;

  if(NULL==(fp=fopen(argv[1],"r"))){
    fprintf(stderr,"Cannot open file: %s\n",argv);
    exit(1);
    }

  while(fgets(tb,sizeof(tb),stdin))
    {
    if(*tb==' ' || *tb=='#')
      {printf("%s",tb);continue;}
    for(i=0;i<21;i++) c[i][0]=0;
    sscanf(tb,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
      c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8],c[9],c[10],
      c[11],c[12],c[13],c[14],c[15],c[16],c[17],c[18],c[19],c[20]);
    if(c[14][0]==0) goto next;

    rewind(fp);
    while(!feof(fp)){
      if(fgets(tb2,sizeof(tb2),fp)==NULL) continue;
      sscanf(tb2,"%s%s%s%s%s%s",lat1,lon1,lat2,lon2,s1,s2);
      if(strcmp(lat2,c[13])==0 && strcmp(lon2,c[14])==0)
        {
        strcpy(c[13],lat1); 
        strcpy(c[14],lon1); 
        goto next;
        }
      }
    fprintf(stderr,"Not found :\n%s",tb);
    exit(1);

next:
    printf("%s",c[0]);
    for(i=1;i<21;i++)
      {
      if(c[i][0]==0) break;
      else printf(" %s",c[i]);
      }
    printf("\n");
    }
  }
