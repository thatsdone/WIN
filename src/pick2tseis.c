/* pick2tseis.c */
/* 98.02.20 tsuru */
/* input (stdin)   : a list of pick file names (ls -l) */
/* output (stdout) : first line of HYPOMH output + owner of pick file */
#include  <stdio.h>
main(argc, argv)
int argc;
char ** argv;
{
  FILE  *fp;
  int flag;
  char tbuf[256],fname[80],buf[256],owner[20],item[20][30],diag[20],wvid[20];
  int i,n;
  int yr, mo, dy, hr, mi, nstn;
  float sec, dt, lon, elon, lat, elat, dep, edep, mag;
  int fmt;
  sscanf(argv[1],"-A%d",&fmt);
  while (fgets(tbuf,255,stdin)) {
    i=sscanf(tbuf,"%s%s%s%s%s%s%s%s%s%s",item[0],item[1],item[2],item[3],
      item[4],item[5],item[6],item[7],item[8],item[9]);
/*    strcpy(owner,item[2]); */
    strcpy(fname,item[i - 1]);
    if((fp=fopen(fname,"r"))==NULL) continue;
    flag=1;
    while(fgets(buf,255,fp)) {
      if(flag&&strncmp(buf,"#p",2)==0) {
        *diag=0;
        *wvid=0;
        *owner=0;
        sscanf(buf+3,"%s%s%s",wvid,diag,owner);
        flag=0;
      }
      if(strncmp(buf,"#f",2)==0) {
        sscanf(buf+3,"%s%s%s%s%s%s%s%s%s%s",
          item[0],item[1],item[2],item[3],item[4],
          item[5],item[6],item[7],item[8],item[9]);
        sscanf(item[0],"%d",&yr); sscanf(item[1],"%d",&mo);
        sscanf(item[2],"%d",&dy); sscanf(item[3],"%d",&hr);
        sscanf(item[4],"%d",&mi); sscanf(item[5],"%f",&sec);
        sscanf(item[6],"%f",&lat);sscanf(item[7],"%f",&lon);
        sscanf(item[8],"%f",&dep);sscanf(item[9],"%f",&mag);
        fgets(buf,255,fp);
        sscanf(buf+3,"%s%s%s%s%s",item[0],item[1],item[2],item[3],item[4]);
        sscanf(item[2],"%f",&elat);
        sscanf(item[3],"%f",&elon);
        sscanf(item[4],"%f",&edep);
        
        fgets(buf,255,fp);
        fgets(buf,255,fp);
        fgets(buf,255,fp);
        sscanf(buf+ 3,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s)",
          item[0],item[1],item[2],item[3],item[4],item[5],item[6], 
          item[7],item[8],item[9],item[10],item[11],item[12],item[13]);
        sscanf(item[0],"%d",&n); nstn=n;
        for(i=0;i<n;i++) fgets(buf,255,fp);
        fgets(buf,255,fp);
        sscanf(buf+3,"%s%s",item[0],item[1]);
        sscanf(item[0],"%f",&dt);
        if(yr>80) yr=1900+yr;
        else yr=2000+yr;
        elon=elon/100.0; elat=elat/100.0;
        switch(fmt){ 
          case 0:
            fprintf(stdout, "%4d %2d %2d %2d %2d %7.3f %9.5f %8.5f %7.3f %4.1f\n", 
                yr, mo, dy, hr, mi, sec ,lon, lat, dep, mag);
            break;
          case 1:
            fprintf(stdout, "%4d %2d %2d %2d %2d %7.3f %7.3f %9.5f %7.5f %8.5f %7.5f %7.3f %7.3f %4.1f %5d\n", 
                yr, mo, dy, hr, mi, sec ,dt, lon, elon, lat, elat, dep, 
edep, mag, nstn);
            break;
          case 1000:
            fprintf(stdout, "%4d %2d %2d %2d %2d %7.3f %9.5f %8.5f %7.3f %4.1f%s %s\n", 
                yr, mo, dy, hr, mi, sec ,lon, lat, dep, mag, owner, diag);
            break;
          case 1001:
            fprintf(stdout, "%4d %2d %2d %2d %2d %7.3f %7.3f %9.5f %7.5f %8.5f %7.5f %7.3f %7.3f %4.1f %5d %s %s %s %s\n", 
                yr, mo, dy, hr, mi, sec ,dt, lon, elon, lat, elat, dep, 
edep, mag, nstn, fname, wvid, owner, diag);
            break;
        }
        break;
      }
    }
    fclose(fp);
  }
}
