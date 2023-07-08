/*
 *   Unreal Internet Relay Chat Daemon, src/support.c
 *   Copyright (C) 1990, 1991 Armin Gruner
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

/** @file
 * @brief Functions that don't always exist on every OS are provided here.
 * That was the original idea, anyway. Right now it also contains some
 * functions that should probably be in src/misc.c instead.
 * In any case, most functions here don't have any special meaning
 * specific for IRC, they could just as well be used in non-IRC code.
 */

/* support.c 2.21 4/13/94 1990, 1991 Armin Gruner; 1992, 1993 Darren Reed */

#include "unrealircd.h"

extern void outofmemory();

#define is_enabled match

/** Convert integer to string */
const char *my_itoa(int i)
{
	static char buf[128];
	ircsnprintf(buf, sizeof(buf), "%d", i);
	return buf;
}

/** Walk through a string of tokens, using a set of separators.
 * @param save	Pointer used for saving between calls
 * @param str	String to parse (will be altered!)
 * @param fs	Separator character(s)
 * @returns substring (token)
 * @note This function works similar to (but not identical?) to strtok_r().
 * @section Ex1 Example
 * @code
 * for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
 *      unreal_log(ULOG_INFO, "test", "TEST", "Got: $name", log_data_string(name));
 * @endcode
 */
char *strtoken(char **save, char *str, char *fs)
{
	char *pos, *tmp;

	if (str)
		pos = str;	/* new string scan */
	else
		pos = *save; /* keep last position across calls */

	while (pos && *pos && strchr(fs, *pos) != NULL)
		pos++;		/* skip leading separators */

	if (!pos || !*pos)
		return (pos = *save = NULL);	/* string contains only sep's */

	tmp = pos;		/* now, keep position of the token */

	while (*pos && strchr(fs, *pos) == NULL)
		pos++;		/* skip content of the token */

	if (*pos)
		*pos++ = '\0';	/* remove first sep after the token */
	else
		pos = NULL;	/* end of string */

	*save = pos;
	return (tmp);
}

/** Walk through a string of tokens, using a set of separators.
 * This is the special version that won't skip/merge tokens,
 * eg "a,,c" would return "a", then "" (empty), then "c".
 * This in contrast to strtoken() which would return "a" and then "c".
 * This strtoken_noskip() will also not skip tokens at the
 * beginning, eg ",,c" would return "" (empty), "" (empty), "c".
 *
 * @param save	Pointer used for saving between calls
 * @param str	String to parse (will be altered!)
 * @param fs	Separator character(s)
 * @returns substring (token)
 * @note This function works similar to (but not identical?) to strtok_r().
 */
char *strtoken_noskip(char **save, char *str, char *fs)
{
	char *pos, *tmp;

	if (str)
	{
		pos = str;	/* new string scan */
	} else {
		if (*save == NULL)
		{
			/* We reached the end of the string */
			return NULL;
		}
		pos = *save; /* keep last position across calls */
	}

	tmp = pos; /* start position, used for returning later */

	/* Hunt for next separator (fs in pos) */
	while (*pos && !strchr(fs, *pos))
		pos++;

	if (!*pos)
	{
		/* Next call is end of string */
		*save = NULL;
		*pos++ = '\0';
	} else {
		*pos++ = '\0';
		*save = pos;
	}

	return tmp;
}

/** Convert binary address to an IP string - like inet_ntop but will always return the uncompressed IPv6 form.
 * @param af	Address family (AF_INET, AF_INET6)
 * @param in	Address (binary)
 * @param out	Buffer to use for storing the returned IP string
 * @param size	Size of the 'out' buffer
 * @returns IP address as a string (IPv4 or IPv6, in case of the latter:
 *          always the uncompressed form without ::)
 */
const char *inetntop(int af, const void *in, char *out, size_t size)
{
	char tmp[MYDUMMY_SIZE];

	inet_ntop(af, in, tmp, size);
	if (!strstr(tmp, "::"))
	{
		/* IPv4 or IPv6 that is already uncompressed */
		strlcpy(out, tmp, size);
	} else
	{
		char cnt = 0, *cp = tmp, *op = out;

		/* It's an IPv6 compressed address that we need to expand */
		while (*cp)
		{
			if (*cp == ':')
				cnt += 1;
			if (*cp++ == '.')
			{
				cnt += 1;
				break;
			}
		}
		cp = tmp;
		while (*cp)
		{
			*op++ = *cp++;
			if (*(cp - 1) == ':' && *cp == ':')
			{
				if ((cp - 1) == tmp)
				{
					op--;
					*op++ = '0';
					*op++ = ':';
				}

				*op++ = '0';
				while (cnt++ < 7)
				{
					*op++ = ':';
					*op++ = '0';
				}
			}
		}
		if (*(op - 1) == ':')
			*op++ = '0';
		*op = '\0';
	}
	return out;
}

/** Cut string off at the first occurance of CR or LF */
void stripcrlf(char *c)
{
	for (; *c; c++)
	{
		if ((*c == '\n') || (*c == '\r'))
		{
			*c = '\0';
			return;
		}
	}
}

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen)
{
	const char *end = memchr (s, 0, maxlen);
	return end ? (size_t)(end - s) : maxlen;
}
#endif

#ifndef HAVE_STRLCPY
/** BSD'ish strlcpy().
 * The strlcpy() function copies up to size-1 characters from the
 * NUL-terminated string src to dst, NUL-terminating the result.
 * Return: total length of the string tried to create.
 */
