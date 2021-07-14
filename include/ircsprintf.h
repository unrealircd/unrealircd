/*
 * sprintf_irc.h
 *
 * $Id$
 */

#ifndef IRCSPRINTF_H
#define IRCSPRINTF_H
#include <stdarg.h>
#include <stdio.h>

/* ugly hack GRR, for both attribute and FORMAT_STRING */
#if !defined(__GNUC__) && !defined(__common_include__)
#define __attribute__(x) /* nothing */
#endif
#ifndef FORMAT_STRING
# define FORMAT_STRING(p) p
#endif

extern char *ircvsnprintf(char *str, size_t size, const char *format, va_list) __attribute__((format(printf,3,0)));
extern char *ircsnprintf(char *str, size_t size, FORMAT_STRING(const char *format), ...) __attribute__((format(printf,3,4)));

#endif
