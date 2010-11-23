/* $Id: ls7000.h,v 1.1.2.2 2010/11/23 03:50:20 uehira Exp $ */

/*
 * Copyright (c) 2009
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University
 */

/*
 * Datamark LS-7000 (not LS-7000XT) utility header file
 */

#ifndef _LS7000_H_
#define _LS7000_H_

#define LS7_PHDER_LEN  5     /* LS7000 packet header length */
#define LS7_PID        4     /* position of packet ID (A1, A8 or A9) */

#define LS7_A8_DLEN   77     /* A8 packet: LS7000 setting data length */
#define LS7_A9_DLEN   67     /* A9 packet: LS7000 status data length */

#define LS7_A89_TIME    0    /* A8, A9: time */
#define LS7_A89_ADDR    6    /* A8, A9: logger address. i.e. 1st channel ID */

#define LS7_A8_CNT_FLAG 0    /* A8 Control module flag */
#define LS7_A8_STL_FLAG 1    /* A8 Short module flag */

#define LS7_A8_CNT_SIZ  18   /* A8 Control module size */
#define LS7_A8_STL_SIZ  22   /* A8 Short module size */

#define LS7_A9_CNT_FLAG 0    /* A9 Control module flag */
#define LS7_A9_STL_FLAG 1    /* A9 Short module flag */

#define LS7_A9_CNT_SIZ  50   /* A9 Control module size */
#define LS7_A9_STL_SIZ  1    /* A9 Short module size */


#define LS7_A8_DIR       "A8"
#define LS7_A9_DIR       "A9"

static int LS7_A8_speed[] = {1200, 2400, 4800, 9600, 19200, 38400};
static int LS7_A8_sampling[] = {200, 100, -1, 20, -1, -1, -1, -1, -1, 80, 10, 2};
static uint8_w LS7_A8_gain_mask = 0xF0;
static uint8_w LS7_A8_bit_mask = 0x0F;

#endif  /*_LS7000_H_ */
