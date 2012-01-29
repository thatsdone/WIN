/* $Id: timsac.h,v 1.1.2.2 2012/01/29 03:33:06 uehira Exp $ */

#ifndef _TIMSAC_H_
#define _TIMSAC_H_

/*  C version of TIMSAC, by Akaike & Nakagawa (1972) */

/***************************************************************************/
/*									   */
/*  �ΰʲ���ʸ�Ϻ�����ʤ��Ǥ���������                                     */
/*  �ץ����κ����ˤ����äƤϡ�                                         */
/*  ���ӡ�����֥����ʥߥå������ƥ������Ū���Ϥ�����ץ������󥹼�(1972) */
/*  �򻲹ͤˤ��ޤ�����                                                     */
/*									   */
/***************************************************************************/
/*									   */
/*  This software is based on the book;                                    */
/*  Nakagawa and Akaike, Statistical Analysis and Control of Dynamic       */
/*  Systems, Saiensu-sha (1972).                                           */
/*  (Don't delete the above notice.)                                       */
/*									   */
/***************************************************************************/

void autcor(double *, int, int, double *, double *);
void smeadl(double *,int, double *);
int fpeaut(int, int, int, double *, double *, double *, int *, double *);

#endif /* !_TIMSAC_H_*/