size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t len = strlen(src);
	size_t ret = len;

	if (size <= 0)
		return 0;
	if (len >= size)
		len = size - 1;
	memcpy(dst, src, len);
	dst[len] = 0;

	return ret;
}
#endif

#ifndef HAVE_STRLNCPY
/** BSD'ish strlncpy() - similar to strlcpy but never copies more then n characters.
 */
size_t strlncpy(char *dst, const char *src, size_t size, size_t n)
{
	size_t len = strnlen(src, n);
	size_t ret = len;

	if (size <= 0)
		return 0;
	if (len >= size)
		len = size - 1;
	memcpy(dst, src, len);
	dst[len] = 0;

	return ret;
}
#endif

#ifndef HAVE_STRLCAT
/* BSD'ish strlcat().
 * The strlcat() function appends the NUL-terminated string src to the end of
 * dst. It will append at most size - strlen(dst) - 1 bytes, NUL-terminating
 * the result.
 */
size_t strlcat(char *dst, const char *src, size_t size)
{
	size_t len1 = strlen(dst);
	size_t len2 = strlen(src);
	size_t ret = len1 + len2;

	if (size <= len1)
		return size;
	if (len1 + len2 >= size)
		len2 = size - (len1 + 1);

	if (len2 > 0) {
		memcpy(dst + len1, src, len2);
		dst[len1 + len2] = 0;
	}
	
	return ret;
}
#endif

#ifndef HAVE_STRLNCAT
/** BSD'ish strlncat() - similar to strlcat but never cat more then n characters.
 */
size_t strlncat(char *dst, const char *src, size_t size, size_t n)
{
	size_t len1 = strlen(dst);
	size_t len2 = strnlen(src, n);
	size_t ret = len1 + len2;

	if (size <= len1)
		return size;
		
	if (len1 + len2 >= size)
		len2 = size - (len1 + 1);

	if (len2 > 0) {
		memcpy(dst + len1, src, len2);
		dst[len1 + len2] = 0;
	}

	return ret;
}
#endif

/** Like strlcpy but concat one letter */
void strlcat_letter(char *buf, char c, size_t buflen)
{
	int n = strlen(buf);
	if (!buflen || (n >= buflen-1))
		return;
	buf[n] = c;
	buf[n+1] = '\0';
}

/** Copies a string and ensure the new buffer is at most 'max' size, including NUL.
 * The syntax is pretty much identical to strlcpy() except that
 * the buffer is newly allocated.
 * If you wonder why not use strndup() instead?
 * I feel that mixing code with strlcpy() and strndup() would be
 * rather confusing since strlcpy() assumes buffer size INCLUDING
 * the nul byte and strndup() assumes WITHOUT the nul byte and
 * will write one character extra. Hence this strldup(). -- Syzop
 */
char *strldup(const char *src, size_t max)
{
	char *ptr;
	int n;

	if ((max == 0) || !src)
		return NULL;

	n = strlen(src);
	if (n > max-1)
		n = max-1;

	ptr = safe_alloc(n+1);
	memcpy(ptr, src, n);
	ptr[n] = '\0';

	return ptr;
}

static const char Base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
   The following encoding technique is taken from RFC 1521 by Borenstein
   and Freed.  It is reproduced here in a slightly edited form for
   convenience.

   A 65-character subset of US-ASCII is used, enabling 6 bits to be
   represented per printable character. (The extra 65th character, "=",
   is used to signify a special processing function.)

   The encoding process represents 24-bit groups of input bits as output
   strings of 4 encoded characters. Proceeding from left to right, a
   24-bit input group is formed by concatenating 3 8-bit input groups.
   These 24 bits are then treated as 4 concatenated 6-bit groups, each
   of which is translated into a single digit in the base64 alphabet.

   Each 6-bit group is used as an index into an array of 64 printable
   characters. The character referenced by the index is placed in the
   output string.

                         Table 1: The Base64 Alphabet

      Value Encoding  Value Encoding  Value Encoding  Value Encoding
          0 A            17 R            34 i            51 z
          1 B            18 S            35 j            52 0
          2 C            19 T            36 k            53 1
          3 D            20 U            37 l            54 2
          4 E            21 V            38 m            55 3
          5 F            22 W            39 n            56 4
          6 G            23 X            40 o            57 5
          7 H            24 Y            41 p            58 6
          8 I            25 Z            42 q            59 7
          9 J            26 a            43 r            60 8
         10 K            27 b            44 s            61 9
         11 L            28 c            45 t            62 +
         12 M            29 d            46 u            63 /
         13 N            30 e            47 v
         14 O            31 f            48 w         (pad) =
         15 P            32 g            49 x
         16 Q            33 h            50 y

   Special processing is performed if fewer than 24 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a quantity.  When fewer than 24 input
   bits are available in an input group, zero bits are added (on the
   right) to form an integral number of 6-bit groups.  Padding at the
   end of the data is performed using the '=' character.

   Since all base64 input is an integral number of octets, only the
         -------------------------------------------------                       
   following cases can arise:
   
       (1) the final quantum of encoding input is an integral
           multiple of 24 bits; here, the final unit of encoded
	   output will be an integral multiple of 4 characters
	   with no "=" padding,
       (2) the final quantum of encoding input is exactly 8 bits;
           here, the final unit of encoded output will be two
	   characters followed by two "=" padding characters, or
       (3) the final quantum of encoding input is exactly 16 bits;
           here, the final unit of encoded output will be three
	   characters followed by one "=" padding character.
   */

