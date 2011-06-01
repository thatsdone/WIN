/* $Id: ls8tel.c,v 1.3 2011/06/01 11:09:21 uehira Exp $ */

/*
 * Copyright (c) 2005
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 *
 *   2005-06-09   Initial version.
 */

/* Datamark LS-8000SH of LS8TEL14/16 utility function */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "winlib.h"
#include "ls8tel.h"

/*
 * LS8TEL16 winformat to fix buffer
 *   returns group size in bytes 
 */
uint32_w
ls8tel16_fix(uint8_w *ptr, int32_w *abuf, WIN_ch *sys_ch, WIN_sr *sr)
/*       uint8_w  *ptr;     : input  */
/*       int32_w  *abuf;    : output */
/*       WIN_ch   *sys_ch;  : sys_ch */
/*       WIN_sr   *sr;      : sr     */
{
  uint32_w       b_size, g_size;
  uint32_w       i;
  uint32_w       s_rate;
  uint8_w        *dp;
  int16_w        shreg;
  int32_w        lonreg, atmp;
  int8_w         chreg;

  /* channel number */
  *sys_ch = (((WIN_ch)ptr[0]) << 8) + (WIN_ch)ptr[1];

  /* sampling rate */
  *sr = s_rate = (WIN_sr)ptr[3] + (((WIN_sr)(ptr[2] & 0x0f)) << 8);
  dp = ptr + 4;

  /* size */
  b_size = (ptr[2] >> 4) & 0x7;
  if (b_size)
    g_size = b_size * (s_rate - 1) + 8;
  else
    g_size = (s_rate >> 1) + 8;
  if (ptr[2] & 0x80)
    g_size++;

  /* read group */
  abuf[0] = ((dp[0] << 24) & 0xff000000) + ((dp[1] << 16) & 0xff0000)
    + ((dp[2] << 8) & 0xff00) + (dp[3] & 0xff);
  if (s_rate == 1)
    return (g_size);  /* normal return */

  dp += 4;
  switch (b_size) {
  case 0:   /* 0.5 byte */
    for (i = 1; i < s_rate; i += 2) {
      chreg = ((*(int8_w *)dp) >> 4);
      lonreg = (int32_w)chreg;
      atmp = abuf[i - 1] + lonreg;
      if (atmp < LS8_AMP_MIN || LS8_AMP_MAX < atmp) {
	if (lonreg <= 0)
	  lonreg &= 0x0000FFFF;
	else
	  lonreg |= 0xFFFF0000;
      }
      abuf[i] = abuf[i - 1] + lonreg;

      if (i == s_rate - 1)
	break;

      chreg = (((int8_w)(*(dp++) << 4)) >> 4);
      lonreg = (int32_w)chreg;
      atmp = abuf[i] + lonreg;
      if (atmp < LS8_AMP_MIN || LS8_AMP_MAX < atmp) {
	if (lonreg <= 0)
	  lonreg &= 0x0000FFFF;
	else
	  lonreg |= 0xFFFF0000;
      }
      abuf[i + 1] = abuf[i] + lonreg;
    }
    break;
  case 1:   /* 1 bye */
    for (i = 1; i < s_rate; i++) {
      chreg =  (*(int8_w *)(dp++));
      lonreg = (int32_w)chreg;
      atmp = abuf[i - 1] + lonreg;
      if (atmp < LS8_AMP_MIN || LS8_AMP_MAX < atmp) {
	if (lonreg <= 0)
	  lonreg &= 0x0000FFFF;
	else
	  lonreg |= 0xFFFF0000;
      }
      abuf[i] = abuf[i - 1] + lonreg;
    }
    break;
  case 2:   /* 2 byte */
    for (i = 1; i < s_rate; i++) {
      shreg = ((dp[0] << 8) & 0xff00) + (dp[1] & 0xff);
      dp += 2;
      lonreg = (int32_w)shreg;
      atmp = abuf[i - 1] + lonreg;
      if (atmp < LS8_AMP_MIN || LS8_AMP_MAX < atmp) {
	if (lonreg <= 0)
	  lonreg &= 0x0000FFFF;
	else
	  lonreg |= 0xFFFF0000;
      }
      abuf[i] = abuf[i - 1] + lonreg;
    }
    break;
  default:
    return (0); /* bad header */
  }

  return (g_size);  /* normal return */
}

