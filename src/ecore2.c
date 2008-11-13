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
/*                      2003/6/10  '#include <errno.h>' deleted by Urabe */
/*                      2003/7/2   introduced strerror() by Urabe/Nakagawa */

/*--- for config   2003/05/02  by N.Nakawaji ---*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

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

#include <sys/types.h>
#include <errno.h>

#include "winlib.h"

/*--------------------------------------------*/

#include <sys/file.h>
#include <memory.h>
#include <dirent.h>
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

char	Tbuf[512], *progname, *logfile;
int     syslog_mode = 0, exit_status;
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


char	file_list[NUMLEN], textbuf[NUMLEN];
void read_chtbl( dmy )
int dmy;
{
	FILE	*fp;
	int	i,j,nch;
	if ((fp = fopen(file_list, "r")) == NULL) {
		sprintf(Tbuf, "channel table file '%s' not open", file_list);
		write_log(Tbuf);
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
				write_log(Tbuf);
				break;
			}
		}
	}
	fclose(fp);
	sprintf(Tbuf,"N of channels = %d", nch);
	write_log(Tbuf);

	signal(SIGHUP,read_chtbl);
}

main(argc, argv)
	int		argc;
	char	**argv;
{
	double	dbuf[512], dt;
	int		i, j, sr, pos, shmid_in, shmid_out;
	long	dl;
	int32_w lbuf[512], work[512];
	unsigned long	shp_in, shp_in_save, ch_size, size_in,
		counter_save_in, shm_size, done_size, write_point;
	unsigned char	*ptr, *ptr_save, *ptw,
		out_data[SR * NCH], out_buf[100], cmp_tbl[20];
	key_t	shmkey_in, shmkey_out;
	struct Shm  *shm_in, *shm_out;
	struct Ch_header {
		unsigned short	ch_no;
		unsigned short	s_size_rate;
	} ch_header;
  
	if (progname = strrchr(argv[0], '/'))
		progname++;
	else
		progname = argv[0];

	exit_status = EXIT_SUCCESS;

	if (argc < 5) {
		fprintf(stderr,
			" usage : '%s [shm_key_in] [shm_key_out] [shm_size(KB)] [chlist] ([log file])'\n",
			progname);
		exit(1);
	}

	if (argc > 5)
		logfile = argv[5];
	else
		logfile = NULL;

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
	write_log(Tbuf);

	if ((shmid_out = shmget(shmkey_out, shm_size, IPC_CREAT|0666)) < 0)
		err_sys("shmget_out");
	if ((shm_out = (struct Shm *)shmat(shmid_out, (char *)0, 0)) ==
		(struct Shm *)-1)
		err_sys("shmat_out");

	sprintf(Tbuf, "out : shmkey_out=%d shmid_out=%d shm_size=%d",
		shmkey_out, shmid_out, shm_size);
	write_log(Tbuf);

	/* initialize output buffer */
	shm_out->p = shm_out->c = 0;
	shm_out->pl = (shm_size - sizeof(*shm_out)) / 10 * 9;
	shm_out->r = -1;

	ptw = shm_out->d;

	signal(SIGTERM,(void *)end_program);
	signal(SIGINT,(void *)end_program);

reset:
	while (shm_in->r == (-1))
		sleep(1);
	shp_in = shp_in_save = shm_in->r;	/* read position */

	while (1) {
		ptr = ptr_save = shm_in->d + shp_in;
		size_in = mkuint4(ptr_save);
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
					write_log("ERROR : ptr_save modified. reset");
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
			ch_header.ch_no = mkuint2(ptr);
			ch_header.s_size_rate = mkuint2(ptr+2);
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
				uint16_w sys_ch0; /*dummy*/
				uint32_w sr0; /*dummy*/
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
						write_log(Tbuf);
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
		write_log(Tbuf);
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
			write_log("ERROR : counter over. reset");
			goto reset;
		}
		if (mkuint4(ptr_save) != size_in) {
			write_log("ERROR : ptr_save or size_in modified. reset");
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
	gh = mkuint4(ddp);
	s_rate = gh & 0xfff;
	ddp += 4;		/* チャネルヘッダーシフト */
	b_size = (gh >> 12) & 0xf;	/* サンプルサイズの取得 */
	if (b_size)
		g_size = b_size * (s_rate - 1) + 4;
	else
		g_size = (s_rate >> 1) + 4;
	return 4 + g_size;
}
