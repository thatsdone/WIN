/* 
 ------------------------------------------------
   wintosac: win text format to sac/sac2000 format
   (C) TSURUOKA Hiroshi / All Rights Reserved.    
 ------------------------------------------------
 
   HISTORY
   2012/02/06 replace function julday
   2005/01/17 -d DIR option
              time stamp in SAC HEADER
              by NAKAGAWA Shigeki
 
 */
/* #define VERSION "1.0.1" */
#define YMD "2005.01.17"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "winlib.h"

/* prototypes */
static long julday(int, int, int);
static int tokenize(char *, char *[], size_t);
int main(int, char *[]);

static long
julday(int mon, int day, int year)
{
  int             y, m;
  int             a, b, c;
  double          d;
  if (mon >= 3) {
    y = year;
    m = mon;
  } else {
    y = year - 1;
    m = mon + 12;
  }
  if ((year > 1582) || (year == 1582 && mon > 10) || (year == 1582 && mon == 10 && day >= 15)) {
    a = (int) (y / 100);
    b = 2 - a + (int) (a / 4);
  }
  if ((year < 1582) || (year == 1582 && mon < 10) || (year == 1582 && mon == 10 && day <= 4)) {
    b = 0;
  }
  if (year > 0) {
    c = (int) (365.25 * y);
  }
  if (year <= 0) {
    c = (int) (365.25 * y - 0.75);
  }
  d = 1720994.5 + (int) (30.6001 * (m + 1)) + b + c + day + 0.5;
  return ((long)d);
}

static int
tokenize(char *command_string, char *tokenlist[], size_t maxtoken)
{
    static char tokensep[] = " \t";
    int tokencount;
    char *thistoken;

    if (command_string == NULL || !maxtoken)
      return (0);
    thistoken = strtok(command_string, tokensep);
    for (tokencount = 0; tokencount < maxtoken && thistoken != NULL;) {
	tokenlist[tokencount++] = thistoken;
	thistoken = strtok(NULL, tokensep);
    }
    tokenlist[tokencount] = NULL;
    return (tokencount);
}

#define MAXTOKEN 4096
char *tokens[MAXTOKEN];

typedef struct {
    char chid[5];
    char code[10];
    char comp[3];
    float stla, stlo, stel;
    double mul;
} c_info;

