/* final2tky2jgd.c */
/* 2005.1.20 urabe */
/* convert from WIN final file to tky2jgd input file */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
main()
  {
  unsigned char ye[20],mo[20],da[20],ho[20],mi[20],se[20],
    lat[20],lon[20],dep[20],mag[20],name[20],label[20],tb[256];
  int i;
  double latit,longit,lat_min,lat_sec,lon_min,lon_sec;

  while(fgets(tb,256,stdin))
    {
    *lat=*lon=0;
    sscanf(tb,"%s%s%s%s%s%s%s%s%s%s%s%s",
      ye,mo,da,ho,mi,se,lat,lon,dep,mag,name,label,tb);
    latit=atof(lat);
    longit=atof(lon);
/*    printf("%f %f\n",latit,longit);*/
    lat_min=60.0*(latit-(int)latit);
    lat_sec=60.0*(lat_min-(int)lat_min);
    lon_min=60.0*(longit-(int)longit);
    lon_sec=60.0*(lon_min-(int)lon_min);
    printf("%d%02d%07.4f %d%02d%07.4f %s",
      (int)latit,(int)lat_min,lat_sec,(int)longit,(int)lon_min,lon_sec,tb);
    }
  }
