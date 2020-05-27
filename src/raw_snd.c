/* $Id: raw_snd.c,v 1.1.2.2 2020/05/27 10:11:35 uehira Exp $ */

/* "raw_snd.c"    2019.8.19-9.5 urabe */
/*                  modified from raw_shift.c */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else  /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif  /* !TIME_WITH_SYS_TIME */

#include "winlib.h"

#define DEBUG      0
#define SR         100    /* only around this sampling rate is accepted */
#define MAX_SR     (SR+20)
#define NCOEF      300
#define MAX_NCOEF  (NCOEF+10)
#define LIM       1024    /* must be power of 2 */
#define LMASK (LIM-1)
#define SR_MIN    (SR-10)
#define SR_MAX    (SR+10)
#define PERIOD    60        /* period (s) for shindo measurement */
#define MUTE_SEC  PERIOD    /* period (s) of initial mute */
#define INIT_VAL  (1.0e+8)  /* to detect initial loop */
#define FIR       1
#define HPF       2

static const char rcsid[] =
  "$Id: raw_snd.c,v 1.1.2.2 2020/05/27 10:11:35 uehira Exp $";

static char *chfile;
double ugal_per_bit[3],upb2[3];
int syslog_mode = 0, exit_status;
char *progname,*logfile;
double sd100[NCOEF]={
  7.00351972e-02, 1.06898042e-01, 1.12342515e-01, 9.60410357e-02,
  7.52659342e-02, 5.86781840e-02, 4.72839566e-02, 3.92880267e-02,
  3.32587434e-02, 2.83684755e-02, 2.42939398e-02, 2.07963499e-02,
  1.77702442e-02, 1.50944232e-02, 1.27228089e-02, 1.05879565e-02,
  8.66755836e-03, 6.91999677e-03, 5.33198005e-03, 3.87673169e-03,
  2.54484139e-03, 1.31904385e-03, 1.91465657e-04,-8.48718170e-04,
 -1.80891317e-03,-2.69538561e-03,-3.51554164e-03,-4.27233215e-03,
 -4.97334415e-03,-5.61908143e-03,-6.21734800e-03,-6.76683298e-03,
 -7.27552275e-03,-7.74076742e-03,-8.17066896e-03,-8.56160435e-03,
 -8.92171326e-03,-9.24668511e-03,-9.54461980e-03,-9.81074361e-03,
 -1.00530461e-02,-1.02664668e-02,-1.04588246e-02,-1.06249094e-02,
 -1.07723215e-02,-1.08958063e-02,-1.10027098e-02,-1.10878107e-02,
 -1.11581778e-02,-1.12086779e-02,-1.12460916e-02,-1.12654093e-02,
 -1.12731234e-02,-1.12643660e-02,-1.12453519e-02,-1.12113590e-02,
 -1.11683438e-02,-1.11117221e-02,-1.10472189e-02,-1.09703703e-02,
 -1.08867029e-02,-1.07918482e-02,-1.06911713e-02,-1.05803687e-02,
 -1.04646848e-02,-1.03398462e-02,-1.02110186e-02,-1.00739229e-02,
 -9.93368780e-03,-9.78599208e-03,-9.63596757e-03,-9.47921660e-03,
 -9.32091103e-03,-9.15654553e-03,-8.99136416e-03,-8.82072784e-03,
 -8.64997874e-03,-8.47432445e-03,-8.29922355e-03,-8.11971872e-03,
 -7.94139426e-03,-7.75912568e-03,-7.57862211e-03,-7.39460028e-03,
 -7.21288181e-03,-7.02804484e-03,-6.84599869e-03,-6.66121591e-03,
 -6.47965523e-03,-6.29573052e-03,-6.11539719e-03,-5.93307216e-03,
 -5.75463945e-03,-5.57459636e-03,-5.39867145e-03,-5.22153614e-03,
 -5.04866254e-03,-4.87500721e-03,-4.70566717e-03,-4.53601317e-03,
 -4.37062998e-03,-4.20545054e-03,-4.04439082e-03,-3.88411387e-03,
 -3.72768978e-03,-3.57270086e-03,-3.42117224e-03,-3.27181750e-03,
 -3.12539394e-03,-2.98198341e-03,-2.84082612e-03,-2.70363725e-03,
 -2.56786086e-03,-2.43714248e-03,-2.30681641e-03,-2.18279334e-03,
 -2.05794288e-03,-1.94082140e-03,-1.82142813e-03,-1.71140270e-03,
 -1.59740400e-03,-1.49466594e-03,-1.38595310e-03,-1.29070206e-03,
 -1.18711617e-03,-1.09957612e-03,-1.00090029e-03,-9.21342693e-04,
 -8.27288175e-04,-7.56067088e-04,-6.66248532e-04,-6.03856751e-04,
 -5.17747138e-04,-4.64911054e-04,-3.81756351e-04,-3.39607374e-04,
 -2.58254716e-04,-2.28665359e-04,-1.47186546e-04,-1.33499824e-04,
 -4.82640007e-05,-5.71204162e-05, 3.98560888e-05,-7.09400668e-06,
  1.23812521e-04,-7.54371358e-06, 2.43014604e-04,-1.50177401e-04,
  3.67177340e-04, 9.47284173e-06,-5.25322129e-04, 4.82424043e-05,
  6.34502472e-04, 1.06841452e-03, 1.23446634e-03, 1.36338573e-03,
  1.40894859e-03, 1.47504351e-03, 1.49422210e-03, 1.53242874e-03,
  1.53715474e-03, 1.55809791e-03, 1.55411596e-03, 1.56344188e-03,
  1.55360522e-03, 1.55452182e-03, 1.54053163e-03, 1.53514449e-03,
  1.51813019e-03, 1.50791078e-03, 1.48866626e-03, 1.47469584e-03,
  1.45380657e-03, 1.43691060e-03, 1.41482330e-03, 1.39565012e-03,
  1.37271441e-03, 1.35178457e-03, 1.32827945e-03, 1.30601799e-03,
  1.28216945e-03, 1.25892786e-03, 1.23492104e-03, 1.21099278e-03,
  1.18698073e-03, 1.16261244e-03, 1.13872252e-03, 1.11412238e-03,
  1.09046111e-03, 1.06580519e-03, 1.04246202e-03, 1.01789926e-03,
  9.94949378e-04, 9.70605518e-04, 9.48112245e-04, 9.24093013e-04,
  9.02109581e-04, 8.78503332e-04, 8.57074376e-04, 8.33954309e-04,
  8.13117052e-04, 7.90543098e-04, 7.70328314e-04, 7.48348782e-04,
  7.28781568e-04, 7.07434589e-04, 6.88534990e-04, 6.67849803e-04,
  6.49633308e-04, 6.29631426e-04, 6.12109362e-04, 5.92805622e-04,
  5.75985460e-04, 5.57388997e-04, 5.41274586e-04, 5.23389724e-04,
  5.07981470e-04, 4.90808547e-04, 4.76103534e-04, 4.59639676e-04,
  4.45631760e-04, 4.29871586e-04, 4.16551452e-04, 4.01487743e-04,
  3.88842938e-04, 3.74467253e-04, 3.62482203e-04, 3.48785442e-04,
  3.37441463e-04, 3.24414398e-04, 3.13689694e-04, 3.01323441e-04,
  2.91193103e-04, 2.79479569e-04, 2.69915572e-04, 2.58847844e-04,
  2.49819054e-04, 2.39391760e-04, 2.30863941e-04, 2.21073565e-04,
  2.13009389e-04, 2.03854556e-04, 1.96213628e-04, 1.87695356e-04,
  1.80434234e-04, 1.72556151e-04, 1.65628373e-04, 1.58396922e-04,
  1.51753025e-04, 1.45177644e-04, 1.38765178e-04, 1.32858481e-04,
  1.26622002e-04, 1.21399959e-04, 1.15280990e-04, 1.10763128e-04,
  1.04700082e-04, 1.00909727e-04, 9.48377510e-05, 9.18023348e-05,
  8.56530661e-05, 8.34045314e-05, 7.71057148e-05, 7.56810667e-05,
  6.91559834e-05, 6.85980527e-05, 6.17646812e-05, 6.21231916e-05,
  5.48929914e-05, 5.62260638e-05, 4.85022212e-05, 5.08785133e-05,
  4.25534016e-05, 4.60551865e-05, 3.70066660e-05, 4.17343223e-05,
  3.18202750e-05, 3.78989578e-05, 2.69490665e-05, 3.45388440e-05,
  2.23419212e-05, 3.16536200e-05, 1.79374716e-05, 2.92583082e-05,
  1.36565146e-05, 2.73933116e-05, 9.38790625e-06, 2.61436022e-05,
  4.96084843e-06, 2.56776205e-05, 8.76683248e-08, 2.63286902e-05,
 -5.75294269e-06, 2.87431850e-05,-1.34487950e-05, 3.37358582e-05,
 -2.29513092e-05, 3.76810390e-05,-2.31251409e-05, 2.12738481e-05
};

