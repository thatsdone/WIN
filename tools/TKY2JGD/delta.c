/* delta.c */
/* 2005.1.12  urabe */
/* get geometrical distance in km (y,x,d) between two points */
/*
input  : 35.11590 139.07274 35.11261 139.07589 E.ATA 熱海
output : 35.11590 139.07274 35.11261 139.07589 E.ATA 熱海  0.366 -0.286  0.464
*/
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/*  this program was translated from HYPOMH(HIRATA and MATSU'URA) */
/*  PLTXY TRANSFORMS (X,Y) TO (ALAT,ALONG) IF IND.EQ.1  */
/*  PLTXY TRANSFORMS (ALAT,ALONG) TO (X,Y) IF IND.EQ.0  */
pltxy(alt0,alng0,alat,along,x,y,ind)
  double alt0,alng0,*alat,*along,*x,*y;
  int ind;
  {
  static double a=6.378160e3,e2=6.6944541e-3,e12=6.7395719e-3,d=5.72958e1;
  double rd,rlat,slat,clat,v2,al,ph1,rph1,rph2,an,c1,c2,rlato,slato,
    tphi1,cphi1,r;
  rd=1.0e0/d;
  if(ind==0)
    {
    rlat=rd*(*alat);
    slat=sin(rlat);
    clat=cos(rlat);
    v2=1.0e0+e12*clat*clat;
    al=(*along)-alng0;
    ph1=(*alat)+(v2*al*al*slat*clat)/(2.0e0*d);
    rph1=ph1*rd;
    rph2=(ph1+alt0)*0.5e0*rd;
    r=a*(1.0e0-e2)/sqrt(pow(1.0e0-e2*pow(sin(rph2),2.0),3.0));
    an=a/sqrt(1.0e0-e2*pow(sin(rph1),2.0));
    c1=d/r;
    c2=d/an;
    *y=(ph1-alt0)/c1;
    *x=(al*clat)/c2+(al*al*al*clat*cos(2.0e0*rlat))/(6.0e0*c2*d*d);
    }
  else
    {
    rlato=alt0*rd;
    slato=sin(rlato);
    r=a*(1.0e0-e2)/sqrt(pow(1.0e0-e2*slato*slato,3.0));
    an=a/sqrt(1.0e0-e2*slato*slato);
    v2=1.0e0+e12*pow(cos(rlato),2.0);
    c1=d/r;
    c2=d/an;
    ph1=alt0+c1*(*y);
    rph1=ph1*rd;
    tphi1=tan(rph1);
    cphi1=cos(rph1);
    *alat=ph1-(c2*(*x))*(c2*(*x))*v2*tphi1/(2.0e0*d);
    *along=alng0+c2*(*x)/cphi1-pow(c2*(*x),3.0)*
      (1.0e0+2.0e0*tphi1*tphi1)/(6.0e0*d*d*cphi1);
    }
  }

main(argc,argv)
  int argc;
  char **argv;
  {
  double alat0,along0,lat1,lon1,lat2,lon2,x1,y1,x2,y2,d;
  unsigned char st[20],name[20],tb[256];

  if(argc>=3)
    {
    alat0=strtod(argv[1],NULL);
    along0=strtod(argv[2],NULL);
    }
  else
    {
    alat0=35.5;
    along0=139.5;
    }

  while(fgets(tb,256,stdin))
    {
    sscanf(tb,"%lf%lf%lf%lf%s%s",&lat2,&lon2,&lat1,&lon1,st,name);
    pltxy(alat0,along0,&lat2,&lon2,&x2,&y2,0);
/*  printf("%f %f %f %f\n",lat2,lon2,x2,y2);*/
    pltxy(alat0,along0,&lat1,&lon1,&x1,&y1,0);
/*  printf("%f %f %f %f\n",lat1,lon1,x1,y1);*/
    tb[strlen(tb)-1]=0;
    d=sqrt((y2-y1)*(y2-y1)+(x2-x1)*(x2-x1));
    printf("%s %6.3f %6.3f %6.3f\n",tb,y2-y1,x2-x1,d);
    }
  }
