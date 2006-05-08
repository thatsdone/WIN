/* $Id: winraw_bp.h,v 1.1 2006/05/08 04:02:30 uehira Exp $ */

/*-
 * Copyright (c) 2006-
 *   Uehira Kenji / All Rights Reserved.
 *    uehira@sevo.kyushu-u.ac.jp
 *    Institute of Seismology and Volcanology, Kyushu University.
 */

/*
 * winraw_bp.h
 *  win raw_data backup protocol
 *  server port number is 6542
 */

#ifndef _WINRAW_BP_H_
#define _WINRAW_BP_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define WRBP_VERSION  "0.1"   /* protocol version */

#define WRBP_CLEN      128    /* command length */
#define WRBP_MSG      1024    /*  length */

#define WRBP_SIZE     "SIZE"
#define WRBP_REQ      "REQ"
#define WRBP_STAT     "STAT"
#define WRBP_QUIT     "QUIT"

#define WRBP_OK       "OK"
#define WRBP_ERR      "ERR"


#define MIN_SEC       60


#endif  /* !_WINRAW_BP_H_ */
