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
#include "h.h"

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

#ifndef USE_LOCALE
#define lc(x) tolowertab2[x]	/* use mylowertab, because registers are FASTER */
#else				/* maybe in the old 4mb hard drive days but not anymore -- codemastr */
#define lc(x) tolower(x)
#endif


/* Match routine for special cases where escaping is needed in a normal fashion.
 * Checks a string ('name') against a globbing(+more) pattern ('mask').
 * Original by Douglas A Lewis (dalewis@acsu.buffalo.edu).
 * Code based on hybrid7's version (match_esc()).
 * Various modifications by Bram Matthys (Syzop).
 * Instead of our previous code, this one is less optimized but actually  _readable_ ;).
 * Modifications I (Syzop) had to do vs the hybrid7 code:
 * - Got rid of (u_char *) casts, since we already compile with
 *   chars defaulting to unsigned [or else major things break] ;).
 * - Use 0 for match and non-zero for no match (a la strcmp), not the reverse.
 * - Support for '_'.
 * - Rip out support for '#'.
 */
int match_esc(const char *mask, const char *name)
{
const u_char *m = mask;
const u_char *n = name;
const u_char *ma = NULL;
const u_char *na = name;

	while(1)
	{
		if (*m == '*')
		{
			while (*m == '*') /* collapse.. */
				m++;
			ma = m; 
			na = n;
		}
		
		if (!*m)
		{
			if (!*n)
				return 0;
			if (!ma)
				return 1;
			for (m--; (m > (const u_char *)mask) && (*m == '?'); m--);
			if (*m == '*')
				return 0;
			m = ma;
			n = ++na;
		} else
		if (!*n)
		{
			while (*m == '*') /* collapse.. */
				m++;
			return (*m != 0);
		}
		
		if (*m != '?')
		{
			if (*m == '\\')
				if (!*++m)
					return 1; /* unfinished escape sequence */
			if ((lc(*m) != lc(*n)) && !((*m == '_') && (*n == ' ')))
			{
				if (!ma)
					return 1;
				m = ma;
				n = ++na;
			} else
			{
				m++;
				n++;
			}
		} else
		{
			m++;
			n++;
		}
	}
	return 1;
}

