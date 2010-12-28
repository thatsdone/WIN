/* $Id: ulaw.c,v 1.1.4.1 2010/12/28 12:55:43 uehira Exp $ */

/************************************************************************/
/*      Copyright 1989 by Rich Gopstein and Harris Corporation          */
/*                                                                      */
/*      Permission to use, copy, modify, and distribute this software   */
/*      and its documentation for any purpose and without fee is        */
/*      hereby granted, provided that the above copyright notice        */
/*      appears in all copies and that both that copyright notice and   */
/*      this permission notice appear in supporting documentation, and  */
/*      that the name of Rich Gopstein and Harris Corporation not be    */
/*      used in advertising or publicity pertaining to distribution     */
/*      of the software without specific, written prior permission.     */
/*      Rich Gopstein and Harris Corporation make no representations    */
/*      about the suitability of this software for any purpose.  It     */
/*      provided "as is" without express or implied warranty.           */
/************************************************************************/

/************************************************************************/
/* sound2sun.c - Convert sampled audio files into uLAW format for the   */
/*               Sparcstation 1.                                        */
/*               Send comments to ..!rutgers!soleil!gopstein            */
/************************************************************************/
/*									*/
/*  Modified November 27, 1989 to convert to 8000 samples/sec           */
/*   (contrary to man page)                                             */
/*  Modified December 13, 1992 to write standard Sun .au header with	*/
/*   unspecified length.  Also made miscellaneous changes for 		*/
/*   VMS port.  (K. S. Kubo, ken@hmcvax.claremont.edu)			*/
/*  Fixed Bug with converting slow sample speeds			*/
/*									*/
/************************************************************************/

/* convert two's complement ch into uLAW format */

unsigned int
ulaw(int c)
{
  int mask;

  if (c < 0) {
    c = (-c);
    mask = 0x7f;
  } else
    mask = 0xff;

  if (c < 32)
    c = 0xF0 | (15 - (c >> 1));
  else if (c < 96)
    c = 0xE0 | (15 - ((c - 32) >> 2));
  else if (c < 224)
    c = 0xD0 | (15 - ((c - 96) >> 3));
  else if (c < 480)
    c = 0xC0 | (15 - ((c - 224) >> 4));
  else if (c < 992)
    c = 0xB0 | (15 - ((c - 480) >> 5));
  else if (c < 2016)
    c = 0xA0 | (15 - ((c - 992) >> 6));
  else if (c < 4064)
    c = 0x90 | (15 - ((c - 2016) >> 7));
  else if (c < 8160)
    c = 0x80 | (15 - ((c - 4064) >> 8));
  else
    c = 0x80;

  return (mask & c);
}

/* original ? */
/* unsigned int */
/* cvt(int ch) */
/* { */

/*   int mask; */

/*   if (ch < 0) { */
/*     ch = -ch; */
/*     mask = 0x7f; */
/*   } else { */
/*     mask = 0xff; */
/*   } */

/*   if (ch < 32) { */
/*     ch = 0xF0 | 15 - (ch / 2); */
/*   } else if (ch < 96) { */
/*     ch = 0xE0 | 15 - (ch - 32) / 4; */
/*   } else if (ch < 224) { */
/*     ch = 0xD0 | 15 - (ch - 96) / 8; */
/*   } else if (ch < 480) { */
/*     ch = 0xC0 | 15 - (ch - 224) / 16; */
/*   } else if (ch < 992) { */
/*     ch = 0xB0 | 15 - (ch - 480) / 32; */
/*   } else if (ch < 2016) { */
/*     ch = 0xA0 | 15 - (ch - 992) / 64; */
/*   } else if (ch < 4064) { */
/*     ch = 0x90 | 15 - (ch - 2016) / 128; */
/*   } else if (ch < 8160) { */
/*     ch = 0x80 | 15 - (ch - 4064) /  256; */
/*   } else { */
/*     ch = 0x80; */
/*   } */
/* return (mask & ch); */
/* } */

/*ulaw = cvt(chr * 16);*/
