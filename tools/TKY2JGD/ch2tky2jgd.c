/* $Id: ch2tky2jgd.c,v 1.2 2005/02/02 15:49:27 uehira Exp $ */

/* ch2tky2jgd.c (tky2jgd_c.c) */
/* 2004.8.30  urabe */
/* 2005.1.7  urabe */
/* 2005.1.20 urabe */
/* convert from WIN channel table to tky2jgd input file */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
main()
  {
  unsigned char ch[1024],rec[1024],delay[1024],st[1024],cmp[1024],scl[1024],unit[1024],per[1024],
    dmp[1024],sensor[1024],gain[1024],tb[1024],lat[1024],lon[1024],hei[1024],scp[1024],
    scs[1024],name[1024],*prefix,*star,cmp2[1024],bits[1024],resol[1024];
  int i;
  double latit,longit,lat_min,lat_sec,lon_min,lon_sec;

  while(fgets(tb,sizeof(tb),stdin))
    {
    if(*tb==' ' || *tb=='#') continue;
    *lat=*lon=*hei=*scp=*scs=*name=0;
    sscanf(tb,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
      ch,rec,delay,st,cmp,scl,bits,sensor,unit,per,dmp,gain,resol,
      lat,lon,hei,scp,scs,name);
    if(*lon==0) continue;
    latit=atof(lat);
    longit=atof(lon);
/*    printf("%f %f\n",latit,longit);*/
    lat_min=60.0*(latit-(int)latit);
    lat_sec=60.0*(lat_min-(int)lat_min);
    lon_min=60.0*(longit-(int)longit);
    lon_sec=60.0*(lon_min-(int)lon_min);
    printf("%d%02d%07.4f %d%02d%07.4f %s %s %s %s\n",
      (int)latit,(int)lat_min,lat_sec,(int)longit,(int)lon_min,lon_sec,
      lat,lon,st,name);
    }
  }
