/*
  program "ecore2.c"   4/16/93-5/13/93,7/2/93,7/5/94  urabe
                      1/6/95 bug in adj_time fixed (tm[0]--)
                      3/17/95 write_log()
                      7/23/01 4/12/02(Hinet) K.Takano
			Endian free
			NCH(Max output channel) 1500 -> 3000 -> 5000(Hinet)
			usleep(800000) -> sleep(1)
			SIGHUP -> read_chtbl()
			abnormal exit(0) -> exit(1)
			Bug: Ch file was used for Logfile if arg>5. fixed.
*/

/*                      2003/4/16  configure for any machine N.Nakawaji  */
/*                      2003/6/3   '#include <errno.h>' by Urabe */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>

#include "subst_func.h"

/*--- for config   2003/05/02  by N.Nakawaji ---*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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


/*--------------------------------------------*/

#include <memory.h>
#include <dirent.h>
#include <signal.h>
#include <ctype.h>

/* following parameters give M=3 (6-th order) */
#define	FP			6.0
#define	FS			9.0
#define	AP			0.5
#define	AS			5.0

#define	SR		20		/* sampling rate of output data */
#define	CHMASK		0xffff	/* max of name channels */
#define	NCH		5000	/* max of output channels */
#define	MAX_FILT	25		/* max of filter order */
#define	NUMLEN		256

#include <errno.h>

char	Tbuf[512], *Progname, Logfile[512];
short	Pos_table[CHMASK + 1];
struct {
	int		before_sr;
	int		sys_ch;
	int		flag_filt;
	int		m_filt;
	int		n_filt;
	double	gn_filt;
	double	coef[MAX_FILT*4];
	double	uv[MAX_FILT*4];
} Ch[NCH];


mklong(ptr)
  unsigned char *ptr;
  {
  unsigned long a;
  a=((ptr[0]<<24)&0xff000000)+((ptr[1]<<16)&0xff0000)+
    ((ptr[2]<<8)&0xff00)+(ptr[3]&0xff);
  return a;
  }

mkshort(ptr)
  unsigned char *ptr;
  {
  unsigned short a;
  a=((ptr[0]<<8)&0xff00)+((ptr[1])&0xff);
  return a;
  }


write_log(logfil, ptr)
	char	*logfil;
	char	*ptr;
{
	FILE	*fp;
	int		tm[6];

	if (*logfil)
		fp = fopen(logfil, "a");
	else
		fp = stdout;
	get_time(tm);
	fprintf(fp, "%02d%02d%02d.%02d%02d%02d %s %s\n",
		tm[0], tm[1], tm[2], tm[3], tm[4], tm[5], Progname, ptr);
	if (*logfil)
		fclose(fp);
}

char	file_list[NUMLEN], textbuf[NUMLEN];
void read_chtbl( dmy )
int dmy;
{
	FILE	*fp;
	int	i,j,nch;
	if ((fp = fopen(file_list, "r")) == NULL) {
		sprintf(Tbuf, "channel table file '%s' not open", file_list);
		write_log(Logfile, Tbuf);
		exit(1);
	}

	/* table initialize */
	for (i = 0; i < CHMASK + 1; i++)
		Pos_table[i] = -1;
	nch = 0;
	if (fp != NULL) {
		while (fgets(textbuf, NUMLEN, fp)) {
			if (*textbuf == '#')
				continue;
			sscanf(textbuf, "%x", &i);
			i &= CHMASK;
			Ch[nch].sys_ch = i;
			Ch[nch].flag_filt = 0;
			Pos_table[i] = nch;
			if (++nch == NCH) {
				sprintf(Tbuf,"WARNING : N of channels exceed table limit = %d at CH=%04X", NCH, i);
				write_log(Logfile, Tbuf);
				break;
			}
		}
	}
	fclose(fp);
	sprintf(Tbuf,"N of channels = %d", nch);
	write_log(Logfile, Tbuf);

	signal(SIGHUP,read_chtbl);
}