/** Base64 encode data.
 * @param src		The data to encode (input)
 * @param srclength	The length of the data to encode (input length)
 * @param target	The output buffer to use (output)
 * @param targetsize	The length of the output buffer to use (maximum output length)
 * @returns length of the targetsize, or -1 in case of error.
 */
int b64_encode(unsigned char const *src, size_t srclength, char *target, size_t targsize)
{
	size_t datalength = 0;
	u_char input[3];
	u_char output[4];
	size_t i;

	while (2 < srclength) {
		input[0] = *src++;
		input[1] = *src++;
		input[2] = *src++;
		srclength -= 3;

		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
		output[3] = input[2] & 0x3f;

		if (datalength + 4 > targsize)
			return (-1);
		target[datalength++] = Base64[output[0]];
		target[datalength++] = Base64[output[1]];
		target[datalength++] = Base64[output[2]];
		target[datalength++] = Base64[output[3]];
	}
    
	/* Now we worry about padding. */
	if (0 != srclength) {
		/* Get what's left. */
		input[0] = input[1] = input[2] = '\0';
		for (i = 0; i < srclength; i++)
			input[i] = *src++;
	
		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);

		if (datalength + 4 > targsize)
			return (-1);
		target[datalength++] = Base64[output[0]];
		target[datalength++] = Base64[output[1]];
		if (srclength == 1)
			target[datalength++] = Pad64;
		else
			target[datalength++] = Base64[output[2]];
		target[datalength++] = Pad64;
	}
	if (datalength >= targsize)
		return (-1);
	target[datalength] = '\0';	/* Returned value doesn't count \0. */
	return (datalength);
}

/** Base64 decode a string.
 * @param src		The data to decode (input)
 * @param srclength	The length of the data to decode (input length)
 * @param target	The output buffer to use (output)
 * @param targetsize	The length of the output buffer to use (maximum output length)
 * @returns length of the targetsize, or -1 in case of error.
 * @note Skips whitespace, and hmm.. I think we don't require padding? Not sure.
 */
int b64_decode(char const *src, unsigned char *target, size_t targsize)
{
	int tarindex, state, ch;
	char *pos;

	state = 0;
	tarindex = 0;

	while ((ch = *src++) != '\0') {
		if (isspace(ch))	/* Skip whitespace anywhere. */
			continue;

		if (ch == Pad64)
			break;

		pos = strchr(Base64, ch);
		if (pos == 0) 		/* A non-base64 character. */
			return (-1);

		switch (state) {
		case 0:
			if (target) {
				if ((size_t)tarindex >= targsize)
					return (-1);
				target[tarindex] = (pos - Base64) << 2;
			}
			state = 1;
			break;
		case 1:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize)
					return (-1);
				target[tarindex]   |=  (pos - Base64) >> 4;
				target[tarindex+1]  = ((pos - Base64) & 0x0f)
							<< 4 ;
			}
			tarindex++;
			state = 2;
			break;
		case 2:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize)
					return (-1);
				target[tarindex]   |=  (pos - Base64) >> 2;
				target[tarindex+1]  = ((pos - Base64) & 0x03)
							<< 6;
			}
			tarindex++;
			state = 3;
			break;
		case 3:
			if (target) {
				if ((size_t)tarindex >= targsize)
					return (-1);
				target[tarindex] |= (pos - Base64);
			}
			tarindex++;
			state = 0;
			break;
		default:
			abort();
		}
	}

	/*
	 * We are done decoding Base-64 chars.  Let's see if we ended
	 * on a byte boundary, and/or with erroneous trailing characters.
	 */

	if (ch == Pad64) {		/* We got a pad char. */
		ch = *src++;		/* Skip it, get next. */
		switch (state) {
		case 0:		/* Invalid = in first position */
		case 1:		/* Invalid = in second position */
			return (-1);

		case 2:		/* Valid, means one byte of info */
			/* Skip any number of spaces. */
			for (; ch != '\0'; ch = *src++)
				if (!isspace(ch))
					break;
			/* Make sure there is another trailing = sign. */
			if (ch != Pad64)
				return (-1);
			ch = *src++;		/* Skip the = */
			/* Fall through to "single trailing =" case. */
			/* FALLTHROUGH */

		case 3:		/* Valid, means two bytes of info */
			/*
			 * We know this char is an =.  Is there anything but
			 * whitespace after it?
			 */
			for (; ch != '\0'; ch = *src++)
				if (!isspace(ch))
					return (-1);

			/*
			 * Now make sure for cases 2 and 3 that the "extra"
			 * bits that slopped past the last full byte were
			 * zeros.  If we don't check them, they become a
			 * subliminal channel.
			 */
			if (target && target[tarindex] != 0)
				return (-1);
		}
	} else {
		/*
		 * We ended by seeing the end of the string.  Make sure we
		 * have no partial bytes lying around.
		 */
		if (state != 0)
			return (-1);
	}

	return (tarindex);
}

/* Natural sort case comparison routines. The following copyright header applies:
  strnatcmp.c -- Perform 'natural order' comparisons of strings in C.
  Copyright (C) 2000, 2004 by Martin Pool <mbp sourcefrog net>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

 The code was modified / UnrealIRCderized by Bram Matthys ("Syzop").
 We always have unsigned char and this makes it possible to get rid
 of various stuff.. also re-indent this monster.
*/

