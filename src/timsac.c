/* $Id: timsac.c,v 1.1 2012/01/29 03:09:16 uehira Exp $ */

/* C version of TIMSAC, by Akaike & Nakagawa (1972) */

/***************************************************************************/
/*									   */
/*  ［以下の文は削除しないでください］                                     */
/*  プログラムの作成にあたっては、                                         */
/*  赤池・中川「ダイナミックシステムの統計的解析と制御」サイエンス社(1972) */
/*  を参考にしました。                                                     */
/*									   */
/***************************************************************************/
/*									   */
/*  This software is based on the book;                                    */
/*  Nakagawa and Akaike, Statistical Analysis and Control of Dynamic       */
/*  Systems, Saiensu-sha (1972).                                           */
/*  (Don't delete the above notice.)                                       */
/*									   */
/***************************************************************************/

#include "winlib.h"
#include "timsac.h"

static void crosco(double *, double *, int, double *, int);

void
autcor(double *x, int n, int lagh, double *cxx, double *z)
  /* double *x;     original data */
  /* int n;         N of original data */
  /* int lagh;      largest lag */
  /* double *cxx;   (output) autocovariances (cxx[0] - cxx[lagh]) */
  /* double *z;      mean level */
  {

  /* mean deletion */
  smeadl(x,n,z);
  /* auto covariance computation */
  crosco(x,x,n,cxx,lagh+1);
  }

void
smeadl(double *x,int n, double *xmean)
  {
  int i;
  double xm;

  xm=0.0;
  for(i=0;i<n;i++) xm+=x[i];
  xm/=(double)n;
  for(i=0;i<n;i++) x[i]-=xm;
  *xmean=xm;
  }

int
fpeaut(int l, int n, int lagh, double *cxx, double *ofpe, double *osd,
       int *mo, double *ao)
  /* int l;          upper limit of model order */
  /* int n;          n of original data */
  /* int lagh;       highest lag */
  /* double *cxx;    autocovariances, cxx[0]-cxx[lagh] */
  /* double *ofpe;   minimum FPE */
  /* double *osd;    SIGMA^2, variance of residual for minimum FPE */
  /* int *mo;        model order for minimum FPE */
  /* double *ao;     coefficients of AR process, ao[0]-ao[mo-1] */
  {
  double sd,an,anp1,anm1,oofpe,se,d,orfpe,d2,fpe,rfpe,chi2;
  double  *a, *b;  /* OLD : a[500], b[500] */
  int  abnum;
  int np1,nm1,m,mp1,i,im,lm;

  sd=cxx[0];
  an=n;
  np1=n+1;
  nm1=n-1;
  anp1=np1;
  anm1=nm1;
  *mo=0;
  *osd=sd;
  if(sd==0.0) return (0);
  *ofpe=(anp1/anm1)*sd;
  oofpe=1.0/(*ofpe);
  orfpe=1.0;
  se=cxx[1];

  if (l > 0)
    abnum = l;
  else
    abnum = 8;
  /* fprintf(stderr,"l=%d abnum=%d\n", l,abnum); */
  if ((a = (double *)win_xmalloc(sizeof(double) * abnum)) == NULL)
    return (1);
    /* emalloc("fpeaut : a"); */
  if ((b = (double *)win_xmalloc(sizeof(double) * abnum)) == NULL)
    /* emalloc("fpeaut : b"); */
    return (2);

  for(m=1;m<=l;m++)
    {
    mp1=m+1;
    d=se/sd;
    a[m-1]=d;
    d2=d*d;
    sd=(1.0-d2)*sd;
    anp1=np1+m;
    anm1=nm1-m;
    fpe=(anp1/anm1)*sd;
    rfpe=fpe*oofpe;
    chi2=d2*anm1;
    if(m>1)
      {
      lm=m-1;
      for(i=0;i<lm;i++) a[i]=a[i]-d*b[i];
      }
    for(i=1;i<=m;i++)
      {
      im=mp1-i;
      b[i-1]=a[im-1];
      }
/*fprintf(stderr,"(%f %f %d) ",fpe,sd,m);*/
    if(*ofpe>=fpe)
      {
      *ofpe=fpe;
      orfpe=rfpe;
      *osd=sd;
      *mo=m;
      for(i=0;i<m;i++) ao[i]=a[i];
      }
    if(m<l)
      {
      se=cxx[mp1];
      for(i=0;i<m;i++) se=se-b[i]*cxx[i+1];
      }
    }
/*fprintf(stderr,"\n");*/
  FREE(a);
  FREE(b);

  return (0);
  }

static void
crosco(double *x, double *y, int n, double *c, int lagh1)
  {
  int i,j,il;
  double t,bn;

  bn=1.0/(double)n;
  for(i=0;i<lagh1;i++)
    {
    t=0.0;
    il=n-i;
    for(j=0;j<il;j++)
      {
      t+=x[j+i]*y[j];
      }
    c[i]=t*bn;
    }
  }
