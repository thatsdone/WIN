/* 
   win text format to win format 
   2003.06.22- (C) Hiroshi TSURUOKA / All Rights Reserved.   
        06.30  bug fix & add fflush
   2004.10.10  writing to share memory option
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

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

#include "winlib.h"
#include "subst_func.h"

/* #define SR 4096 */
#define SR 200

int tokenize(char *command_string, char *tokenlist[], size_t maxtoken)
{
    static char tokensep[] = " \t";
    int tokencount;
    char *thistoken;
    if (command_string == NULL || !maxtoken)
	return 0;
    thistoken = strtok(command_string, tokensep);
    for (tokencount = 0; tokencount < maxtoken && thistoken != NULL;) {
	tokenlist[tokencount++] = thistoken;
	thistoken = strtok(NULL, tokensep);
    }
    tokenlist[tokencount] = NULL;
    return tokencount;
}

#define MAXTOKEN (SR + 2)
char *tokens[MAXTOKEN];

main(argc, argv)
int argc;
char *argv[];
{
#define MAXCH 1000
    static unsigned char outbuf[MAXCH][4 + 4 * SR], tt[6], cbuf;
    static long inbuf[SR];
    char buf[4096];
    int sr, ch, size, chsize[MAXCH], t[6], i, j, ntoken, nch;
    int c;

    key_t shmkey_out;
    struct Shm {
	unsigned long p;	/* write point */
	unsigned long pl;	/* write limit */
	unsigned long r;	/* Latest */
	unsigned long c;	/* counter */
	unsigned char d[1];	/* data buffer */
    } *shm_out;
    int shmid_out, shm_size = 0;
    unsigned char *ptw;
    unsigned long ltime;

    extern int optind;
    extern char *optarg;

    while ((c = getopt(argc, argv, "hk:s:")) != EOF) {
	switch (c) {
	case 'h':
	    fprintf(stderr, "usage: wintowin <[in_file] >[out_file]\n");
	    fprintf(stderr, "          or\n");
	    fprintf(stderr, "       wintowin [shm_key] [shm_size] \n");
	    exit(1);
	    break;
/*
	case 'k':
	    shmkey_out = atoi(optarg);
	    fprintf(stderr, "key=%d\n", shmkey_out);
	    break;
	case 's':
	    shm_size = atoi(optarg);
	    shm_size = shm_size * 1000;
	    fprintf(stderr, "size=%d\n", shm_size);
	    break;
 */
	default:
	    fprintf(stderr, " option -%c unknown\n", c);
	    exit(1);
	}
    }
    optind--;
    if ( argc > 1 ) {
      shmkey_out = atoi(argv[1+optind]);
      shm_size = atoi(argv[2+optind]);
      shm_size = shm_size * 1000;
    }

    if (shm_size > 0) {
	if ((shmid_out = shmget(shmkey_out, shm_size, IPC_CREAT | 0666)) < 0)
	    fprintf(stderr, "error shmget_out\n");
	if ((shm_out = (struct Shm *) shmat(shmid_out, (char *) 0, 0)) == (struct Shm *) -1)
	    fprintf(stderr, "error shmget_out\n");
	/* initialize output buffer */
	shm_out->p = shm_out->c = 0;
	shm_out->pl = (shm_size - sizeof(*shm_out)) / 10 * 9;
	shm_out->r = -1;
	ptw = shm_out->d;
    }

    while (fgets(buf, 2048, stdin) != NULL) {
	sscanf(buf, "%2x %2x %2x %2x %2x %2x %d", &t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &nch);
	for (i = 0; i < 6; i++)
	    tt[i] = t[i];
	size = 0;
	for (j = 0; j < nch; j++) {
	    fgets(buf, 2048, stdin);
	    ntoken = tokenize(buf, tokens, MAXTOKEN);
	    sscanf(tokens[0], "%x", &ch);
	    sr = atoi(tokens[1]);
	    for (i = 0; i < sr; i++) {
		inbuf[i] = atoi(tokens[i + 2]);
	    }
	    chsize[j] = winform(inbuf, outbuf[j], sr, ch);
	    size = size + chsize[j];
	}
	size = size + 10;
	if (shm_size == 0) {
	    cbuf = size >> 24;
	    fwrite(&cbuf, 1, 1, stdout);
	    cbuf = size >> 16;
	    fwrite(&cbuf, 1, 1, stdout);
	    cbuf = size >> 8;
	    fwrite(&cbuf, 1, 1, stdout);
	    cbuf = size;
	    fwrite(&cbuf, 1, 1, stdout);
	    fwrite(tt, 6, 1, stdout);
	    for (j = 0; j < nch; j++)
		fwrite(outbuf[j], chsize[j], 1, stdout);
	    fflush(stdout);
	} else {
/*
            size=size+4;
 */
	    cbuf = size >> 24; memcpy(ptw, &cbuf, 1); ptw++;
	    cbuf = size >> 16; memcpy(ptw, &cbuf, 1); ptw++;
	    cbuf = size >> 8; memcpy(ptw, &cbuf, 1); ptw++;
	    cbuf = size; memcpy(ptw, &cbuf, 1); ptw++;
/*
            time(&ltime);
            memcpy(ptw,&ltime, 4); ptw +=4;
 */
	    memcpy(ptw, tt, 6); ptw += 6;
	    for (j = 0; j < nch; j++) {
		memcpy(ptw, outbuf[j], chsize[j]);
		ptw += chsize[j];
	    }
	    if (ptw > (shm_out->d + shm_out->pl))
		ptw = shm_out->d;
	    shm_out->r = shm_out->p;
	    shm_out->p = ptw - shm_out->d;
	    shm_out->c++;
	}
    }				/* read data loop end */
}