static int compare_right(char const *a, char const *b)
{
	int bias = 0;

	/* The longest run of digits wins.  That aside, the greatest
	 * value wins, but we can't know that it will until we've scanned
	 * both numbers to know that they have the same magnitude, so we
	 * remember it in BIAS.
	 */
	for (;; a++, b++)
	{
		if (!isdigit(*a) && !isdigit(*b))
			return bias;
		if (!isdigit(*a))
			return -1;
		if (!isdigit(*b))
			return +1;
		if (*a < *b)
		{
			if (!bias)
				bias = -1;
		} else
		if (*a > *b)
		{
			if (!bias)
				bias = +1;
		} else
		if (!*a && !*b)
			return bias;
	}

	return 0;
}


static int compare_left(char const *a, char const *b)
{
	/* Compare two left-aligned numbers: the first to have a
	 * different value wins.
	 */
	for (;; a++, b++)
	{
		if (!isdigit(*a) && !isdigit(*b))
			return 0;
		if (!isdigit(*a))
			return -1;
		if (!isdigit(*b))
			return +1;
		if (*a < *b)
			return -1;
		if (*a > *b)
			return +1;
	}

	return 0;
}

static int strnatcmp0(char const *a, char const *b, int fold_case)
{
	int ai, bi;
	char ca, cb;
	int fractional, result;

	ai = bi = 0;
	while (1)
	{
		ca = a[ai]; cb = b[bi];

		/* skip over leading spaces or zeros */
		while (isspace(ca))
			ca = a[++ai];

		while (isspace(cb))
			cb = b[++bi];

		/* process run of digits */
		if (isdigit(ca)  &&  isdigit(cb))
		{
			fractional = (ca == '0' || cb == '0');
			if (fractional)
			{
				if ((result = compare_left(a+ai, b+bi)) != 0)
					return result;
			} else {
				if ((result = compare_right(a+ai, b+bi)) != 0)
					return result;
			}
		}

		if (!ca && !cb)
		{
			/* The strings compare the same.  Perhaps the caller
			 * will want to call strcmp to break the tie.
			 */
			return 0;
		}

		if (fold_case)
		{
			ca = toupper(ca);
			cb = toupper(cb);
		}

		if (ca < cb)
			return -1;

		if (ca > cb)
			return +1;

		ai++;
		bi++;
	}
}

/** Like strcmp() but with "natural sort", so that for example
 * the string "1.4.10" is seen as higher than "1.4.9"
 * This is the case sensitive version.
 */
int strnatcmp(char const *a, char const *b)
{
	return strnatcmp0(a, b, 0);
}

/** Like strcmp() but with "natural sort", so that for example
 * the string "1.4.10" is seen as higher than "1.4.9"
 * This is the case insensitive version.
 */
int strnatcasecmp(char const *a, char const *b)
{
	return strnatcmp0(a, b, 1);
}

/* End of natural sort case comparison functions */

/* Memory allocation routines */

/** Allocate memory - should always be used instead of malloc/calloc.
 * @param size How many bytes to allocate
 * @returns A pointer to the newly allocated memory.
 * @note If out of memory then the IRCd will exit.
 */
void *safe_alloc(size_t size)
{
	void *p;
	if (size == 0)
		return NULL;
	p = calloc(1, size);
	if (!p)
		outofmemory(size);
	return p;
}

/** Safely duplicate a string */
char *our_strdup(const char *str)
{
	char *ret = strdup(str);
	if (!ret)
		outofmemory(strlen(str));
	return ret;
}

/** Safely duplicate a string with a maximum size */
char *our_strldup(const char *str, size_t max)
{
	char *ret = strldup(str, max);
	if (!ret)
		outofmemory(MAX(strlen(str), max));
	return ret;
}

/** Called when out of memory */
void outofmemory(size_t bytes)
{
	static int log_attempt = 1;

	if (log_attempt)
	{
		/* This will probably fail, but we can try... */
		unreal_log(ULOG_ERROR, "main", "OUT_OF_MEMORY", NULL,
		           "Out of memory while trying to allocate $bytes bytes!",
		           log_data_integer("bytes", bytes));
		log_attempt = 0;
	}
	exit(7);
}

/** Allocate sensitive memory - this should only be used for HIGHLY sensitive data, since
 * it wastes 8192+ bytes even if only asked to allocate for example 32 bytes (this is by design).
 * @param size How many bytes to allocate
 * @returns A pointer to the newly allocated memory.
 * @note If out of memory then the IRCd will exit.
 */
void *safe_alloc_sensitive(size_t size)
{
	void *p;
	if (size == 0)
		return NULL;
	p = sodium_malloc(((size/32)*32)+32);
	if (!p)
		outofmemory(size);
	memset(p, 0, size);
	return p;
}

/** Safely duplicate a string */
char *our_strdup_sensitive(const char *str)
{
	char *ret = safe_alloc_sensitive(strlen(str)+1);
	if (!ret)
		outofmemory(strlen(str));
	strcpy(ret, str); /* safe, see above */
	return ret;
}

/** Returns a unique filename in the specified directory
 * using the specified suffix. The returned value will
 * be of the form <dir>/<random-hex>.<suffix>
 */
char *unreal_mktemp(const char *dir, const char *suffix)
{
	FILE *fd;
	unsigned int i;
	static char tempbuf[PATH_MAX+1];

	for (i = 500; i > 0; i--)
	{
		snprintf(tempbuf, PATH_MAX, "%s/%X.%s", dir, getrandom32(), suffix);
		fd = fopen(tempbuf, "r");
		if (!fd)
			return tempbuf;
		fclose(fd);
	}
	config_error("Unable to create temporary file in directory '%s': %s",
		dir, strerror(errno)); /* eg: permission denied :p */
	return NULL; 
}

