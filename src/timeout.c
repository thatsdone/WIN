/* $Id: timeout.c,v 1.4 2011/06/01 11:09:22 uehira Exp $ */
/* program timeout.c     11/21/90,8/27/91,11/18/92  urabe */
/* "timeout" executes a program with timeout */
/* usage : timeout [time(sec)] [command line] */

#ifdef HAVE_CONFIG_H
#include        "config.h"
#endif

#include        <sys/types.h>
#include        <sys/wait.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<signal.h>
#include	<unistd.h>

#include        "subst_func.h"

static const char rcsid[] =
  "$Id: timeout.c,v 1.4 2011/06/01 11:09:22 uehira Exp $";

/* prototypes */
int main(int, char *[]);

int
main(int argc, char *argv[])
	{
	pid_t pid,pid_com,pid_to;
	
	if((pid_com=fork())==0)		/* child - rcp */
		{
		argv[argc]=0;
		execvp(argv[2],&argv[2]);
		exit(0);
		}
	else if((pid_to=fork())==0)	/* child - timeout */
		{
		sleep(atoi(argv[1]));
		exit(0);
		}
	else pid=wait(0);			/* parent */
	if(pid==pid_com)
		{
		kill(pid_to,SIGKILL);
		wait(0);
		exit(0);
		}
	else if(pid==pid_to)
		{
		fprintf(stderr,"'%s' timed out after %d sec\007\007\n",argv[2],atoi(argv[1]));
		kill(pid_com,SIGKILL);
		wait(0);
		exit(1);
		}
	else if(pid==(-1))
		{
		exit(1);
		}
	exit(0);
	}
