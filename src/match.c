/*
 *   Unreal Internet Relay Chat Daemon, src/match.c
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "struct.h"
#include "common.h"
#include "sys.h"

ID_Copyright("(C) 1990 Jarkko Oikarinen");

/*
 *  Compare if a given string (name) matches the given
 *  mask (which can contain wild cards: '*' - match any
 *  number of chars, '?' - match any single character.
 *
 *	return	0, if match
 *		1, if no match
 */

#ifndef USE_LOCALE
u_char touppertab[], tolowertab[];
#define tolowertab2 tolowertab
#endif
/*
 * match()
 *  written by binary
 */
static inline match2(char *mask, char *name)
{
	u_char *m;		/* why didn't the old one use registers */
	u_char *n;		/* because registers suck -- codemastr */
	u_char cm;
	u_char *wsn;
	u_char *wsm;

	m = (u_char *)mask;

	cm = *m;

#ifndef USE_LOCALE
#define lc(x) tolowertab2[x]	/* use mylowertab, because registers are FASTER */
#else				/* maybe in the old 4mb hard drive days but not anymore -- codemastr */
#define lc(x) tolower(x)
#endif

	n = (u_char *)name;
	if (cm == '*')
	{
		if (m[1] == '\0')	/* mask is just "*", so true */
			return 0;
	}
	else if (cm != '?' && lc(cm) != lc(*n))
		return 1;	/* most likely first chars won't match */
	else
	{
		m++;
		n++;
	}
	cm = lc(*m);
	wsm = (char *)NULL;
	while (1)
	{
		if (cm == '*')	/* found the * wildcard */
		{
			m++;	/* go to next char of mask */
			if (!*m)	/* if at end of mask, */
				return 0;	/* function becomes true. */
			while (*m == '*')	/* while the char at m is "*" */
			{
				m++;	/* go to next char of mask */
				if (!*m)	/* if at end of mask, */
					return 0;	/* function becomes true. */
			}
			cm = *m;
			if (cm == '\\')	/* don't do ? checking if a \ */
			{
				cm = *(++m);	/* just skip this char, no ? checking */
			}
			else if (cm == '?')	/* if it's a ? */
			{
				do
				{
					m++;	/* go to the next char of both */
					n++;
					if (!*n)	/* if end of test string... */
						return (!*m ? 0 : 1);	/* true if end of mask str, else false */
				}
				while (*m == '?');	/* while we have ?'s */
				cm = *m;
				if (!cm)	/* last char of mask is ?, so it's true */
					return 0;
			}
			cm = lc(cm);
			while (lc(*n) != cm)
			{	/* compare */
				n++;	/* go to next char of n */
				if (!*n)	/* if at end of n string */
					return 1;	/* function becomes false. */
			}
			wsm = m;	/* mark after where wildcard found */
			cm = lc(*(++m));	/* go to next mask char */
			wsn = n;	/* mark spot first char was found */
			n++;	/* go to next char of n */
			continue;
		}
		if (cm == '?')	/* found ? wildcard */
		{
			cm = lc(*(++m));	/* just skip and go to next */
			n++;
			if (!*n)	/* return true if end of both, */
				return (cm ? 1 : 0);	/* false if end of test str only */
			continue;
		}
		if (cm == '\\')	/* next char will not be a wildcard. */
		{		/* skip wild checking, don't continue */
			cm = lc(*(++m));
			n++;
		}
		/* Complicated to read, but to save CPU time.  Every ounce counts. */
		if (lc(*n) != cm)	/* if the current chars don't equal, */
		{
			if (!wsm)	/* if there was no * wildcard, */
				return 1;	/* function becomes false. */
			n = wsn + 1;	/* start on char after the one we found last */
			m = wsm;	/* set m to the spot after the "*" */
			cm = lc(*m);
			while (cm != lc(*n))
			{	/* compare them */
				n++;	/* go to next char of n */
				if (!*n)	/* if we reached end of n string, */
					return 1;	/* function becomes false. */
			}
			wsn = n;	/* mark spot first char was found */
		}
		if (!cm)	/* cm == cn, so if !cm, then we've */
			return 0;	/* reached end of BOTH, so it matches */
		m++;		/* go to next mask char */
		n++;		/* go to next testing char */
		cm = lc(*m);	/* pointers are slower */
	}
}