/** Returns the path portion of the given path/file
 * in the specified location (must be at least PATH_MAX bytes).
 */
char *unreal_getpathname(const char *filepath, char *path)
{
	const char *end = filepath+strlen(filepath);

	while (*end != '\\' && *end != '/' && end > filepath)
		end--;
	if (end == filepath)
		path = NULL;
	else
	{
		int size = end-filepath;
		if (size >= PATH_MAX)
			path = NULL;
		else
		{
			memcpy(path, filepath, size);
			path[size] = 0;
		}
	}
	return path;
}

/** Returns the filename portion of the given path.
 * The original string is not modified
 */
const char *unreal_getfilename(const char *path)
{
	int len = strlen(path);
	const char *end;

	if (!len)
		return NULL;

	end = path+len-1;
	if (*end == '\\' || *end == '/')
		return NULL;

	while (end > path)
	{
		if (*end == '\\' || *end == '/')
		{
			end++;
			break;
		}
		end--;
	}

	return end;
}

/** Wrapper for mkdir() so you don't need ifdefs everywhere for Windows.
 * @returns 0 on failure!! (like mkdir)
 */
int unreal_mkdir(const char *pathname, mode_t mode)
{
#ifdef _WIN32
	return mkdir(pathname);
#else
	return mkdir(pathname, mode);
#endif
}

/** Create the entire directory structure.
 * @param dname	The directory name, eg /home/irc/unrealircd/logs/2022/08/05
 * @param mode	The mode to create with, eg 0777. Ignored on Windows.
 * @returns 1 on success, 0 on failure.
 */
int unreal_create_directory_structure(const char *dname, mode_t mode)
{
	if (unreal_mkdir(dname, mode) == 0)
	{
		/* Ok, that failed as well, we have some work to do:
		 * for every path element run mkdir().
		 */
		int lastresult;
		char buf[512], *p;
		strlcpy(buf, dname, sizeof(buf)); /* work on a copy */
		for (p=strchr(buf+1, '/'); p; p=strchr(p+1, '/'))
		{
			*p = '\0';
			unreal_mkdir(buf,mode);
			*p = '/';
		}
		/* Finally, try the complete path */
		if (unreal_mkdir(dname, mode))
			return 0; /* failed */
		/* fallthrough.... */
	}
	return 1; /* success */
}

/** Create entire directory structure for a path with a filename.
 * @param fname	The full path name, eg /home/irc/unrealircd/logs/2022/08/05/ircd.log
 * @param mode	The mode to create with, eg 0777. Ignored on Windows.
 * @notes This is used as an easier way to call unreal_create_directory_structure()
 *        if you have a filename instead of the directory part.
 * @returns 1 on success, 0 on failure.
 */
int unreal_create_directory_structure_for_file(const char *fname, mode_t mode)
{
	char buf[PATH_MAX+1];
	const char *path = unreal_getpathname(fname, buf);
	if (!path)
		return 0;
	return unreal_create_directory_structure(path, mode);
}

/** Returns the special module tmp name for a given path.
 * The original string is not modified.
 */
const char *unreal_getmodfilename(const char *path)
{
	static char ret[512];
	char buf[512];
	char *p;
	char *name = NULL;
	char *directory = NULL;
	
	if (BadPtr(path))
		return path;
	
	strlcpy(buf, path, sizeof(buf));
	
	/* Backtrack... */
	for (p = buf + strlen(buf); p >= buf; p--)
	{
		if ((*p == '/') || (*p == '\\'))
		{
			name = p+1;
			*p = '\0';
			directory = buf; /* fallback */
			for (; p >= buf; p--)
			{
				if ((*p == '/') || (*p == '\\'))
				{
					directory = p + 1;
					break;
				}
			}
			break;
		}
	}
	
	if (!name)
		name = buf;
	
	if (!directory || !strcmp(directory, "modules"))
		snprintf(ret, sizeof(ret), "%s", name);
	else
		snprintf(ret, sizeof(ret), "%s.%s", directory, name);
	
	return ret;
}

/* Returns a consistent filename for the cache/ directory.
 * Returned value will be like: cache/<hash of url>
 */
const char *unreal_mkcache(const char *url)
{
	static char tempbuf[PATH_MAX+1];
	char tmp2[128];
	
	snprintf(tempbuf, PATH_MAX, "%s/%s", CACHEDIR, sha256hash(tmp2, url, strlen(url)));
	return tempbuf;
}

/** Returns 1 if a cached version of the url exists, otherwise 0. */
int has_cached_version(const char *url)
{
	return file_exists(unreal_mkcache(url));
}

/** Used to blow away result of bad copy or cancel file copy */
void cancel_copy(int srcfd, int destfd, const char *dest)
{
	close(srcfd);
	close(destfd);
	unlink(dest);
}

/** Copys the contents of the src file to the dest file.
 * The dest file will have permissions r-x------
 */
