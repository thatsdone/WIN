/* $Id: final2tky2jgd.c,v 1.2 2005/02/02 15:49:27 uehira Exp $ */

/* final2tky2jgd.c */
/* 2005.1.20 urabe */
/* convert from WIN final file to tky2jgd input file */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
main()
  {
  unsigned char ye[1024],mo[1024],da[1024],ho[1024],mi[1024],se[1024],
    lat[1024],lon[1024],dep[1024],mag[1024],name[1024],label[1024],tb[1024];
  int i;
  double latit,longit,lat_min,lat_sec,lon_min,lon_sec;

  while(fgets(tb,sizeof(tb),stdin))
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
