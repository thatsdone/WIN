/* $Id: win2fix.c,v 1.1 2000/04/30 10:05:24 urabe Exp $ */

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