int unreal_copyfile(const char *src, const char *dest)
{
	char buf[2048];
	time_t mtime;
	int srcfd, destfd, len;

	mtime = unreal_getfilemodtime(src);

#ifndef _WIN32
	srcfd = open(src, O_RDONLY);
#else
	srcfd = open(src, _O_RDONLY|_O_BINARY);
#endif

	if (srcfd < 0)
	{
		config_error("Unable to open file '%s': %s", src, strerror(errno));
		return 0;
	}

#ifndef _WIN32
#if defined(DEFAULT_PERMISSIONS) && (DEFAULT_PERMISSIONS != 0)
	destfd  = open(dest, O_WRONLY|O_CREAT, DEFAULT_PERMISSIONS);
#else
	destfd  = open(dest, O_WRONLY|O_CREAT, S_IRUSR | S_IXUSR);
#endif /* DEFAULT_PERMISSIONS */
#else
	destfd = open(dest, _O_BINARY|_O_WRONLY|_O_CREAT, _S_IWRITE);
#endif /* _WIN32 */
	if (destfd < 0)
	{
		config_error("Unable to create file '%s': %s", dest, strerror(errno));
		close(srcfd);
		return 0;
	}

	while ((len = read(srcfd, buf, 1023)) > 0)
		if (write(destfd, buf, len) != len)
		{
			config_error("Write error to file '%s': %s [not enough free hd space / quota? need several mb's!]",
				dest, strerror(ERRNO));
			cancel_copy(srcfd,destfd,dest);
			return 0;
		}

	if (len < 0) /* very unusual.. perhaps an I/O error */
	{
		config_error("Read error from file '%s': %s", src, strerror(errno));
		cancel_copy(srcfd,destfd,dest);
		return 0;
	}

	close(srcfd);
	close(destfd);
	unreal_setfilemodtime(dest, mtime);
	return 1;
}

/** Same as unreal_copyfile, but with an option to try hardlinking first */
int unreal_copyfileex(const char *src, const char *dest, int tryhardlink)
{
	unlink(dest);
#ifndef _WIN32
	/* Try a hardlink first... */
	if (tryhardlink && !link(src, dest))
		return 1; /* success */
#endif
	return unreal_copyfile(src, dest);
}

/** Set the modification time on a file */
void unreal_setfilemodtime(const char *filename, time_t mtime)
{
#ifndef _WIN32
	struct utimbuf utb;
	utb.actime = utb.modtime = mtime;
	utime(filename, &utb);
#else
	FILETIME mTime;
	LONGLONG llValue;
	HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
				  FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;
	llValue = Int32x32To64(mtime, 10000000) + 116444736000000000;
	mTime.dwLowDateTime = (long)llValue;
	mTime.dwHighDateTime = llValue >> 32;
	
	SetFileTime(hFile, &mTime, &mTime, &mTime);
	CloseHandle(hFile);
#endif
}

/** Get the modification time ("last modified") of a file */
time_t unreal_getfilemodtime(const char *filename)
{
#ifndef _WIN32
	struct stat sb;
	if (stat(filename, &sb))
		return 0;
	return sb.st_mtime;
#else
	/* See how much more fun WinAPI programming is??? */
	FILETIME cTime;
	SYSTEMTIME sTime, lTime;
	ULARGE_INTEGER fullTime;
	time_t result;
	HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
				  FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;
	if (!GetFileTime(hFile, NULL, NULL, &cTime))
		return 0;

	CloseHandle(hFile);

	FileTimeToSystemTime(&cTime, &sTime);
	SystemTimeToTzSpecificLocalTime(NULL, &sTime, &lTime);
	SystemTimeToFileTime(&sTime, &cTime);

	fullTime.LowPart = cTime.dwLowDateTime;
	fullTime.HighPart = cTime.dwHighDateTime;
	fullTime.QuadPart -= 116444736000000000;
	fullTime.QuadPart /= 10000000;
	
	return fullTime.LowPart;	
#endif
}

#ifndef	AF_INET6
#define	AF_INET6	AF_MAX+1	/* just to let this compile */
#endif

/** Encode an IP string (eg: "1.2.3.4") to a BASE64 encoded value for S2S traffic */
const char *encode_ip(const char *ip)
{
	static char retbuf[25]; /* returned string */
	char addrbuf[16];

	if (!ip)
		return "*";

	if (strchr(ip, ':'))
	{
		/* IPv6 (likely) */
		inet_pton(AF_INET6, ip, addrbuf);
		/* hack for IPv4-in-IPv6 (::ffff:1.2.3.4) */
		if (addrbuf[0] == 0 && addrbuf[1] == 0 && addrbuf[2] == 0 && addrbuf[3] == 0
		    && addrbuf[4] == 0 && addrbuf[5] == 0 && addrbuf[6] == 0
			&& addrbuf[7] == 0 && addrbuf[8] == 0 && addrbuf[9] == 0
			&& addrbuf[10] == 0xff && addrbuf[11] == 0xff)
		{
			b64_encode(&addrbuf[12], sizeof(struct in_addr), retbuf, sizeof(retbuf));
		} else {
			b64_encode(addrbuf, 16, retbuf, sizeof(retbuf));
		}
	}
	else
	{
		/* IPv4 */
		inet_pton(AF_INET, ip, addrbuf);
		b64_encode((char *)&addrbuf, sizeof(struct in_addr), retbuf, sizeof(retbuf));
	}
	return retbuf;
}

/** Decode a BASE64 encoded string to an IP address string. Used for S2S traffic. */
const char *decode_ip(const char *buf)
{
	int n;
	char targ[25];
	static char result[64];

	n = b64_decode(buf, targ, sizeof(targ));
	if (n == 4) /* should be IPv4 */
		return inetntop(AF_INET, targ, result, sizeof(result));
	else if (n == 16) /* should be IPv6 */
		return inetntop(AF_INET6, targ, result, sizeof(result));
	else /* Error! */
		return NULL;
}

/* IPv6 stuff */

#ifndef IN6ADDRSZ
#define	IN6ADDRSZ	16
#endif

