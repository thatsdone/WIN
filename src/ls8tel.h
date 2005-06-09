/* $Id: ls8tel.h,v 1.1 2005/06/09 16:50:38 uehira Exp $ */

/*
 * Copyright (c) 2005
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University
 */

/*
 * Datamark LS-8000SH of LS8TEL14/16 utility header file
 */

#ifndef _LS8TEL_H_
#define _LS8TEL_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "win_system.h"


#define LS8_PHDER_LEN  5     /* LS8000SH packet header length */
#define LS8_PID        4     /* position of packet ID (A1, A8 or A9) */

#define LS8_A8_DLEN   53     /* A8 packet: LS8000SH setting data length */
#define LS8_A9_DLEN   64     /* A9 packet: LS8000SH status data length */

#define LS8_A89_TIME    0    /* A8, A9: time */
#define LS8_A89_ADDR    6    /* A8, A9: logger address. i.e. 1st channel ID */

#define A8_DIR       "STS"
#define A9_DIR       "STM"

/* 16 bit dynamic range */
#define LS8_AMP_MAX  32767
#define LS8_AMP_MIN  -32768


WIN_blocksize ls8tel16_fix(unsigned char *, long *, long *, long *);

#endif  /*_LS8TEL_H_ */