/** Same credit/copyright as match_esc() applies, except escaping removed.. ;p */
static inline int match2(const char *mask, const char *name)
{
const u_char *m = mask;
const u_char *n = name;
const u_char *ma = NULL;
const u_char *na = name;

	while(1)
	{
		if (*m == '*')
		{
			while (*m == '*') /* collapse.. */
				m++;
			ma = m; 
			na = n;
		}
		
		if (!*m)
		{
			if (!*n)
				return 0;
			if (!ma)
				return 1;
			for (m--; (m > (const u_char *)mask) && (*m == '?'); m--);
			if (*m == '*')
				return 0;
			m = ma;
			n = ++na;
		} else
		if (!*n)
		{
			while (*m == '*') /* collapse.. */
				m++;
			return (*m != 0);
		}
		
		if ((lc(*m) != lc(*n)) && !((*m == '_') && (*n == ' ')) && (*m != '?'))
		{
			if (!ma)
				return 1;
			m = ma;
			n = ++na;
		} else
		{
			m++;
			n++;
		}
	}
	return 1;
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
int  smycmp(const char *s1, const char *s2)
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


int  myncmp(const char *str1, const char *str2, int n)
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
/* YZ[ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT,
/* \]^ */ PRINT, PRINT, PRINT,
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
/* yz{ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT,
/* |}~ */ PRINT, PRINT, PRINT,
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
int _match(const char *mask, const char *name) {
	return match2(mask,name);
}


/* Old match() plus some optimizations from bahamut */
int match(const char *mask, const char *name) {
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

/** Free up all resources of an aMatch entry (including the struct itself).
 * NOTE: this function may (also) be called for aMatch structs that have only been
 *       setup half-way, so use special care when accessing members (NULL checks!)
 */
void unreal_delete_match(aMatch *m)
{
	safefree(m->str);
#ifdef USE_TRE
	if (m->type == MATCH_TRE_REGEX)
	{
		if (m->ext.tre_expr)
		{
			regfree(m->ext.tre_expr);
			MyFree(m->ext.tre_expr);
		}
	}
#endif
	if (m->type == MATCH_PCRE_REGEX)
	{
		if (m->ext.pcre2_expr)
			pcre2_code_free(m->ext.pcre2_expr);
	}
	MyFree(m);
}

aMatch *unreal_create_match(MatchType type, char *str, char **error)
{
	aMatch *m = MyMallocEx(sizeof(aMatch));
	static char errorbuf[512];

	*errorbuf = '\0';

	m->str = strdup(str);
	m->type = type;
	
	if (m->type == MATCH_SIMPLE)
	{
		/* Nothing to do */
	}
	else if (m->type == MATCH_PCRE_REGEX)
	{
		int errorcode = 0;
		PCRE2_SIZE erroroffset = 0;
		int options = 0;
		char buf2[512];
		
		options = PCRE2_CASELESS|PCRE2_NEVER_UTF|PCRE2_NEVER_UCP;
		
		m->ext.pcre2_expr = pcre2_compile(str, PCRE2_ZERO_TERMINATED, options, &errorcode, &erroroffset, NULL);
		if (m->ext.pcre2_expr == NULL)
		{
			pcre2_get_error_message(errorcode, buf2, sizeof(buf2));
			if (error)
			{
				if (erroroffset > 0)
					snprintf(errorbuf, sizeof(errorbuf), "%s (at character #%d)", buf2, (int)erroroffset);
				else
					strlcpy(errorbuf, buf2, sizeof(errorbuf));
				*error = errorbuf;
			}
			unreal_delete_match(m);
			return NULL;
		}
		pcre2_jit_compile(m->ext.pcre2_expr, PCRE2_JIT_COMPLETE);
		return m;
	}
#ifdef USE_TRE
	else if (m->type == MATCH_TRE_REGEX)
	{
		int errorcode;
		
		m->ext.tre_expr = MyMallocEx(sizeof(regex_t));
		errorcode = regcomp(m->ext.tre_expr, str, REG_ICASE|REG_EXTENDED|REG_NOSUB);
		if (errorcode > 0)
		{
			int errorbufsize = 512;
			char *errtmp = MyMallocEx(errorbufsize);
			regerror(errorcode, m->ext.tre_expr, errtmp, errorbufsize);
			strlcpy(errorbuf, errtmp, sizeof(errorbuf));
			MyFree(errtmp);
			if (error)
				*error = errorbuf;
			unreal_delete_match(m);
			return NULL;
		}
	}
#endif
	else {
		abort(); /* unknown type, how did that happen ? */
	}
	return m;
}

/** Try to match an aMatch entry ('m') against a string ('str').
 * @returns 1 if matched, 0 if not.
 * @notes These (more logical) return values are opposite to the match() function.
 */
int unreal_match(aMatch *m, char *str)
{
	if (m->type == MATCH_SIMPLE)
	{
		if (_match(m->str, str) == 0)
			return 1;
		return 0;
	}
	
	if (m->type == MATCH_PCRE_REGEX)
	{
		pcre2_match_data *md = pcre2_match_data_create(9, NULL);
		int ret;
		
		ret = pcre2_match(m->ext.pcre2_expr, str, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL); /* run the regex */
		pcre2_match_data_free(md); /* yeah, we never use it. unfortunately argument must be non-NULL for pcre2_match() */
		
		if (ret > 0)
			return 1; /* MATCH */		
		return 0; /* NO MATCH */
	}

#ifdef USE_TRE
	if (m->type == MATCH_TRE_REGEX)
	{
		if (regexec(m->ext.tre_expr, str, 0, NULL, 0) == 0)
			return 1;
		return 0;
	}
#endif

	return 0;
}

int unreal_match_method_strtoval(char *str)
{
	if (!strcmp(str, "regex") || !strcmp(str, "pcre"))
		return MATCH_PCRE_REGEX;
	if (!strcmp(str, "posix") || !strcmp(str, "tre"))
		return MATCH_TRE_REGEX;
	if (!strcmp(str, "simple") || !strcmp(str, "glob"))
		return MATCH_SIMPLE;
	return 0;
}

char *unreal_match_method_valtostr(int val)
{
	if (val == MATCH_PCRE_REGEX)
		return "regex";
	if (val == MATCH_TRE_REGEX)
		return "posix";
	if (val == MATCH_SIMPLE)
		return "simple";
	
	return "unknown";
}

/* compares the first 'mask' bits. returns 1 if equal, 0 if not.
 * taken from atheme
 */
static int comp_with_mask(void *addr, void *dest, u_int mask)
{
	if (memcmp(addr, dest, mask / 8) == 0)
	{
		int n = mask / 8;
		int m = ((-1) << (8 - (mask % 8)));
		if (mask % 8 == 0 || (((u_char *) addr)[n] & m) == (((u_char *) dest)[n] & m))
		{
			return (1);
		}
	}
	return (0);
}

#define IPSZ 16

/** Match a user against a mask.
 * This will deal with 'nick!user@host', 'user@host' and just 'host'.
 * We try to match the 'host' portion against the client IP, real host, etc...
 * CIDR support is available so 'host' may be like '1.2.0.0/16'.
 * @returns 1 on match, 0 on no match.
 */
int match_user(char *rmask, aClient *acptr, int options)
{
	char mask[NICKLEN+USERLEN+HOSTLEN+8];
	char clientip[IPSZ], maskip[IPSZ];
	char *p = NULL;
	char *nmask = NULL, *umask = NULL, *hmask = NULL;
	int cidr = -1; /* CIDR length, -1 for no CIDR */

	strlcpy(mask, rmask, sizeof(mask));
	
	if (!(options & MATCH_MASK_IS_UHOST))
	{
		p = strchr(mask, '!');
		if (p)
		{
			*p++ = '\0';
			if (!*mask)
				return 0; /* NOMATCH: '!...' */
			nmask = mask;
			umask = p;
			
			/* Could just as well check nick right now */
			if (match(nmask, acptr->name) != 0)
				return 0; /* NOMATCH: nick mask did not match */
		}
	}

	if (!(options & (MATCH_MASK_IS_HOST)))
	{
		p = strchr(p ? p : mask, '@');
		if (p)
		{
			char *client_username;
			
			if (options & MATCH_USE_IDENT)
				client_username = acptr->username;
			else
				client_username = (acptr->user && *acptr->user->username) ? acptr->user->username : acptr->username;

			*p++ = '\0';
			if (!*p || !*mask)
				return 0; /* NOMATCH: '...@' or '@...' */
			hmask = p;
			if (!umask)
				umask = mask;
			
			/* Check user portion right away */
			if (match(umask, client_username) != 0)
				return 0; /* NOMATCH: user mask did not match */
		} else {
			if (nmask)
				return 0; /* NOMATCH: 'abc!def' (or even just 'abc!') */
			hmask = mask;
		}
	} else {
		hmask = mask;
	}
	
	/* If we get here then we have done checking nick / ident (if it was needed)
	 * and now need to match the 'host' portion.
	 */
	
	/**** check on IP ****/
	if (options & MATCH_CHECK_IP)
	{
		p = strchr(hmask, '/');
		if (p)
		{
			*p++ = '\0';
			cidr = atoi(p);
			if (cidr <= 0)
				return 0; /* NOMATCH: invalid CIDR */
		}

		if (strchr(hmask, '?') || strchr(hmask, '*'))
		{
			/* Wildcards */
			if (acptr->ip && (match(hmask, acptr->ip) == 0))
				return 1; /* MATCH (IP with wildcards) */
		} else 
		if (strchr(hmask, ':'))
		{
			/* IPv6 hostmask */

			/* We can actually return here on match/nomatch as we don't need to check the
			 * virtual host and things like that since ':' can never be in a hostname.
			 */
			if (!acptr->ip || !strchr(acptr->ip, ':'))
				return 0; /* NOMATCH: hmask is IPv6 address and client is not IPv6 */
			if (!inet_pton6(acptr->ip, clientip))
				return 0; /* NOMATCH: unusual failure */
			if (!inet_pton6(hmask, maskip))
				return 0; /* NOMATCH: invalid IPv6 IP in hostmask */
			
			if (cidr < 0)
				return comp_with_mask(clientip, maskip, 128); /* MATCH/NOMATCH by exact IP */

			if (cidr > 128)
				return 0; /* NOMATCH: invalid CIDR */

			return comp_with_mask(clientip, maskip, cidr);
		} else
		{
			/* Host is not IPv6 and does not contain wildcards.
			 * So could be a literal IPv4 address or IPv4 CIDR.
			 * NOTE: could also be neither (like a real hostname), so don't return 0 on nomatch,
			 * in that case we should just continue...
			 * The exception is CIDR. If we have CIDR mask then don't bother checking for
			 * virtual hosts and things like that since '/' can never be in a hostname.
			 */
			if (acptr->ip && inet_pton4(acptr->ip, clientip) && inet_pton4(hmask, maskip))
			{
				if (cidr < 0)
				{
					if (comp_with_mask(clientip, maskip, 32))
						return 1; /* MATCH: exact IP */
				}
				else if (cidr > 32)
					return 0; /* NOMATCH: invalid CIDR */
				else 
					return comp_with_mask(clientip, maskip, cidr); /* MATCH/NOMATCH by CIDR */
			}
		}
	}

	/**** Check real host ****/
	if (options & MATCH_CHECK_REAL_HOST)
	{
		char *hostname = acptr->user ? acptr->user->realhost : (MyClient(acptr) ? acptr->local->sockhost : NULL);
		if (hostname && (match(hmask, hostname) == 0))
			return 1; /* MATCH: hostname match */
	}
	
	/**** Check cloaked host ****/
	if (options & MATCH_CHECK_CLOAKED_HOST)
	{
		if (acptr->user && (match(hmask, acptr->user->cloakedhost) == 0))
			return 1; /* MATCH: cloaked host */
	}

	/**** Check visible host ****/
	// TODO: optimize if real host was checked already and visible host == real host
	if (options & MATCH_CHECK_VISIBLE_HOST)
	{
		char *hostname = acptr->user ? GetHost(acptr) : (MyClient(acptr) ? acptr->local->sockhost : NULL);
		if (hostname && (match(hmask, hostname) == 0))
			return 1; /* MATCH: cloaked host */
	}
	
	return 0; /* NOMATCH: nothing of the above matched */
}