#ifndef INT16SZ
#define	INT16SZ		 2
#endif

#ifndef INADDRSZ
#define	INADDRSZ	 4
#endif

#ifdef _WIN32
/* Microsoft makes things nice and fun for us! */
struct u_WSA_errors {
	int error_code;
	char *error_string;
};

/* Must be sorted ascending by error code */
struct u_WSA_errors WSAErrors[] = {
 { WSAEINTR,              "Interrupted system call" },
 { WSAEBADF,              "Bad file number" },
 { WSAEACCES,             "Permission denied" },
 { WSAEFAULT,             "Bad address" },
 { WSAEINVAL,             "Invalid argument" },
 { WSAEMFILE,             "Too many open sockets" },
 { WSAEWOULDBLOCK,        "Operation would block" },
 { WSAEINPROGRESS,        "Operation now in progress" },
 { WSAEALREADY,           "Operation already in progress" },
 { WSAENOTSOCK,           "Socket operation on non-socket" },
 { WSAEDESTADDRREQ,       "Destination address required" },
 { WSAEMSGSIZE,           "Message too long" },
 { WSAEPROTOTYPE,         "Protocol wrong type for socket" },
 { WSAENOPROTOOPT,        "Bad protocol option" },
 { WSAEPROTONOSUPPORT,    "Protocol not supported" },
 { WSAESOCKTNOSUPPORT,    "Socket type not supported" },
 { WSAEOPNOTSUPP,         "Operation not supported on socket" },
 { WSAEPFNOSUPPORT,       "Protocol family not supported" },
 { WSAEAFNOSUPPORT,       "Address family not supported" },
 { WSAEADDRINUSE,         "Address already in use" },
 { WSAEADDRNOTAVAIL,      "Can't assign requested address" },
 { WSAENETDOWN,           "Network is down" },
 { WSAENETUNREACH,        "Network is unreachable" },
 { WSAENETRESET,          "Net connection reset" },
 { WSAECONNABORTED,       "Software caused connection abort" },
 { WSAECONNRESET,         "Connection reset by peer" },
 { WSAENOBUFS,            "No buffer space available" },
 { WSAEISCONN,            "Socket is already connected" },
 { WSAENOTCONN,           "Socket is not connected" },
 { WSAESHUTDOWN,          "Can't send after socket shutdown" },
 { WSAETOOMANYREFS,       "Too many references, can't splice" },
 { WSAETIMEDOUT,          "Connection timed out" },
 { WSAECONNREFUSED,       "Connection refused" },
 { WSAELOOP,              "Too many levels of symbolic links" },
 { WSAENAMETOOLONG,       "File name too long" },
 { WSAEHOSTDOWN,          "Host is down" },
 { WSAEHOSTUNREACH,       "No route to host" },
 { WSAENOTEMPTY,          "Directory not empty" },
 { WSAEPROCLIM,           "Too many processes" },
 { WSAEUSERS,             "Too many users" },
 { WSAEDQUOT,             "Disc quota exceeded" },
 { WSAESTALE,             "Stale NFS file handle" },
 { WSAEREMOTE,            "Too many levels of remote in path" },
 { WSASYSNOTREADY,        "Network subsystem is unavailable" },
 { WSAVERNOTSUPPORTED,    "Winsock version not supported" },
 { WSANOTINITIALISED,     "Winsock not yet initialized" },
 { WSAHOST_NOT_FOUND,     "Host not found" },
 { WSATRY_AGAIN,          "Non-authoritative host not found" },
 { WSANO_RECOVERY,        "Non-recoverable errors" },
 { WSANO_DATA,            "Valid name, no data record of requested type" },
 { WSAEDISCON,            "Graceful disconnect in progress" },
 { WSASYSCALLFAILURE,     "System call failure" },
 { 0,NULL}
};

/** Get socket error string */
const char *sock_strerror(int error)
{
	static char unkerr[64];
	int start = 0;
	int stop = sizeof(WSAErrors)/sizeof(WSAErrors[0])-1;
	int mid;
	
	if (!error)
		return "No error";

	if (error < WSABASEERR) /* Just a regular error code */
		return strerror(error);

	/* Microsoft decided not to use sequential numbers for the error codes,
	 * so we can't just use the array index for the code. But, at least
	 * use a binary search to make it as fast as possible. 
	 */
	while (start <= stop)
	{
		mid = (start+stop)/2;
		if (WSAErrors[mid].error_code > error)
			stop = mid-1;
		
		else if (WSAErrors[mid].error_code < error)
			start = mid+1;
		else
			return WSAErrors[mid].error_string;	
	}
	snprintf(unkerr, sizeof(unkerr), "Unknown Error: %d", error);
	return unkerr;
}
#endif

/** Build a string and replace $variables where needed.
 * See src/modules/blacklist.c for an example.
 * @param inbuf		The input string
 * @param outbuf	The output string
 * @param len		The maximum size of the output string (including NUL)
 * @param name		Array of variables names
 * @param value		Array of variable values
 */
void buildvarstring(const char *inbuf, char *outbuf, size_t len, const char *name[], const char *value[])
{
	NameValuePrioList *list = NULL;
	int i;

	for (i=0; name[i]; i++)
		add_nvplist(&list, 0, name[i], value[i]);

	buildvarstring_nvp(inbuf, outbuf, len, list, 0);
	safe_free_nvplist(list);
}