/* prototypes */
static void read_chfile(WIN_ch);
static void usage(void);
int main(int, char *[]);

static void
read_chfile(WIN_ch ch_base)
  {
  FILE *fp;
  int i,j,sysch,chidx;
  char tbuf[1024],unit[32];
  double sens,g,adc;

  ugal_per_bit[0]=ugal_per_bit[1]=ugal_per_bit[2]=0.0;
  if((fp=fopen(chfile,"r"))!=NULL)
    {
#if DEBUG
    fprintf(stderr,"ch_file=%s\n",chfile);
#endif
    i=j=0;
    while(fgets(tbuf,sizeof(tbuf),fp))
      {
      if(*tbuf=='#' || sscanf(tbuf,"%x%*d%*d%*s%*s%*d%*s%lf%s%*f%*f%lf%lf",
        &sysch,&sens,unit,&g,&adc)<5) continue;
      sysch&=0xffff;
      if(sysch>=ch_base && sysch<=ch_base+2)
	{
        chidx=sysch-ch_base;
	if(strcmp(unit,"m/s/s")==0)
          ugal_per_bit[chidx]=1.0e+8*fabs(adc/sens)/pow(10.0,(g/20.0));
	else if(strcmp(unit,"gal")==0 || strcmp(unit,"cm/s/s")==0)
          ugal_per_bit[chidx]=1.0e+6*fabs(adc/sens)/pow(10.0,(g/20.0));
#if DEBUG
        fprintf(stderr,"ch %04X chidx=%d sens=%.4f %s g=%.0f adc=%.4e upb=%.0f\n",
          sysch,chidx,sens,unit,g,adc,ugal_per_bit[chidx]);
#endif
        j++;
        }
      i++;
      }
    snprintf(tbuf,sizeof(tbuf),"%d valid channels in %d lines in chfile %s",
      j,i,chfile);
    if(ugal_per_bit[0]==0.0)
      {
      if(ugal_per_bit[1]>0.0) ugal_per_bit[0]=ugal_per_bit[1];
      if(ugal_per_bit[2]>0.0) ugal_per_bit[0]=ugal_per_bit[2];
      }
    else
      {
      if(ugal_per_bit[1]==0.0) ugal_per_bit[1]=ugal_per_bit[0];
      if(ugal_per_bit[2]==0.0) ugal_per_bit[2]=ugal_per_bit[0];
      }
    write_log(tbuf);
    fclose(fp);
    }
  else
    {
    snprintf(tbuf,sizeof(tbuf),"channel list file '%s' not open",chfile);
    write_log(tbuf);
    write_log("end");
    exit(1);
    }
  }

