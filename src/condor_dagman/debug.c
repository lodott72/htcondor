#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

// Declare DC_Exit so we don't have to include
// "../condor_daemon_core.V6/condor_daemon_core.h"
// and open a huge can of C++ worms.
extern void DC_Exit( int status );	

debug_level_t debug_level    = DEBUG_NORMAL;
char *        debug_progname = NULL;

/*--------------------------------------------------------------------------*/
int debug_printf (debug_level_t level, const char *fmt, ...) {
  int ret = 0;
  if (DEBUG_LEVEL(level)) {
    va_list args;
    va_start (args, fmt);
    ret = vprintf (fmt, args);
    va_end (args);
    if (DEBUG_LEVEL(DEBUG_DEBUG_1)) debug_fflush();
  }
  return ret;
}

/*--------------------------------------------------------------------------*/
void debug_println (debug_level_t level, const char *fmt, ...) {
  if (DEBUG_LEVEL(level)) {
    printf ("%s: ", debug_progname);
    {
      va_list args;
      va_start (args, fmt);
      vprintf (fmt, args);
      va_end (args);
    }
    printf ("\n");
    if (DEBUG_LEVEL(DEBUG_DEBUG_1)) debug_fflush();
  }
}

/*--------------------------------------------------------------------------*/
void debug_error (int error, debug_level_t level, const char *fmt, ...) {
  if (DEBUG_LEVEL(level)) {
    printf ("%s: ", debug_progname);
    {
      va_list args;
      va_start (args, fmt);
      vprintf (fmt, args);
      va_end (args);
    }
    printf ("\n");
    if (DEBUG_LEVEL(DEBUG_DEBUG_1)) debug_fflush();
  }
  DC_Exit(error);
  /* exit(error); */
}

/*--------------------------------------------------------------------------*/
void debug_perror (int error, debug_level_t level, const char *s) {
  if (DEBUG_LEVEL(level)) {
    perror (s);
    DC_Exit(error);
    /* exit(error); */
  }
}