/** Build a string and replace $variables where needed.
 * See src/modules/blacklist.c for an example.
 * @param inbuf		The input string
 * @param outbuf	The output string
 * @param len		The maximum size of the output string (including NUL)
 * @param nvp		A list of var=value items in the form of a NameValuePrioList
 */
void buildvarstring_nvp(const char *inbuf, char *outbuf, size_t len, NameValuePrioList *list, int flags)
{
	const char *i, *p;
	char *o;
	int left = len - 1;
	int cnt, found;
	NameValuePrioList *n;
	char varname[256];
	static char outputbuf[4096];

#ifdef DEBUGMODE
	if (len <= 0)
		abort();
#endif

	for (i = inbuf, o = outbuf; *i; i++)
	{
		if (*i == '$')
		{
			i++;

			/* $$ = literal $ */
			if (*i == '$')
				goto literal;

			if (!validvarcharacter(*i))
			{
				/* What do we do with things like '$/' ? -- treat literal */
				i--;
				goto literal;
			}
			
			/* find termination */
			for (p=i; validvarcharacter(*p); p++);
			
			/* find variable name in list */
			strlncpy(varname, i, sizeof(varname), p - i);
			n = find_nvplist(list, varname);
			if (n)
			{
				if (n->value)
				{
					const char *output = n->value;
					if (flags & BUILDVARSTRING_URLENCODE)
						output = urlencode(output, outputbuf, sizeof(outputbuf));
					strlcpy(o, output, left);
					left -= strlen(output); /* may become <0 */
					if (left <= 0)
						return; /* return - don't write \0 to 'o'. ensured by strlcpy already */
					o += strlen(output); /* value entirely written */
				}
			} else
			{
				/* variable name does not exist -- treat literal */
				i--;
				goto literal;
			}

			/* value written. we're done. */
			i = p - 1;
			continue;
		}
literal:
		if (!left)
			break;
		*o++ = *i;
		left--;
		if (!left)
			break;
	}
	*o = '\0';
}

/** Return the PCRE2 library version in use */
const char *pcre2_version(void)
{
	static char buf[256];

	strlcpy(buf, "PCRE2 ", sizeof(buf));
	pcre2_config(PCRE2_CONFIG_VERSION, buf+6);
	return buf;
}


#ifdef _WIN32
/** POSIX gettimeofday function for Windows (ignoring timezones) */
int gettimeofday(struct timeval *tp, void *tzp)
{
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970
	static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

	SYSTEMTIME system_time;
	FILETIME file_time;
	uint64_t time;

	GetSystemTime( &system_time );
	SystemTimeToFileTime( &system_time, &file_time );
	time =  ((uint64_t)file_time.dwLowDateTime )      ;
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
	tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
	return 0;
}
#endif

/** Get the numer of characters per line that fit on the terminal (the width) */
int get_terminal_width(void)
{
#if defined(_WIN32) || !defined(TIOCGWINSZ)
	return 80;
#else
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	return w.ws_col;
#endif
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

/** Like strftime() but easier. */
char *unreal_strftime(const char *str)
{
	time_t t;
	struct tm *tmp;
	static char buf[512];

	t = time(NULL);
	tmp = localtime(&t);
	if (!tmp || !strftime(buf, sizeof(buf), str, tmp))
	{
		/* If anything fails bigtime, then return the format string */
		strlcpy(buf, str, sizeof(buf));
		return buf;
	}
	return buf;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/** Convert a string to lowercase - with separate input/output buffer */
void strtolower_safe(char *dst, const char *src, int size)
{
	if (!size)
		return; /* size of 0 is unworkable */
	size--; /* for \0 */

	for (; *src && size; src++)
	{
		*dst++ = tolower(*src);
		size--;
	}
	*dst = '\0';
}

/** Convert a string to lowercase - modifying existing string */
void strtolower(char *str)
{
	for (; *str; str++)
		*str = tolower(*str);
}

/** Convert a string to uppercase - with separate input/output buffer */
void strtoupper_safe(char *dst, const char *src, int size)
{
	if (!size)
		return; /* size of 0 is unworkable */
	size--; /* for \0 */

	for (; *src && size; src++)
	{
		*dst++ = toupper(*src);
		size--;
	}
	*dst = '\0';
}

/** Convert a string to uppercase - modifying existing string */
void strtoupper(char *str)
{
	for (; *str; str++)
		*str = toupper(*str);
}

/** Checks if a string starts with a certain substring - case sensitive version.
 */
int str_starts_with_case_sensitive(const char *haystack, const char *needle)
{
	return !strncmp(haystack, needle, strlen(needle)) ? 1 : 0;
}

/** Checks if a string ends with a certain substring - case sensitive version.
 */
int str_ends_with_case_sensitive(const char *haystack, const char *needle)
{
	int haystacklen = strlen(haystack);
	int needlelen = strlen(needle);

	if (haystacklen < needlelen)
		return 0;

	return !strncmp(haystack+haystacklen-needlelen, needle, needlelen) ? 1 : 0;
}

/** Checks if a string starts with a certain substring - case insensitive version.
 */
int str_starts_with_case_insensitive(const char *haystack, const char *needle)
{
	return !strncasecmp(haystack, needle, strlen(needle)) ? 1 : 0;
}

/** Checks if a string ends with a certain substring - case insensitive version.
 */
int str_ends_with_case_insensitive(const char *haystack, const char *needle)
{
	int haystacklen = strlen(haystack);
	int needlelen = strlen(needle);

	if (haystacklen < needlelen)
		return 0;

	return !strncasecmp(haystack+haystacklen-needlelen, needle, needlelen) ? 1 : 0;
}
