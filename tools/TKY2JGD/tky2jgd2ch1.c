/* tky2jgd2ch1.c (tky2jgd_c2.c) */
/* 2005.1.11  urabe */
/* 2005.1.20  urabe strlcpy()->strncpy() */
/* convert tky2jgd output file to unit of degree */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
main()
  {
  unsigned char tb[1024],lat1[1024],lon1[1024],lat2[1024],lon2[1024],st[1024],name[1000],
     tt[1024];
  double latit,longit,lat_min,lat_sec,lon_min,lon_sec;

  while(fgets(tb,sizeof(tb),stdin))
    {
    sscanf(tb,"%s%s%s%s%s%s",lat1,lon1,lat2,lon2,st,name);

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

    printf("%s %s %s %s\n",lat2,lon2,st,name);
    }
  }
