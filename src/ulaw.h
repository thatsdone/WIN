/* $Id: ulaw.h,v 1.1.2.2 2010/11/23 03:50:20 uehira Exp $ */

#ifndef _ULAW_H_
#define _ULAW_H_

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

/* ulaw.c */
unsigned int ulaw(int);
/* unsigned int cvt(int); */

#endif /* !_ULAW_H_*/
