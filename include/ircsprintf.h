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

extern char *ircvsprintf(char *str, const char *format, va_list);
extern char *ircsprintf(char *str, const char *format, ...);

extern const char atoi_tab[4000];

#endif
