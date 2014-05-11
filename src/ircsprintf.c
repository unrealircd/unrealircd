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

/*
 * sprintf_irc
 *
 * sprintf_irc is optimized for the formats: %c, %s, %lu, %d and %u.
 *
 * sprintf_irc is NOT optimized for any other format and resorts to using
 * the normal sprintf when it encounters a format it doesn't understand
 * (including padding, width, precision etc).
 *
 * It also treats %lu as %010lu (for simplicity and speed; it is designed
 * to print timestamps).
 *
 */

char *ircvsnprintf(char *str, size_t size, const char *format, va_list vl) {
	char* str_begin = str;
	char c;
        const char* end = str+size-1; //for comparison, not dereferencing.  It is the last position a null can go.

	if (!size) return str;

	while (str!=end && (c = *format++))
	{
		if (c == '%')
		{
			c = *format++;	/* May never be '\0' ! */
                        if (!c) break;  /* But just in case it is... these 2 instructions take care of it. */
			if (c == 'c') {
				*str++ = (char)va_arg(vl, int);
				continue;
			}
			if (c == 's') {
				const char *p1 = va_arg(vl, const char *);
				while (str!=end && *p1) *str++ = *p1++;
				continue;
			}
			if (c == 'l' && *format == 'u') { /* Prints time_t value in interval
							     [ 100000000 , 4294967295 ]
							     Actually prints like "%010lu" */
				int i;
				unsigned long int v;
				format++;
				if (str+10>end) break;
				v = va_arg(vl, unsigned long int);
				for (i = 9; i>=0; i--) {
					str[i] = (v%10)+'0';
					v /= 10;
				}
				str+=10;
				continue;
			}
			if (c == 'd' || c == 'i') {
				char scratch_buffer[16], *t;
				int v = va_arg(vl, int);
				int i = 0;
				size_t len;
				if (v<0) {
					v*=-1;
					*str++ = '-';
					if (str==end) break;
				}
				if (v==0) {
					*str++ = '0';
					continue;
				}

				t = scratch_buffer + sizeof(scratch_buffer);
				while (v) {
					*--t = (v%10) + '0';
					v/=10;
				}

				len = sizeof(scratch_buffer)-(t-scratch_buffer);
				if ((str+len)>end) break;
				for (i = 0; i < len; i++)
					*str++=t[i];
				continue;
			}
			if (c == 'u') {
				char scratch_buffer[16], *t;
				unsigned int v = va_arg(vl, unsigned int);
				int i = 0;
				size_t len;
				if (v==0) {
					*str++ = '0';
					continue;
				}

				t = scratch_buffer + sizeof(scratch_buffer);
				while (v) {
					*--t = (v%10) + '0';
					v/=10;
				}

				len = sizeof(scratch_buffer)-(t-scratch_buffer);
				if ((str+len)>end) break;
				for (i = 0; i < len; i++)
					*str++=t[i];
				continue;
			}
			if (c == '%') {
				*str++ = '%';
				continue;
			}
			
			//The default case: stop what we are doing and pass control to the real vsnprintf()
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
