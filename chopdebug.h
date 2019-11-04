#ifndef __CHOPDEBUG_H__
#define __CHOPDEBUG_H__

#include <unistd.h>

#define DEBUG 1 // TODO: remove this and place in makefile for compilation-time decision to debug

static const int debug_fd = STDERR_FILENO;
static const char debug_header[] = "[DEBUG]: ";
static const char debug_header_ext[] = "[DEBUG][%s]: ";
static const char debug_header_err[] = "[ERRNO][%d]: ";

/*
 * Prints requested format string into stderr, prefixing properly
 */
void _debug_print(const char *format, const char *function, ...);

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) _debug_print(__FUNCTION__, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...) {} /* Don't do anything in release builds */
#endif

#endif
