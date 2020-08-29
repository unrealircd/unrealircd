/*
 * Unreal Internet Relay Chat Daemon, src/ircsprintf.c
 *
 * (C) Copyright 2013
 *
 * Author: Falcon Darkstar Momot based upon earlier code by Carlo Wood in 1997:
 * 1024/624ACAD5 1997/01/26 Carlo Wood, Run on IRC <carlo@runaway.xs4all.nl>
 * Key fingerprint = 32 EC A7 B6 AC DB 65 A6  F6 F6 55 DD 1C DC FF 61
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */
#include "ircsprintf.h"
#include <stdio.h>

/** Optimized version of vsnprintf() for us.
 * ircvsnprintf is optimized for the formats: %s, %c, %d, %i, %u, %lu and %lld.
 * If you only use these format string types then this function is significantly
 * faster than regular vsnprintf().
 * When it encounters any other format type or things like padding, precision,
 * etc. then it will resort to calling vsnprintf(), so no problem.
 */
char *ircvsnprintf(char *str, size_t size, const char *format, va_list vl)
{
	char *str_begin = str;
	char c;
	const char *end = str+size-1; /* for comparison, not dereferencing.  It is the last position a null can go. */
	char scratch_buffer[32]; /* large enough for 64 bit integer as a string */

	if (!size) return str;

	while (str!=end && (c = *format++))
	{
		if (c == '%')
		{
			c = *format++;
			if (c == 's')
			{
				/* %s - string */
				const char *p1 = va_arg(vl, const char *);
				while (str!=end && *p1) *str++ = *p1++;
				continue;
			}
			else if (c == 'c')
			{
				/* %c - single character */
				*str++ = (char)va_arg(vl, int);
				continue;
			}
			else if (c == 'd' || c == 'i')
			{
				/* %d and %i - integer */
				char *t;
				int v = va_arg(vl, int);
				int i = 0;
				size_t len;
				if (v==0)
				{
					*str++ = '0';
					continue;
				}
				t = scratch_buffer + sizeof(scratch_buffer);
				if (v<0)
				{
					*str++ = '-';
					if (str==end) break;
					while (v)
					{
						*--t = '0' - (v%10);
						v/=10;
					}
				} else {
					while (v)
					{
						*--t = (v%10) + '0';
						v/=10;
					}
				}

				len = sizeof(scratch_buffer)-(t-scratch_buffer);
				if ((str+len)>end) break;
				for (i = 0; i < len; i++)
					*str++=t[i];
				continue;
			}
			else if (c == 'l')
			{
				if (format[0] == 'l' && format[1] == 'd')
				{
					/* %lld - long long */
					char *t;
					long long v = va_arg(vl, long long);
					int i = 0;
					size_t len;

					format += 2;

					if (v==0)
					{
						*str++ = '0';
						continue;
					}
					t = scratch_buffer + sizeof(scratch_buffer);
					if (v<0)
					{
						*str++ = '-';
						if (str==end) break;
						while (v)
						{
							*--t = '0' - (v%10);
							v/=10;
						}
					} else {
						while (v)
						{
							*--t = (v%10) + '0';
							v/=10;
						}
					}

					len = sizeof(scratch_buffer)-(t-scratch_buffer);
					if ((str+len)>end) break;
					for (i = 0; i < len; i++)
						*str++=t[i];
					continue;
				}
				if (*format == 'u')
				{
					/* %lu - unsigned long */
					char *t;
					unsigned long v = va_arg(vl, unsigned long);
					int i = 0;
					size_t len;

					format++;
					if (v==0)
					{
						*str++ = '0';
						continue;
					}

					t = scratch_buffer + sizeof(scratch_buffer);
					while (v)
					{
						*--t = (v%10) + '0';
						v/=10;
					}

					len = sizeof(scratch_buffer)-(t-scratch_buffer);
					if ((str+len)>end) break;
					for (i = 0; i < len; i++)
						*str++=t[i];
					continue;
				}
			}
			else if (c == 'u')
			{
				/* %u - unsigned integer */
				char *t;
				unsigned int v = va_arg(vl, unsigned int);
				int i = 0;
				size_t len;
				if (v==0)
				{
					*str++ = '0';
					continue;
				}

				t = scratch_buffer + sizeof(scratch_buffer);
				while (v)
				{
					*--t = (v%10) + '0';
					v/=10;
				}

				len = sizeof(scratch_buffer)-(t-scratch_buffer);
				if ((str+len)>end) break;
				for (i = 0; i < len; i++)
					*str++=t[i];
				continue;
			}
			else if (c == '%')
			{
				/* %% - literal percent character */
				*str++ = '%';
				continue;
			}
			else if (!c)
				break; /* A % at the end of the format string (illegal, skipped) */
			
			/* The default case, when we cannot handle the % format:
			 * Stop what we are doing and pass control to the real vsnprintf()
			 */
			format -= 2;
			vsnprintf(str, (size_t)(end-str+1), format, vl);
			return str_begin;
		}
		*str++ = c;
	}
	*str = 0;
	return str_begin;
}

char *ircsnprintf(char *str, size_t size, const char *format, ...) {
	va_list vl;
	char *ret;
	va_start(vl, format);
	ret = ircvsnprintf(str, size, format, vl);
	va_end(vl);
	return ret;
}
