/* $Id: timsac.h,v 1.1.2.2 2012/01/29 03:33:06 uehira Exp $ */

#ifndef _TIMSAC_H_
#define _TIMSAC_H_

/*  C version of TIMSAC, by Akaike & Nakagawa (1972) */

/***************************************************************************/
/*									   */
/*  ［以下の文は削除しないでください］                                     */
/*  プログラムの作成にあたっては、                                         */
/*  赤池・中川「ダイナミックシステムの統計的解析と制御」サイエンス社(1972) */
/*  を参考にしました。                                                     */
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
