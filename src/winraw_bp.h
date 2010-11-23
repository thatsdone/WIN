/* $Id: winraw_bp.h,v 1.1.4.1.6.1 2010/11/23 05:25:18 uehira Exp $ */

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

#define WRBP_VERSION  "1.0"   /* protocol version */

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