void ctrlc( sig )
int sig;
{
	write_log(Logfile, "end.");
	exit(0);
}


err_sys(ptr)
	char	*ptr;
{
	perror(ptr);
	write_log(Logfile, ptr);
	if (errno < sys_nerr)
		write_log(Logfile, sys_errlist[errno]);
	ctrlc(0);
}


get_time(rt)
	int		*rt;
{
	struct tm	*nt;
	long	ltime;

	time(&ltime);
	nt = localtime(&ltime);
	rt[0] = nt->tm_year % 100;
	rt[1] = nt->tm_mon+1;
	rt[2] = nt->tm_mday;
	rt[3] = nt->tm_hour;
	rt[4] = nt->tm_min;
	rt[5] = nt->tm_sec;
}



main(argc, argv)
	int		argc;
	char	**argv;
{
	double	dbuf[512], dt;
	int		i, j, sr, pos, shmid_in, shmid_out;
	long	dl, lbuf[512], work[512];
	unsigned long	shp_in, shp_in_save, ch_size, size_in,
		counter_save_in, shm_size, done_size, write_point;
	unsigned char	*ptr, *ptr_save, *ptw,
		out_data[SR * NCH], out_buf[100], cmp_tbl[20];
	key_t	shmkey_in, shmkey_out;
	struct Shm {
		unsigned long	p;		/* write point */
		unsigned long	pl;		/* write limit */
		unsigned long	r;		/* Latest */
		unsigned long	c;		/* counter */
		unsigned char	d[1];	/* data buffer */
	} *shm_in, *shm_out;
	struct Ch_header {
		unsigned short	ch_no;
		unsigned short	s_size_rate;
	} ch_header;
  
	if (Progname = strrchr(argv[0], '/'))
		Progname++;
	else
		Progname = argv[0];

	if (argc < 5) {
		fprintf(stderr,
			" usage : '%s [shm_key_in] [shm_key_out] [shm_size(KB)] [chlist] ([log file])'\n",
			Progname);
		exit(1);
	}

	if (argc > 5)
		strcpy(Logfile, argv[5]);
	else
		*Logfile = 0;

	shmkey_in = atoi(argv[1]);
	shmkey_out = atoi(argv[2]);
	shm_size = atoi(argv[3]) * 1000;

	/* read channel list */
	sscanf(argv[4], "%s", file_list);
	read_chtbl();

	/* shered memory */
	if ((shmid_in = shmget(shmkey_in, 0, 0)) < 0)
		err_sys("shmget_in");
	if ((shm_in = (struct Shm *)shmat(shmid_in, (char *)0, SHM_RDONLY)) ==
		(struct Shm *)-1)
		err_sys("shmat_in");

	sprintf(Tbuf, "in : shmkey_in=%d shmid_in=%d", shmkey_in, shmid_in);
	write_log(Logfile, Tbuf);

	if ((shmid_out = shmget(shmkey_out, shm_size, IPC_CREAT|0666)) < 0)
		err_sys("shmget_out");
	if ((shm_out = (struct Shm *)shmat(shmid_out, (char *)0, 0)) ==
		(struct Shm *)-1)
		err_sys("shmat_out");

	sprintf(Tbuf, "out : shmkey_out=%d shmid_out=%d shm_size=%d",
		shmkey_out, shmid_out, shm_size);
	write_log(Logfile, Tbuf);

	/* initialize output buffer */
	shm_out->p = shm_out->c = 0;
	shm_out->pl = (shm_size - sizeof(*shm_out)) / 10 * 9;
	shm_out->r = -1;

	ptw = shm_out->d;

	signal(SIGTERM,ctrlc);
	signal(SIGINT,ctrlc);

reset:
	while (shm_in->r == (-1))
		sleep(1);
	shp_in = shp_in_save = shm_in->r;	/* read position */

	while (1) {
		ptr = ptr_save = shm_in->d + shp_in;
		size_in = mklong(ptr_save);
		memcpy(cmp_tbl, ptr_save, 10);
		counter_save_in = shm_in->c;
		done_size = 0;
		/* フイルタリング手続き */
		ptr += (4 + 6);	/* ブロックサイズ+秒ブロックシフト */
		done_size = 4 + 6;	/* ブロックサイズ+秒ブロック */
		write_point = 0;

		for (;;) {		/* 1秒分のチャネル処理ループ */
			if (done_size >= size_in) {
				/* 秒ブロックの終了 shm_outへ書き出し */
				if (memcmp(cmp_tbl, ptr_save, 10)) {
					write_log(Logfile, "ERROR : ptr_save modified. reset");
					goto reset;
				}
				/* 先頭のブロックサイズと時刻ヘッダ
					時刻ヘッダはコピー */
				memcpy(ptw, ptr_save, 10);
				ptw[0]= ( (write_point +10) >>24 ) & 0xff;
				ptw[1]= ( (write_point +10) >>16 ) & 0xff;
				ptw[2]= ( (write_point +10) >> 8 ) & 0xff;
				ptw[3]= ( (write_point +10)      ) & 0xff;
				ptw += 10;
				/* フィルタリング済みデータを書き出し */
				memcpy(ptw, out_data, write_point);
				ptw += write_point;
				/* shm_outポインタの更新 */
				if (ptw > (shm_out->d + shm_out->pl))
					ptw = shm_out->d;
				shm_out->r = shm_out->p;	/* latest */
				shm_out->p = ptw - shm_out->d;
				shm_out->c++;
				break;
			}

			/* チャネルヘッダ:ch_no(2B)+sampleing(0.5B+1.5B) */
			ch_header.ch_no = mkshort(ptr);
			ch_header.s_size_rate = mkshort(ptr+2);
			pos = Pos_table[ch_header.ch_no];
			if (pos < 0) { /* チャネル表にないのでskip */
				ch_size = get_ch_size(&ptr);
				ptr += ch_size;
				done_size += ch_size;
				continue;
			}
			/* sampling rate(1.5B) */
			sr = ch_header.s_size_rate & 0x0fff;

			if (sr <= 20) {
				/* 20Hz以下は、そのままセット */
				ch_size = get_ch_size(&ptr);
				memcpy(&out_data[write_point], ptr, ch_size);
				write_point += ch_size;
				ptr += ch_size;
				done_size += ch_size;
			} else {
				/* winフォーマットをlbufに展開 */
				int sys_ch0, sr0; /*dummy*/
				ch_size = get_ch_size(&ptr);
				win2fix(ptr,lbuf,&sys_ch0,&sr0);
				ptr += ch_size;
				done_size += ch_size;

				/* lbuf -> (double)dbuf */
				for (j = 0; j < sr; j++)
					dbuf[j] = (double)lbuf[j];

				/* フイルタリング処理 */
				if (Ch[pos].flag_filt == 0) {
					dt = 1.0 / (double)sr;
					butlop(Ch[pos].coef, &Ch[pos].m_filt,
						&Ch[pos].gn_filt, &Ch[pos].n_filt,
						FP * dt, FS * dt, AP, AS);
					/* 保留!!!*/
					if (Ch[pos].m_filt > MAX_FILT) {
						sprintf(Tbuf,"WARNING : filter order exceeded limit");
						write_log(Logfile, Tbuf);
						/*end_process(1);*/
					}
					/**/
					Ch[pos].flag_filt = 1;
				}
				/**************************************
				tandem(dbuf, dbuf, sr, Ch[pos].coef,
					Ch[pos].m_filt, 1, Ch[pos].uv);
				****************************************/
{
	int t_i;
	if(sr<=0 || Ch[pos].m_filt<=0) {
		sprintf(Tbuf, "? (tandem) invalid input : sr=%d Ch[pos].m_filt=%d ?\n", sr,Ch[pos].m_filt);
		write_log(Logfile, Tbuf);
	} else {
		{
			int i,j,jd;
			double a,aa,b,bb,u1,u2,u3,v1,v2,v3;
			j=0; jd=1;
			a =Ch[pos].coef[0]; aa=Ch[pos].coef[1]; b =Ch[pos].coef[2]; bb=Ch[pos].coef[3];
			u1=Ch[pos].uv[0]; u2=Ch[pos].uv[1]; v1=Ch[pos].uv[2]; v2=Ch[pos].uv[3];
			/****  FILTERING */
			for(i=0;i<sr;i++) {
				u3=u2; u2=u1; u1=dbuf[j];
				v3=v2; v2=v1; v1=u1+a*u2+aa*u3-b*v2-bb*v3;
				dbuf[j]=v1;
				j+=jd;
			}
			Ch[pos].uv[0]=u1; Ch[pos].uv[1]=u2; Ch[pos].uv[2]=v1; Ch[pos].uv[3]=v2;
		}
		if(Ch[pos].m_filt>1)
			for(t_i=1;t_i<Ch[pos].m_filt;t_i++) {
				{
					int i,j,jd;
					double a,aa,b,bb,u1,u2,u3,v1,v2,v3;
					j=0; jd=1;
					a =Ch[pos].coef[t_i*4+0]; aa=Ch[pos].coef[t_i*4+1]; b =Ch[pos].coef[t_i*4+2]; bb=Ch[pos].coef[t_i*4+3];
					u1=Ch[pos].uv[t_i*4+0]; u2=Ch[pos].uv[t_i*4+1]; v1=Ch[pos].uv[t_i*4+2]; v2=Ch[pos].uv[t_i*4+3];
					/****  FILTERING */
					for(i=0;i<sr;i++) {
						u3=u2; u2=u1; u1=dbuf[j];
						v3=v2; v2=v1; v1=u1+a*u2+aa*u3-b*v2-bb*v3;
						dbuf[j]=v1;
						j+=jd;
					}
					Ch[pos].uv[t_i*4+0]=u1; Ch[pos].uv[t_i*4+1]=u2; Ch[pos].uv[t_i*4+2]=v1; Ch[pos].uv[t_i*4+3]=v2;
				}
			}
	}
}
				/* re-sample data by SR */
				for (j = 0; j < SR; j++) {
					dl = (long)(dbuf[(j * sr) / SR] *
						Ch[pos].gn_filt);
					work[j] = dl;
				}
				ch_size = winform(work, out_buf, SR, ch_header.ch_no);
				/* store data in Out_data buffer */
				memcpy(&out_data[write_point], out_buf, ch_size);
				write_point += ch_size;
			}
		}
		shp_in += size_in;
		if (shp_in > shm_in->pl)
			shp_in = shp_in_save = 0;
		while (shm_in->p == shp_in)
			sleep(1); /* usleep(800000) -> sleep(1) */
		if (shm_in->c < counter_save_in) {
			write_log(Logfile, "ERROR : counter over. reset");
			goto reset;
		}
		if (mklong(ptr_save) != size_in) {
			write_log(Logfile, "ERROR : ptr_save or size_in modified. reset");
			goto reset;
		}
	}
}


