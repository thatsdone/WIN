/* tky2jgd2map.c */
/* 2005.1.20  urabe */
/* convert tky2jgd output file to WIN map file */
/* Delete header comments (maybe ten lines) before running this ! */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
main()
  {
  unsigned char tb[256],lat1[20],lon1[20],lat2[20],lon2[20],tt[20];
  double latit,longit,lat_min,lat_sec,lon_min,lon_sec;

  while(fgets(tb,256,stdin))
    {
    if(*tb=='#')
      {
      printf("%s",tb+1);
      continue;
      }

    sscanf(tb,"%s%s%s%s",lat1,lon1,lat2,lon2);

    strncpy(tt,lat1,2);
    tt[2]=0;
    latit=strtod(tt,NULL);
    strncpy(tt,lat1+2,2);
    tt[2]=0;
    lat_min=strtod(tt,NULL);
    lat_sec=strtod(lat1+4,NULL);
    latit+=lat_min/60.0;
    latit+=lat_sec/(60.0*60.0);

    switch (strlen(lat2))
      {
      case 6:
        printf("%6.3f ",latit);break;
      case 7:
        printf("%7.4f ",latit);break;
      case 8:
        printf("%8.5f ",latit);break;
      case 9:
        printf("%9.6f ",latit);break;
      default:
        printf("%6.3f ",latit);break;
      }

    strncpy(tt,lon1,4);
    tt[3]=0;
    longit=strtod(tt,NULL);
    strncpy(tt,lon1+3,2);
    tt[2]=0;
    lon_min=strtod(tt,NULL);
    lon_sec=strtod(lon1+5,NULL);
    longit+=lon_min/60.0;
    longit+=lon_sec/(60.0*60.0);

    switch (strlen(lon2))
      {
      case 7:
        printf("%7.3f ",longit);break;
      case 8:
        printf("%8.4f ",longit);break;
      case 9:
        printf("%9.5f ",longit);break;
      case 10:
        printf("%10.6f ",longit);break;
      default:
        printf("%7.3f ",longit);break;
      }

    printf("\n");
    }
  }
