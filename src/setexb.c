/* program "setexb.c"
	2/27/90, 3/8/93,1/17/94,5/27/94  urabe	*/

#include	<stdio.h>
#include	<string.h>
#include	<sys/types.h>
#define         N_EXABYTE	8

	int exb_status[N_EXABYTE],n_exb;
	char exb_name[N_EXABYTE][20],raw_dir[50],raw_dir1[50];

read_param(f_param,textbuf)
        FILE *f_param;
        unsigned char *textbuf;
        {
        do      {
                if(fgets(textbuf,200,f_param)==NULL) return 1;
                } while(*textbuf=='#');
        return 0;
        }

read_units(file)
        char *file;
        {
        FILE *fp;
        char tb[50],tb1[50];
        int i;
        for(i=0;i<n_exb;i++) exb_status[i]=0;
        sprintf(tb,"%s/%s",raw_dir1,file);
        if((fp=fopen(tb,"r"))==NULL)
                {
                sprintf(tb1,"wtape: %s",tb);
                perror(tb1);
                for(i=0;i<n_exb;i++) exb_status[i]=1;
                }
        else while(read_param(fp,tb)==0)
                {
                sscanf(tb,"%d",&i);
                if(i<n_exb && i>=0) exb_status[i]=1;
                }
        fclose(fp);
        }

write_units(file)
        char *file;
        {
        FILE *fp;
        char tb[50];
        int i;
        sprintf(tb,"%s/%s",raw_dir1,file);
        fp=fopen(tb,"w+");
        for(i=0;i<n_exb;i++) if(exb_status[i]) fprintf(fp,"%d\n",i);
        fclose(fp);
        }

init_param()
        {
        char tb[100],*ptr;
        FILE *fp;
        int i,j;

        if((fp=fopen("wtape.prm","r"))==NULL)
                {
                fprintf(stderr,"parameter file 'wtape.prm' not found\007\n");
                exit(1);
                }
        read_param(fp,tb);
		if((ptr=strchr(tb,':'))==0)
			{
			sscanf(tb,"%s",raw_dir);
			sscanf(tb,"%s",raw_dir1);
			}
		else
			{
			*ptr=0;
			sscanf(tb,"%s",raw_dir);
			sscanf(ptr+1,"%s",raw_dir1);
			}
        read_param(fp,tb);
        for(n_exb=0;n_exb<N_EXABYTE;n_exb++)
                {
                if(read_param(fp,tb)) break;
                sscanf(tb,"%s",exb_name[n_exb]);
                }

/* read exabyte mask file $raw_dir/UNITS */
        read_units("UNITS");
        write_units("_UNITS");
        }

main()
	{
	int i;
	init_param();
	printf("**** EXABYTES *****\n");
	printf("***  unit  use  ***\n");
	for(i=0;i<n_exb;i++)
		printf("       %d    %d\n",i,exb_status[i]);
	printf("*******************\n");
	}