get_ch_size(dp)
	unsigned char	**dp;
{
	unsigned char	*ddp;
	int		gh, s_rate, g_size, b_size;

	ddp = (*dp);
	gh = mklong(ddp);
	s_rate = gh & 0xfff;
	ddp += 4;		/* チャネルヘッダーシフト */
	b_size = (gh >> 12) & 0xf;	/* サンプルサイズの取得 */
	if (b_size)
		g_size = b_size * (s_rate - 1) + 4;
	else
		g_size = (s_rate >> 1) + 4;
	return 4 + g_size;
}

/* $Id: ecore2.c,v 1.2 2003/06/03 00:19:46 urabe Exp $ */

win2fix(ptr,abuf,sys_ch,sr) /* returns group size in bytes */
  unsigned char *ptr; /* input */
  register long *abuf;/* output */
  long *sys_ch;       /* sys_ch */
  long *sr;           /* sr */
  {
  int b_size,g_size;
  register int i,s_rate;
  register unsigned char *dp;
  unsigned int gh;
  short shreg;
  int inreg;

  dp=ptr;
  gh=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  *sr=s_rate=gh&0xfff;
/*  if(s_rate>MAX_SR) return 0;*/
  if(b_size=(gh>>12)&0xf) g_size=b_size*(s_rate-1)+8;
  else g_size=(s_rate>>1)+8;
  *sys_ch=(gh>>16)&0xffff;

  /* read group */
  abuf[0]=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
    ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
  dp+=4;
  if(s_rate==1) return g_size;  /* normal return */
  switch(b_size)
    {
    case 0:
      for(i=1;i<s_rate;i+=2)
        {
        abuf[i]=abuf[i-1]+((*(char *)dp)>>4);
        abuf[i+1]=abuf[i]+(((char)(*(dp++)<<4))>>4);
        }
      break;
    case 1:
      for(i=1;i<s_rate;i++)
        abuf[i]=abuf[i-1]+(*(char *)(dp++));
      break;
    case 2:
      for(i=1;i<s_rate;i++)
        {
        shreg=((dp[0]<<8)&0xff00)+(dp[1]&0xff);
        dp+=2;
        abuf[i]=abuf[i-1]+shreg;
        }
      break;
    case 3:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00);
        dp+=3;
        abuf[i]=abuf[i-1]+(inreg>>8);
        }
      break;
    case 4:
      for(i=1;i<s_rate;i++)
        {
        inreg=((dp[0]<<24)&0xff000000)+((dp[1]<<16)&0xff0000)+
          ((dp[2]<<8)&0xff00)+(dp[3]&0xff);
        dp+=4;
        abuf[i]=abuf[i-1]+inreg;
        }
      break;
    default:
      return 0; /* bad header */
    }
  return g_size;  /* normal return */
  }

