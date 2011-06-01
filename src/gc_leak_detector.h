/* $Id: gc_leak_detector.h,v 1.2 2011/06/01 11:09:21 uehira Exp $ */

/* Using the Boehm Garbage Collector as Leak Detector */

#ifndef _GC_LEAK_DETECTOR_H_
#define _GC_LEAK_DETECTOR_H_

#ifdef GC_MEMORY_LEAK_TEST
#define GC_DEBUG
#include "gc.h"
#define malloc(n) GC_MALLOC(n)
#define calloc(m,n) GC_MALLOC((m)*(n))
#define free(p) GC_FREE(p)
#define realloc(p,n) GC_REALLOC((p),(n))
#define CHECK_LEAKS() GC_gcollect()
#endif /* !GC_MEMORY_LEAK_TEST */

#endif  /* !_GC_LEAK_DETECTOR_H_ */