int
main(int argc, char *argv[])
{
/*     extern int optind; */
/*     extern char *optarg; */
    FILE *fp;
    int ntoken;
    int chindex[WIN_CHMAX], id[WIN_CHMAX];
    int c, chsel, check, swap, i, j, n, yr, mo, dy, hr, mi, sc, nch;
    int jul, jul0, idt, nsec, nsec0, ndata, nfreq;
    unsigned char chlist[WIN_CHMAX / 8];
    static unsigned int mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
#define setch(a) (chlist[a>>3]|=mask[a&0x07])
    int table = 0;
    char chtable[256];
    c_info ch_info[128];
    char sacfile[256];
    char outdir[768], outfile[1024];
    int idum[40], itmp;
    float fdum[70], ftmp;
    char cbuf[4096];
    char kstnm[8], kevnm[16], kdum[24][8];
    int buf[MAXTOKEN];
#define DATAMAX 1440000
    float data[DATAMAX];
    float begin = 0.0;
    int out = 0;
    int outd = 0;
    char stime[16];
    int little = 0;

    if (argc < 2) {
	fprintf(stderr, "usage: wintosac (-l) (-b[BEGIN]) (-t[TABLE]) (-o[OUTPUT]) (-d[OUTDIR]) CHID\n");
	exit(1);
    }

    while ((c = getopt(argc, argv, "lt:b:o:d:")) != -1) {
	switch (c) {
	case 'b':
	    begin = atof(optarg);
	    break;
	case 'o':
	    out = 1;
	    sprintf(sacfile, "%s", optarg);
	    break;
	case 'd':
	    outd = 1;
	    sprintf(outdir, "%s", optarg);
	    break;
	case 'l':
	    little = 1;
	    break;
	case 't':
	    table = 1;
	    sprintf(chtable, "%s", optarg);
	    break;
	default:
	    fprintf(stderr, " option -%c unknown\n", c);
	    exit(1);
	}
    }
    optind--;
    if (argc > 1 + optind) {
	nch = 0;
	for (i = 0; i < WIN_CHMAX / 8; i++)
	    chlist[i] = 0;
	for (i = 0; i < WIN_CHMAX; i++)
	    chindex[i] = -1;
	for (i = 1 + optind; i < argc; i++) {
	    chsel = strtol(argv[i], 0, 16);
	    setch(chsel);
	    chindex[chsel] = nch;
	    id[nch] = chsel;
	    nch++;
	}
    }

    if (nch < 1) {
	fprintf(stderr, "Please set channel ID\n");
	exit(1);
    }

    if (table) {
        if ((fp = fopen(chtable, "r")) == NULL) {
            fprintf(stderr, "file not open %s\n", chtable);
            exit(1);
        }
        while (fgets(cbuf, 200, fp) != NULL) {
            ntoken = tokenize(cbuf, tokens, MAXTOKEN);
            if (*cbuf == '#' || ntoken < 6)
                continue;
            chsel = strtol(tokens[0], 0, 16);
            setch(chsel);
/* 
            fprintf(stderr,"%s chindex %d\n",tokens[0],chindex[chsel]);
 */
    
            if (chindex[chsel] > -1) {
                sprintf(ch_info[chindex[chsel]].chid, "%s", tokens[0]);
                sprintf(ch_info[chindex[chsel]].code, "%s", tokens[3]);
                sprintf(ch_info[chindex[chsel]].comp, "%s", tokens[4]);
                ch_info[chindex[chsel]].mul = atof(tokens[12]) / (atof(tokens[7]) * pow(10.0, atoi(tokens[11]) / 20.0));
/*
                fprintf(stderr, "%04X %g\n", tokens[0], ch_info[chindex[chsel]].mul);
 */
    
            }
        }
        fclose(fp);
    }

    for (i = 0; i < 40; i++)
	idum[i] = -12345;
    for (i = 0; i < 70; i++)
	fdum[i] = -12345.0;

    n = 0;
    ndata = 0;
    while (fgets(cbuf, sizeof(cbuf), stdin) != NULL) {
	sscanf(cbuf, "%d %d %d %d %d %d %d", &yr, &mo, &dy, &hr, &mi, &sc, &nch);
	for (j = 0; j < nch; j++) {
	    fgets(cbuf, sizeof(cbuf), stdin);
	    ntoken = tokenize(cbuf, tokens, MAXTOKEN);
	    chsel = strtol(tokens[0], 0, 16);
	    setch(chsel);
	    if (chindex[chsel] > -1) {
		nfreq = atoi(tokens[1]);
		for (i = 0; i < nfreq; i++) {
		    buf[i] = atoi(tokens[i + 2]);
		}
	    }
	}
	if (yr < 100)
	    yr = 2000 + yr;
	jul = julday(mo, dy, yr) - julday(1, 1, yr) +1;
	nsec = 3600 * hr + 60 * mi + sc;
	if (n == 0) {
	    idt = 0;
	    sprintf(stime, "%02d/%02d/%02d %02d:%02d", yr % 100, mo, dy, hr, mi);
	    if (!out) {
		if (table) {
		    sprintf(sacfile, "%02d%02d%02d%02d%02d_%s_%s.s", yr % 100, mo, dy, hr, mi, ch_info[0].code,
			    ch_info[0].comp);
		} else {
		    sprintf(sacfile, "%02d%02d%02d%02d%02d_%04X.s", yr % 100, mo, dy, hr, mi, id[0]);
		}
	    }
	    idum[0]=yr;
	    idum[1]=jul;
	    idum[2]=hr;
	    idum[3]=mi;
	    idum[4]=sc;
	    idum[5]=0;
	} else {
	    idt = 86400 * (jul - jul0) + (nsec - nsec0);
	}
	if (idt > 1) {
	    for (j = 0; j < idt; j++) {
		for (i = 0; i < nfreq; i++)
		    data[n * nfreq + i] = 0.0;
		n = n + 1;
		ndata = ndata + nfreq;
	    }
	}
	for (i = 0; i < nfreq; i++)
	    data[n * nfreq + i] = (float) buf[i];
	n = n + 1;
	ndata = ndata + nfreq;
	jul0 = jul;
	nsec0 = nsec;
    }

    if (table) {
	for (i = 0; i < ndata; i++)
	    data[i] = ch_info[0].mul * data[i];
    }

/* write sac data */

    fdum[0] = 1.0 / (float) nfreq;	/* DELTA */
    fdum[5] = begin;		/* B */
    fdum[6] = fdum[5] + fdum[0] * (float) (ndata - 1);	/* E */
/*
    if (table) {
	fdum[31] = ch_info[0].stla;
	fdum[32] = ch_info[0].stlo;	
	fdum[33] = ch_info[0].stel;	
    }
 */

    idum[6] = 6;		/* NVHDR */

    idum[9] = ndata;		/* NPTS */
    idum[15] = 01;		/* IFTYPE */
    idum[35] = 1;		/* LEVEN */

    if (table) {
	sprintf(kevnm, "%s", stime);
	sprintf(kstnm, "%s%s", ch_info[0].code, ch_info[0].comp);
    } else {
	sprintf(kevnm, "%s", stime);
	sprintf(kstnm, "%04X", id[0]);
    }

    for (i = 0; i < 21; i++)
	strcpy(kdum[i], "-12345  ");

    if(outd){
        sprintf(outfile,"%s/%s",outdir,sacfile);
        if ((fp = fopen(outfile, "w")) == NULL) {
 	   fprintf(stdout, "Error in opening output file: %s\n", outfile);
           exit(1);
        }
    }else{
        if ((fp = fopen(sacfile, "w")) == NULL) {
    	   fprintf(stdout, "Error in opening output file: %s\n", sacfile);
           exit(1);
        }
    }

/* little/big endian */
    check = 1;
    swap = 0;
    if (*(char *) &check) {
	if (little == 0)
	    swap = 1;
    } else {
	if (little == 1)
	    swap = 1;
    }

    for (i = 0; i < 70; i++) {
	ftmp = fdum[i];
	if (swap)
	    SWAPF(ftmp);
	fwrite(&ftmp, sizeof(float), 1, fp);
    }
    for (i = 0; i < 40; i++) {
	itmp = idum[i];
	if (swap)
	    SWAP32(itmp);
	fwrite(&itmp, sizeof(float), 1, fp);
    }
    fwrite(kstnm, 1, 8, fp);
    fwrite(kevnm, 1, 16, fp);
    for (i = 0; i < 21; i++) {
	fwrite(kdum[i], 1, 8, fp);
    }
    for (i = 0; i < ndata; i++) {
	ftmp = data[i];
	if (swap)
	    SWAPF(ftmp);
	fwrite(&ftmp, sizeof(float), 1, fp);
    }
    fclose(fp);

    exit(0);
}