/* $Id: ecore2.c,v 1.2 2003/06/03 00:19:46 urabe Exp $ */
/* winform.c  4/30/91,99.4.19   urabe */
/* winform converts fixed-sample-size-data into win's format */
/* winform returns the length in bytes of output data */

winform(inbuf,outbuf,sr,sys_ch)
  long *inbuf;      /* input data array for one sec*/
  unsigned char *outbuf;  /* output data array for one sec */
  int sr;         /* n of data (i.e. sampling rate) */
  unsigned short sys_ch;  /* 16 bit long channel ID number */
  {
  int dmin,dmax,aa,bb,br,i,byte_leng;
  long *ptr;
  unsigned char *buf;

  /* differentiate and obtain min and max */
  ptr=inbuf;
  bb=(*ptr++);
  dmax=dmin=0;
  for(i=1;i<sr;i++)
    {
    aa=(*ptr);
    *ptr++=br=aa-bb;
    bb=aa;
    if(br>dmax) dmax=br;
    else if(br<dmin) dmin=br;
    }

  /* determine sample size */
  if(((dmin&0xfffffff8)==0xfffffff8 || (dmin&0xfffffff8)==0) &&
    ((dmax&0xfffffff8)==0xfffffff8 || (dmax&0xfffffff8)==0)) byte_leng=0;
  else if(((dmin&0xffffff80)==0xffffff80 || (dmin&0xffffff80)==0) &&
    ((dmax&0xffffff80)==0xffffff80 || (dmax&0xffffff80)==0)) byte_leng=1;
  else if(((dmin&0xffff8000)==0xffff8000 || (dmin&0xffff8000)==0) &&
    ((dmax&0xffff8000)==0xffff8000 || (dmax&0xffff8000)==0)) byte_leng=2;
  else if(((dmin&0xff800000)==0xff800000 || (dmin&0xff800000)==0) &&
    ((dmax&0xff800000)==0xff800000 || (dmax&0xff800000)==0)) byte_leng=3;
  else byte_leng=4;
  /* make a 4 byte long header */
  buf=outbuf;
  *buf++=(sys_ch>>8)&0xff;
  *buf++=sys_ch&0xff;
  *buf++=(byte_leng<<4)|(sr>>8);
  *buf++=sr&0xff;

  /* first sample is always 4 byte long */
  *buf++=inbuf[0]>>24;
  *buf++=inbuf[0]>>16;
  *buf++=inbuf[0]>>8;
  *buf++=inbuf[0];
  /* second and after */
  switch(byte_leng)
    {
    case 0:
      for(i=1;i<sr-1;i+=2)
        *buf++=(inbuf[i]<<4)|(inbuf[i+1]&0xf);
      if(i==sr-1) *buf++=(inbuf[i]<<4);
      break;
    case 1:
      for(i=1;i<sr;i++)
        *buf++=inbuf[i];
      break;
    case 2:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 3:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    case 4:
      for(i=1;i<sr;i++)
        {
        *buf++=inbuf[i]>>24;
        *buf++=inbuf[i]>>16;
        *buf++=inbuf[i]>>8;
        *buf++=inbuf[i];
        }
      break;
    }
  return (int)(buf-outbuf);
  }

