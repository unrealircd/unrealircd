/*
 * sprintf_irc.h
 *
 * $Id$
 */

#ifndef IRCSPRINTF_H
#define IRCSPRINTF_H
#include <stdarg.h>
#include <stdio.h>


/*
 * Proto types
 */

/* You do want it to work in debug mode yes ? --DrBin */

/* ugly hack GRR */
#if !defined(__GNUC__) && !defined(__common_include__)
#define __attribute__(x) /* nothing */
#endif

extern char *ircvsnprintf(char *str, size_t size, const char *format, va_list);
extern char *ircsnprintf(char *str, size_t size, const char *format, ...) __attribute__((format(printf,3,4)));

extern const char atoi_tab[4000];

#endif
