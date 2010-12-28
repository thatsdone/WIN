/* $Id: win_xmalloc.c,v 1.1.6.1 2010/12/28 12:55:44 uehira Exp $ */

/*-
 *  memory allocation related functions.
 -*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>

#include "winlib.h"

#ifdef GC_MEMORY_LEAK_TEST
#include "gc_leak_detector.h"
#endif

/* replace malloc(3) */
void *
win_xmalloc(size_t size)
{

  if (size == 0) {
    /* (void)fputs("xmalloc : 0 size request.\n", stderr); */
    return (NULL);
  }

  return (malloc(size));
}

/* replace realloc(3) */
void *
win_xrealloc(void *ptr, size_t size)
{
  void *new;

  if (size == 0) {
    /* (void)fputs("xrealloc : 0 size request.\n", stderr); */
    return (NULL);
  }

  new = realloc(ptr, size);
  if (new == NULL) {
    win_xfree(ptr);
    return (NULL);
  }

  return (new);
}

/* replace calloc(3) */
void *
win_xcalloc(size_t number, size_t size)
{

  if (number == 0 || size == 0) {
    /* (void)fputs("xrcalloc : 0 size request.\n", stderr); */
    return (NULL);
  }

  return (calloc(number, size));
}

/* replace free(3) */
void
win_xfree(void *ptr)
{

  free(ptr);
  ptr = NULL;
}