#define	PI		3.141593
#define	HP		1.570796
#include <math.h>

/*
+      BUTTERWORTH LOW PASS FILTER COEFFICIENT
+
+      ARGUMENTS
+        H      : FILTER COEFFICIENTS
+        M      : ORDER OF FILTER  (M=(N+1)/2)
+        GN     : GAIN FACTOR
+        N      : ORDER OF BUTTERWORTH FUNCTION
+        FP     : PASS BAND FREQUENCY  (NON-DIMENSIONAL)
+        FS     : STOP BAND FREQUENCY
+        AP     : MAX. ATTENUATION IN PASS BAND
+        AS     : MIN. ATTENUATION IN STOP BAND
+
+      M. SAITO  (17/XII/75)
*/
butlop(h, m, gn, n, fp, fs, ap, as)
	double	*h, fp, fs, ap, as, *gn;
	int		*m, *n;
{
	double	wp, ws, tp, ts, pa, sa, cc, c, dp, g, fj, c2, sj, tj, a;
	int		k, j;

	if (fabs(fp) < fabs(fs))
		wp = fabs(fp) * PI;
	else
		wp = fabs(fs) * PI;
	if (fabs(fp) > fabs(fs))
		ws = fabs(fp) * PI;
	else
		ws = fabs(fs) * PI;
	if (wp == 0.0 || wp == ws || ws >= HP) {
		sprintf(Tbuf,"? (butlop) invalid input : fp=%14.6e fs=%14.6e ?\n", fp, fs);
		write_log(Logfile, Tbuf);
		return 1;
	}

/****  DETERMINE N & C */
	tp = tan(wp);
	ts = tan(ws);
	if (fabs(ap) < fabs(as))
		pa = fabs(ap);
	else
		pa = fabs(as);
	if (fabs(ap) > fabs(as))
		sa = fabs(ap);
	else
		sa = fabs(as);
	if (pa == 0.0)
		pa=0.5;
	if (sa == 0.0)
		sa=5.0;
	if ((*n = (int)fabs(log(pa / sa) / log(tp / ts)) + 0.5) < 2)
		*n = 2;
	cc = exp(log(pa * sa) / (double)(*n)) / (tp * ts);
	c = sqrt(cc);

	dp = HP / (double)(*n);
	*m = (*n) / 2;
	k = (*m) * 4;
	g = fj =1.0;
	c2 = 2.0 * (1.0 - c) * (1.0 + c);

	for (j = 0; j < k; j += 4) {
		sj = pow(cos(dp * fj), 2.0);
		tj = sin(dp * fj);
		fj = fj + 2.0;
		a = 1.0 / (pow(c + tj, 2.0) + sj);
		g = g * a;
		h[j] = 2.0;
		h[j + 1] = 1.0;
		h[j + 2] = c2 * a;
		h[j + 3] = (pow(c - tj, 2.0) + sj) * a;
	}
/****  EXIT */
	*gn = g;
	if (*n % 2 == 0)
		return 0;
/****  FOR ODD N */
	*m = (*m) + 1;
	*gn = g / (1.0 + c);
	h[k] = 1.0;
	h[k + 1] = 0.0;
	h[k + 2] = (1.0 - c) / (1.0 + c);
	h[k + 3] = 0.0;
	return 0;
}


