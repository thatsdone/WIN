/* map2tky2jgd_c.c */
/* 2005.1.20 urabe */
/* convert from WIN map file to tky2jgd input file */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
main()
  {
  unsigned char lat[20],lon[20],tb[256];
  int i;
  double latit,longit,lat_min,lat_sec,lon_min,lon_sec;

  while(fgets(tb,256,stdin))
    {
    if(*tb=='#')
      {
      printf("#%s",tb);
      continue;
      }
    *lat=*lon=0;
    sscanf(tb,"%s%s",lat,lon);
    latit=atof(lat);
    longit=atof(lon);
    if(latit>=999.0 || longit>=999.9)
      {
      printf("#%s",tb);
      continue;
      }

/*    printf("%f %f\n",latit,longit);*/
    lat_min=60.0*(latit-(int)latit);
    lat_sec=60.0*(lat_min-(int)lat_min);
    lon_min=60.0*(longit-(int)longit);
    lon_sec=60.0*(lon_min-(int)lon_min);
    printf("%d%02d%07.4f %d%02d%07.4f %s %s\n",
      (int)latit,(int)lat_min,lat_sec,(int)longit,(int)lon_min,lon_sec,
      lat,lon);
    }
  }