static void
usage(void)
{

  WIN_version();
  fprintf(stderr,"%s\n",rcsid);
  fprintf(stderr,
" usage : '%s (-ahnop) [in_key] [ch_base] [out_key] [shm_size(KB)] [ch_out]\\\n",
	  progname);
  fprintf(stderr,"                       ([ch_file] ([log file]))'\n");
  exit(1);
}

int
main(int argc, char *argv[])
  {
  struct Shm  *shr,*shm;
  key_t rawkey,outkey;
  /* int shmid_raw,shmid_out; */
  uint32_w uni;
  char tb[100];
  uint8_w *ptr,*ptw,*ptr_lim,*ptr_save,tm[6],tms[6],*ptw1;
  int i,j,k,ik,tow,rest;
  int eobsize_in, eobsize_in_count;
  WIN_sr  sr;
  WIN_bs  size;
  size_t  size_shm;
  WIN_ch ch1;
  WIN_sr  sr1;
  unsigned long c_save;  /* 64bit ok */
  WIN_ch ch;
  uint32_w gs,gs1;
  static int32_w buf1[MAX_SR],buf2[MAX_SR],buf3[MAX_SR],buf4;
  static double dbuf[3][MAX_SR],cyb[3][LIM],sbuf[3][MAX_SR];
  static double coef[MAX_NCOEF],dlp[3][MAX_SR],dlp0[3];
  double alp,alpp,alpm,hps,pga,pga1,ga,dd,ti,dt,sd,sds,grav;
  int  c,ss_mode=SSIZE5_MODE,ssf_flag=0,mpga,mshindo,wave,mupb;
  WIN_ch ch_base,ch_out;
  int chidx,wp,cc,ncoef,sec,isd,secc,mute,cmute,cnt[60][110],cnt60[110];

  if((progname=strrchr(argv[0],'/')) != NULL) progname++;
  else progname=argv[0];
  exit_status = EXIT_SUCCESS;
  mshindo=1; /* shindo output as default */
  mpga=wave=0;
  mute=1;  /* PERIOD sec mute as default */

  while ((c = getopt(argc, argv, "ahnop")) != -1) {
    switch (c) {
    case 'a':  /* SHINDO+PGA mode */
      mpga=mshindo=1;
      break;
    case 'h':  /* output HPF waveform data */
      wave=HPF;
      break;
    case 'n':  /* no mute */
      mute=0;
      break;
    case 'o':  /* output shindo FIR waveform data */
      wave=FIR;
      break;
    case 'p':  /* PGA mode */
      mpga=1;
      mshindo=0;
      break;
    default:
      usage();
    }
  }
  argc-=optind-1;
  argv+=optind-1;

  if(argc<6)
    usage();
  rawkey=atol(argv[1]);
  ch_base=(WIN_ch)strtol(argv[2],0,16);
  outkey=atol(argv[3]);
  size_shm=(size_t)atol(argv[4])*1000;
  ch_out=(WIN_ch)strtol(argv[5],0,16);
  chfile=NULL;
  rest=1;
  eobsize_in = 0;
  if(argc>6)
    {
    if(strcmp("-",argv[6])==0) chfile=NULL;
    else
      {
      chfile=argv[6];
      }
    }    
  if(argc>7) logfile=argv[7];
  else logfile=NULL;

  snprintf(tb,sizeof(tb),"shm_in=%d   ch_base=%04X",rawkey,ch_base);
  write_log(tb);
  snprintf(tb,sizeof(tb),"shm_out=%d   size=%d KB   ch=%04X",
    outkey,(int)size_shm/1000,ch_out);
  write_log(tb);
  snprintf(tb,sizeof(tb),"chfile=%s",chfile);
  write_log(tb);
  if(mpga==1 && mshindo==0) write_log("PGA mode");
  else if(mpga==0 && mshindo==1) write_log("SHINDO mode");
  else if(mpga==1 && mshindo==1) write_log("PGA+SHINDO mode");
  if(wave==FIR) write_log("output shindo FIR waveform data");
  else if(wave==HPF) write_log("output HPF waveform data");
  else write_log("no waveform data output");

  if(chfile!=NULL)
    {
    read_chfile(ch_base);
    snprintf(tb,sizeof(tb),"micro_gal_per_bit=(%.0f,%.0f,%.0f)",
      ugal_per_bit[0],ugal_per_bit[1],ugal_per_bit[2]);
    write_log(tb);
    }

  if(ugal_per_bit[0]==0.0)
    {
    mupb=1;
    write_log("ugal_per_bit will be estimated from gravity data.");
    }
  else mupb=0;

  /* shared memory */
  shr = Shm_read(rawkey, "in");
  shm = Shm_create(outkey, size_shm, "out");

  signal(SIGTERM,(void *)end_program);
  signal(SIGINT,(void *)end_program);

  alp=0.999;
  alpm=1-alp;
  alpp=1+alp;
  hps=alpp/(2.0*alp);
  dlp0[0]=dlp0[1]=dlp0[2]=INIT_VAL;
  wp=0;
  sec=0;
  ncoef=NCOEF;
  for(i=0;i<ncoef;i++) coef[i]=sd100[i];
  cmute=0;
  for(i=0;i<110;i++)
    {
    for(j=0;j<PERIOD;j++) cnt[j][i]=0;
    cnt60[i]=0;
    }

reset:
  /* initialize buffer */
  Shm_init(shm, size_shm);
  while(shr->r==(-1)) sleep(1);
  ptr=shr->d+shr->r;
  tow=(-1);
  for(i=0;i<3;i++) upb2[i]=ugal_per_bit[i]*ugal_per_bit[i];

  size=mkuint4(ptr);
  if(mkuint4(ptr+size-4)==size)
    eobsize_in=1;
  else
    eobsize_in=0;
  eobsize_in_count = eobsize_in;
  snprintf(tb,sizeof(tb),"eobsize_in=%d",eobsize_in);
  write_log(tb);

  for(;;)
    {
    size=mkuint4(ptr_save=ptr);
    if (mkuint4(ptr+size-4) == size) {
      if (++eobsize_in_count == 0)
	eobsize_in_count = 1;
    } else
      eobsize_in_count=0;
    if(eobsize_in && eobsize_in_count==0) goto reset;
    if(!eobsize_in && eobsize_in_count>3) goto reset;
    ptr_lim=ptr+size;
    if(eobsize_in)
      ptr_lim -= 4;

    c_save=shr->c;
    ptr+=4;
#if DEBUG
    for(i=0;i<6;i++) printf("%02X",ptr[i]);
    printf(" : %d R\n",size);
#endif
  /* make output data */
    ptw=shm->d+shm->p;
    ptw+=4;               /* size (4) */
    uni=(uint32_w)(time(NULL)-TIME_OFFSET);
    i=uni-mkuint4(ptr);
    if(i>=0 && i<1440)   /* with tow */
      {
      if(tow!=1)
        {
	snprintf(tb,sizeof(tb),"with TOW (diff=%ds)",i);
        write_log(tb);
        if(tow==0)
          {
          write_log("reset");
          goto reset;
          }
        tow=1;
        }
      ptr+=4;
      *ptw++=uni>>24;  /* tow (H) */
      *ptw++=uni>>16;
      *ptw++=uni>>8;
      *ptw++=uni;      /* tow (L) */
      }
    else if(tow!=0)
      {
      write_log("without TOW");
      if(tow==1)
        {
        write_log("reset");
        goto reset;
        }
      tow=0;
      }
    ptw1=tm;
    for(i=0;i<6;i++) *ptw1++=*ptw++=(*ptr++); /* YMDhms (6) */
    cc=0;
    do    /* loop for ch's */
      {
      gs = win_get_chhdr(ptr, &ch, &sr);
      if(sr>=SR_MIN && sr<=SR_MAX && ch>=ch_base && ch<ch_base+3)
        {
        chidx=ch-ch_base; /* 0,1,2 */
        cc|=(1<<chidx);
#if 0
        fprintf(stderr,"chidx=%d %u",chidx,gs);
#endif
        win2fix(ptr,buf1,&ch,&sr);
        for(i=0;i<sr;i++) dbuf[chidx][i]=(double)buf1[i];

        /* LPF */
        if(dlp0[chidx]==INIT_VAL) /* to converge fast */
          {
          dlp0[chidx]=dbuf[chidx][0];
          for(i=1;i<sr;i++) dlp0[chidx]+=dbuf[chidx][i];
          dlp0[chidx]/=(double)sr;
          }
        dlp[chidx][0]=alp*dlp0[chidx]+alpm*dbuf[chidx][0];
        for(i=1;i<sr;i++) dlp[chidx][i]=alp*dlp[chidx][i-1]+alpm*dbuf[chidx][i];
        dlp0[chidx]=dlp[chidx][sr-1];

	/* HPF */
	j=wp;
	for(i=0;i<sr;i++)
          {
          buf2[i]=(int)(cyb[chidx][j]=(dbuf[chidx][i]-dlp[chidx][i])*hps);
          j=(++j)&LMASK;
          }

	/* shindo FIR filter */
        j=wp;
        for(i=0;i<sr;i++)
          {
          dd=0.0;
          for(k=0;k<ncoef;k++)
            {
            ik=(j-k)&LMASK;
            dd+=cyb[chidx][ik]*coef[k];
            }
          buf3[i]=(int)(sbuf[chidx][i]=dd);
	  j=(++j)&LMASK;
          }
        /* output wave data */
	if(wave==FIR) /* output FIR data */
          ptw+=(gs1=mk_windata(buf3,ptw,sr,ch,ss_mode,ssf_flag));
        else if(wave==HPF) /* output HPF data */
          ptw+=(gs1=mk_windata(buf2,ptw,sr,ch,ss_mode,ssf_flag));
#if 0
        fprintf(stderr,"->%u (dlp0=%d) <wp=%d> ",gs1,(int)dlp0[chidx],wp);
#endif
        }
      ptr+=gs;
      } while(ptr<ptr_lim);
    /* end of one sec */
    dt=1.0/(double)sr;
    if(cc==0x7) /* all three chs filled */
      {
      if(mupb==1) /* calculate G from dlp */
	{
        grav=sqrt(dlp0[0]*dlp0[0]+dlp0[1]*dlp0[1]+dlp0[2]*dlp0[2]);
        ugal_per_bit[0]=ugal_per_bit[1]=ugal_per_bit[2]=980.0e+6/grav;
	upb2[0]=upb2[1]=upb2[2]=ugal_per_bit[0]*ugal_per_bit[0];
#if DEBUG
        fprintf(stderr,"upb_mode=%d grav=%.0f micro_gal_per_bit=%.0f\n",
          mupb,grav,ugal_per_bit[0]);
#endif
	}
	
      /* calculate PGA */
      pga=0.0;
      j=wp;
      for(i=0;i<sr;i++)
        {
	pga1=cyb[0][j]*cyb[0][j]*upb2[0]+cyb[1][j]*cyb[1][j]*upb2[1]+
	  cyb[2][j]*cyb[2][j]*upb2[2];
	if(pga1>pga) pga=pga1;
	j=(++j)&LMASK;
	}
      pga=sqrt(pga)*0.001; /* in mgal */

      /* calculate shindo */
      for(isd=0;isd<110;isd++)
	{
	cnt60[isd]-=cnt[sec][isd]; /* delete old data */
	cnt[sec][isd]=0;
	}
      for(i=0;i<sr;i++)
	{
	ga=sbuf[0][i]*sbuf[0][i]*upb2[0]+sbuf[1][i]*sbuf[1][i]*upb2[1]+
          sbuf[2][i]*sbuf[2][i]*upb2[2];
        ga=sqrt(ga)*0.000001; /* in gal */
        sd=2.0*log10(ga)+0.94;	
	isd=(int)((sd+3.0)*10+0.5); /* shindo -3.0->7.9 for isd 0->109 */ 
	if(isd<0) isd=0;
	else if(isd>109) isd=109;
	cnt[sec][isd]++;
        }
      for(isd=0;isd<110;isd++) cnt60[isd]+=cnt[sec][isd]; /* add new data */
      ti=0.0;
      for(isd=109;isd>=0;isd--)
	{
	if((ti+=dt*(double)cnt60[isd])>=0.3) break;
	}
      sd=-3.0+(double)isd*0.1; 
      if(sd!=sds)
	{
        for(k=0;k<6;k++) tms[k]=tm[k];
	sds=sd;
	secc=0;
	}
      else secc++;
/*      if(mute==1 && sd<=0.0) mute=0;*/
#if DEBUG
      fprintf(stderr,
   "snd-mode=%d snd=%.1f (%d) for %d s, pga-mode=%d pga=%.0f mgal, mute=%d\n",
        mshindo,sd,isd,secc,mpga,pga,mute);
#endif
      /* WIN format output */
      if(mute==0)
	{
        if(mshindo==1 && mpga==0)
          { /* isd(=shindo index:0-119) in 4 byte integer */
          buf4=isd;
          ptw+=(gs1=mk_windata(&buf4,ptw,1,ch_out,ss_mode,ssf_flag));
          }
        else if(mshindo==0 && mpga==1)
          { /* PGA (mgal) in 4 byte integer */
          buf4=(int)pga;
          ptw+=(gs1=mk_windata(&buf4,ptw,1,ch_out,ss_mode,ssf_flag));
          }
        else if(mshindo==1 && mpga==1)
          { /* isd in lowest 1 byte and PGA in upper 3 bytes */
          buf4=(((int)pga)<<8)+isd;
          ptw+=(gs1=mk_windata(&buf4,ptw,1,ch_out,ss_mode,ssf_flag));
          }
	}
      }
    if(++sec==PERIOD) sec=0;
    if(mute==1 && ++cmute==MUTE_SEC) mute=0;
    wp=j;

    if(tow) i=14;
    else i=10;
    if((uni=(uint32_w)(ptw-(shm->d+shm->p)))>i)
      {
      /* uni=ptw-(shm->d+shm->p); */ /* verbose */
      shm->d[shm->p  ]=uni>>24; /* size (H) */
      shm->d[shm->p+1]=uni>>16;
      shm->d[shm->p+2]=uni>>8;
      shm->d[shm->p+3]=uni;     /* size (L) */

#if DEBUG
      if(tow) for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+8+i]);
      else for(i=0;i<6;i++) printf("%02X",shm->d[shm->p+4+i]);
      printf(" : %d M\n",uni);
#endif

      shm->r=shm->p;
      if(ptw>shm->d+shm->pl) ptw=shm->d;
      shm->p=ptw-shm->d;
      shm->c++;
      }
    if((ptr=ptr_save+size)>shr->d+shr->pl) ptr=shr->d;
    while(ptr==shr->d+shr->p) usleep(100000);
    if(shr->c<c_save || mkuint4(ptr_save)!=size)
      {
      write_log("reset");
      goto reset;
      }
    }
  }