/*
+      RECURSIVE FILTERING : F(Z) = (1+A*Z+AA*Z**2)/(1+B*Z+BB*Z**2)
+
+      ARGUMENTS
+        X      : INPUT TIME SERIES
+        Y      : OUTPUT TIME SERIES  (MAY BE EQUIVALENT TO X)
+        N      : LENGTH OF X & Y
+        H      : FILTER COEFFICIENTS ; H(1)=A, H(2)=AA, H(3)=B, H(4)=BB
+        NML    : >0 ; FOR NORMAL  DIRECTION FILTERING
+                 <0 ; FOR REVERSE DIRECTION FILTERING
+		 uv		: past data and results saved
+
+      M. SAITO  (6/XII/75)
*/
recfil(x, y, n, h, nml, uv)
	int		n,	nml;
	double	*x, *y, *h, *uv;
{
	int		i, j, jd;
	double	a, aa, b, bb, u1, u2, u3, v1, v2, v3;

	if (n <= 0) {
		sprintf(Tbuf,"? (recfil) invalid input : n=%d ?\n", n);
		write_log(Logfile, Tbuf);
		return 1;
	}
	if (nml >= 0) {
		j = 0;
		jd = 1;
	} else {
		j = n - 1;
		jd = -1;
	}
	a = h[0];
	aa = h[1];
	b = h[2];
	bb = h[3];
	u1 = uv[0];
	u2 = uv[1];
	v1 = uv[2];
	v2 = uv[3];
/****  FILTERING */
	for(i = 0; i < n; i++) {
		u3 = u2;
		u2 = u1;
		u1 = x[j];
		v3 = v2;
		v2 = v1;
		v1 = u1 + a * u2 + aa *u3 - b * v2 - bb * v3;
		y[j] = v1;
		j += jd;
	}
	uv[0] = u1;
	uv[1] = u2;
	uv[2] = v1;
	uv[3] = v2;

	return 0;
}