/*
 * collapse a pattern string into minimal components.
 * This particular version is "in place", so that it changes the pattern
 * which is to be reduced to a "minimal" size.
 */
char *collapse(char *pattern)
{
	char *s;
	char *s1;
	char *t;

	s = pattern;

	if (BadPtr(pattern))
		return pattern;
	/*
	 * Collapse all \** into \*, \*[?]+\** into \*[?]+
	 */
	for (; *s; s++)
		if (*s == '\\')
		{
			if (!*(s + 1))
				break;
			else
				s++;
		}
		else if (*s == '*')
		{
			if (*(t = s1 = s + 1) == '*')
				while (*t == '*')
					t++;
			else if (*t == '?')
				for (t++, s1++; *t == '*' || *t == '?'; t++)
					if (*t == '?')
						*s1++ = *t;
			while ((*s1++ = *t++))
				;
		}
	return pattern;
}


/*
 *  Case insensitive comparison of two NULL terminated strings.
 *
 *	returns	 0, if s1 equal to s2
 *		<0, if s1 lexicographically less than s2
 *		>0, if s1 lexicographically greater than s2
 */
int  smycmp(char *s1, char *s2)
{
	u_char *str1;
	u_char *str2;
	int  res;

	str1 = (u_char *)s1;
	str2 = (u_char *)s2;

	while ((res = toupper(*str1) - toupper(*str2)) == 0)
	{
		if (*str1 == '\0')
			return 0;
		str1++;
		str2++;
	}
	return (res);
}


int  myncmp(char *str1, char *str2, int n)
{
	u_char *s1;
	u_char *s2;
	int  res;

	s1 = (u_char *)str1;
	s2 = (u_char *)str2;

	while ((res = toupper(*s1) - toupper(*s2)) == 0)
	{
		s1++;
		s2++;
		n--;
		if (n == 0 || (*s1 == '\0' && *s2 == '\0'))
			return 0;
	}
	return (res);
}

#ifndef USE_LOCALE
u_char tolowertab[] = {
	0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
	0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f,
	' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
	'*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	':', ';', '<', '=', '>', '?',
	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '[', '\\', ']', '^',
	'_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
	0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
	0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
	0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
	0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

u_char touppertab[] = {
	0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
	0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f,
	' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
	'*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
	'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
	0x5f,
	'`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
	'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '{', '|', '}', '~',
	0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
	0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
	0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
	0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#endif
u_char char_atribs[] = {
/* 0-7 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 8-12 */ CNTRL, CNTRL | SPACE, CNTRL | SPACE, CNTRL | SPACE,
	CNTRL | SPACE,
/* 13-15 */ CNTRL | SPACE, CNTRL, CNTRL,
/* 16-23 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 24-31 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* space */ PRINT | SPACE,
/* !"#$%&'( */ PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
/* )*+,-./ */ PRINT, PRINT, PRINT, PRINT, PRINT | ALLOW, PRINT | ALLOW,
	PRINT,
/* 012 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 345 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 678 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 9:; */ PRINT | DIGIT | ALLOW, PRINT, PRINT,
/* <=>? */ PRINT, PRINT, PRINT, PRINT,
/* @ */ PRINT,
/* ABC */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* DEF */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* GHI */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* JKL */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* MNO */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* PQR */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* STU */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* VWX */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* YZ[ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT | ALPHA,
/* \]^ */ PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
/* _`  */ PRINT | ALLOW, PRINT,
/* abc */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* def */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* ghi */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* jkl */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* mno */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* pqr */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* stu */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* vwx */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* yz{ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT | ALPHA,
/* |}~ */ PRINT | ALPHA, PRINT | ALPHA, PRINT | ALPHA,
/* del */ 0,
/* 80-8f */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90-9f */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0-af */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0-bf */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0-cf */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0-df */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0-ef */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0-ff */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Old match() */
int _match(char *mask, char *name) {
	return match2(mask,name);
}


/* Old match() plus some optimizations from bahamut */
int match(char *mask, char *name) {
	if (mask[0] == '*' && mask[1] == '!') {
		mask += 2;
		while (*name != '!' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
		
	if (mask[0] == '*' && mask[1] == '@') {
		mask += 2;
		while (*name != '@' && *name)
			name++;
		if (!*name)
			return 1;
		name++;
	}
	return match2(mask,name);
}
