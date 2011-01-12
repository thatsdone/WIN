/* $Id: select_stations.c,v 1.3.8.5 2011/01/12 15:44:30 uehira Exp $ */

/* select_stations  1999.11.9  urabe */
/* debugged 2004.1.28  urabe */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pltxy.h"
#include "subst_func.h"

#define NAME_PRG "select_stations"
/* #define DEBUG 0 */

static const char rcsid[] =
  "$Id: select_stations.c,v 1.3.8.5 2011/01/12 15:44:30 uehira Exp $";

/* prototypes */
static void print_usage(void);
static void get_dist_and_azim(double, double, double, double, 
			      double *, double *);
int main(int, char *[]);

static void
print_usage(void)
  {

  fprintf(stderr,"%s\n", rcsid);
  fprintf(stderr,"usage of '%s' :\n",NAME_PRG);
  fprintf(stderr,"   %s [stations_file] [polygon_file]\n",NAME_PRG);
  fprintf(stderr,"       stationss_file : WIN's ch file lines with Lat/Long\n");
  fprintf(stderr,"       polygon_file: Lat and Long of vertexes, clockwise !\n");
  fprintf(stderr,"                     Polygon must be closed.\n");
  }

/* example of polygon_file
39 136
39 139
38 140
38 143
20 143
20 137
34 137
35 137.5
36 137
37 136
39 136
*/

static void
get_dist_and_azim(double x0, double y0, double x1, double y1,
		  double *d, double *a)
/*  double x0,y0;    start point */
/*  double x1,y1;    end point */
/*  double *d,*a;    distance and azimuth */
  {
#define PI          3.141592654
  int i,i0,i1;
  double x00,y00,x11,y11,x2,y2,d0,d1,d2;
  x00=x0;
  y00=y0;
  x11=x1;
  y11=y1;
#if DEBUG
printf(" (%8.3f,%8.3f) (%8.3f,%8.3f) ",x0,y0,x1,y1);
#endif
 /* find nearest point on the line */
  for(i=0;i<10;i++)
    {
    d0=x0*x0+y0*y0;
    d1=x1*x1+y1*y1;
    x2=(x0+x1)*0.5;
    y2=(y0+y1)*0.5;
    d2=x2*x2+y2*y2;
    if((d1<=d2 && d2<=d0) || (d2<=d1 && d1<=d0)) /* d1,d2 */
      {
      x0=x2;
      y0=y2;
      }
    else if((d0<=d2 && d2<=d1) || (d2<=d0 && d0<=d1)) /* d0,d2 */
      {
      x1=x2;
      y1=y2;
      }
    }
  d0=x0*x0+y0*y0;
  d1=x1*x1+y1*y1;
  x2=(x0+x1)*0.5;
  y2=(y0+y1)*0.5;
  d2=x2*x2+y2*y2;
  if(d0<=d2 && d0<=d1)
    {
    *d=sqrt(d0);
    x2=x0;
    y2=y0;
    }
  if(d1<=d0 && d1<=d2)
    {
    *d=sqrt(d1);
    x2=x1;
    y2=y1;
    }
  if(d2<=d0 && d2<=d1)
    {
    *d=sqrt(d2);
    }
  if((i1=450-(int)(180.0*atan2(y11-y00,x11-x00)/PI))>=360) i1-=360;
  if((i0=450-(int)(180.0*atan2(-y00,-x00)/PI))>=360) i0-=360;
  *a=i0-i1; 
#if DEBUG
printf("(%8.3f,%8.3f) d=%8.3f,a=%8.3f\n",x2,y2,*d,*a);
#endif
#undef PI
  }

int
main(int argc, char *argv[])
  {
  FILE *f1,*f2;
  int init,init2;
  char ta[256],tb[256],name[100];
  double alat0,along0,x00,y00,d,a,dmin,amin;
  double alat1,along1,x0,y0,x1,y1,xa,ya,xb,yb;

  if(argc<3)
    {
    print_usage();
    exit(1);
    }
  f1=fopen(argv[1],"r"); /* list of points */
  f2=fopen(argv[2],"r"); /* list of a polygon */

  while(fgets(tb,256,f1)!=NULL)
    {
    if(*tb=='#') continue;
    alat0=along0=0.0;
    sscanf(tb,"%*s%*s%*s%s%*s%*s%*s%*s%*s%*s%*s%*s%*s%lf%lf",name,&alat0,&along0);
    if(alat0==0.0 || along0==0.0) continue;
#if DEBUG
    printf("%s",tb);
#endif
    rewind(f2);
    init=init2=1;
    while(fgets(ta,256,f2)!=NULL)
      {
      if(*ta=='#') continue;
      sscanf(ta,"%lf%lf",&alat1,&along1);
      pltxy(alat0,along0,&alat1,&along1,&x1,&y1,0);
      if(init)
        {
        x00=x1;
        y00=y1;
        init=0;
        }
      else
        {
#if DEBUG
printf("(%8.3f,%8.3f) (%8.3f,%8.3f)",x0,y0,x1,y1);
#endif
        get_dist_and_azim(x0,y0,x1,y1,&d,&a);
#if DEBUG
printf("d=%8.3f,a=%8.3f ,dmin=%8.3f,amin=%8.3f",d,a,dmin,amin);
#endif
        if(init2 || d<dmin ||
            (dmin==d && !(a>=0.0 && a<180.0) && !(a<-180.0 && a>=(-360.0))))
          {
          dmin=d;
          amin=a;
          xa=x0;ya=y0;xb=x1;yb=y1;
#if DEBUG
    printf("#(%5.0f %5.0f)-(%5.0f %5.0f) %3.0fkm %3.0fdeg\n",xa,ya,xb,yb,dmin,amin);
#endif
          init2=0;
          }
        }
      x0=x1;
      y0=y1;
      }
#if DEBUG
    printf("(%5.0f %5.0f)-(%5.0f %5.0f) %3.0fkm %3.0fdeg\n",xa,ya,xb,yb,dmin,amin);
#endif
    if((amin>=0.0 && amin<180.0) || (amin<-180.0 && amin>=(-360.0)))
      {
#if DEBUG
      printf("o %s",tb);
#endif
      printf("%s",tb);
      }
#if DEBUG
    else
      printf("x %s",tb);
#endif
    }

  exit(0);
  }