/*
+      RECURSIVE FILTERING IN SERIES
+
+      ARGUMENTS
+        X      : INPUT TIME SERIES
+        Y      : OUTPUT TIME SERIES  (MAY BE EQUIVALENT TO X)
+        N      : LENGTH OF X & Y
+        H      : COEFFICIENTS OF FILTER
+        M      : ORDER OF FILTER
+        NML    : >0 ; FOR NORMAL  DIRECTION FILTERING
+                 <0 ;     REVERSE DIRECTION FILTERING
+		 uv		: past data and results saved
+
+      SUBROUTINE REQUIRED : RECFIL
+
+      M. SAITO  (6/XII/75)
*/
tandem(x, y, n, h, m, nml, uv)
	double	*x, *y, *h, *uv;
	int		n, m, nml;
{
	int		i;

	if (n <= 0 || m <= 0) {
		sprintf(Tbuf,"? (tandem) invalid input : n=%d m=%d ?\n", n, m);
		write_log(Logfile, Tbuf);
		return 1;
	}
/****  1-ST CALL */
	recfil(x, y, n, h, nml, uv);
/****  2-ND AND AFTER */
	if (m > 1)
		for (i = 1; i < m; i++)
			recfil(y, y, n, &h[i * 4], nml, &uv[i * 4]);

	return 0;
}
/**/
