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

/* support.c 2.21 4/13/94 1990, 1991 Armin Gruner; 1992, 1993 Darren Reed */

#include "unrealircd.h"

extern void outofmemory();

#define is_enabled match

char	*my_itoa(int i)
{
	static char buf[128];
#ifndef _WIN32	
	ircsnprintf(buf, sizeof(buf), "%d", i);
#else
	_itoa_s(i, buf, sizeof(buf), 10);
#endif
	return (buf);
}

#ifdef NEED_STRTOKEN
/*
** 	strtoken.c --  	walk through a string of tokens, using a set
**			of separators
**			argv 9/90
*/

char *strtoken(char **save, char *str, char *fs)
{
	char *pos, *tmp;

	if (str)
		pos = str;	/* new string scan */
	else
		pos = *save; /* keep last position across calls */

	while (pos && *pos && index(fs, *pos) != NULL)
		pos++;		/* skip leading separators */

	if (!pos || !*pos)
		return (pos = *save = NULL);	/* string contains only sep's */

	tmp = pos;		/* now, keep position of the token */

	while (*pos && index(fs, *pos) == NULL)
		pos++;		/* skip content of the token */

	if (*pos)
		*pos++ = '\0';	/* remove first sep after the token */
	else
		pos = NULL;	/* end of string */

	*save = pos;
	return (tmp);
}
#endif /* NEED_STRTOKEN */

#ifdef	NEED_STRTOK
/*
** NOT encouraged to use!
*/

char *strtok2(char *str, char *fs)
{
	static char *pos;

	return strtoken(&pos, str, fs);
}

#endif /* NEED_STRTOK */

#ifdef NEED_STRERROR
/*
**	strerror - return an appropriate system error string to a given errno
**
**		   argv 11/90
*/
char *strerror(int err_no)
{
	extern char *sys_errlist[];	/* Sigh... hopefully on all systems */
	extern int sys_nerr;

	static char buff[40];
	char *errp;

	errp = (err_no > sys_nerr ? NULL : sys_errlist[err_no]);

	if (errp == NULL)
	{
		errp = buff;
		(void)ircsnprintf(buff, sizeof(buff), "Unknown Error %d", err_no);

	}
	return errp;
}

#endif /* NEED_STRERROR */

/*
 * inetntop: return the : notation of a given IPv6 internet number.
 *           make sure the compressed representation (rfc 1884) isn't used.
 */
char *inetntop(int af, const void *in, char *out, size_t the_size)
{
	static char local_dummy[MYDUMMY_SIZE];

	inet_ntop(af, in, local_dummy, the_size);
	if (strstr(local_dummy, "::"))
	{
		char cnt = 0, *cp = local_dummy, *op = out;

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
		cp = local_dummy;
		while (*cp)
		{
			*op++ = *cp++;
			if (*(cp - 1) == ':' && *cp == ':')
			{
				if ((cp - 1) == local_dummy)
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
		Debug((DEBUG_DNS, "Expanding `%s' -> `%s'", local_dummy, out));
	}
	else
		bcopy(local_dummy, out, 64);
	return out;
}

/* Made by Potvin originally, i guess */
time_t	atime_exp(char *base, char *ptr)
{
	time_t	tmp;
	char	*p, c = *ptr;
	
	p = ptr;
	*ptr-- = '\0';
	while (ptr-- > base)
		if (isalpha(*ptr))
			break;
	tmp = atoi(ptr + 1);
	*p = c;

	return tmp;
}

#define Xtract(x, y) if (x) y = atime_exp(xtime, x)

time_t	atime(char *xtime)
{
	char *d, *h, *m, *s;
	time_t D, H, M, S;
	int i;
	
	d = h = m = s = NULL;
	D = H = M = S = 0;
	
	
	i = 0;
	for (d = xtime; *d; d++)
		if (isalpha(*d) && (i != 1))
			i = 1;
	if (i == 0)
		return (atol(xtime)); 
	d = strchr(xtime, 'd');
	h = strchr(xtime, 'h');
	m = strchr(xtime, 'm');
	s = strchr(xtime, 's');
	
	Xtract(d, D);
	Xtract(h, H);
	Xtract(m, M);
	Xtract(s, S);

	return ((D * 86400) + (H * 3600) + (M * 60) + S);		
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

#ifndef HAVE_STRLCPY
/*
 * bsd'sh strlcpy().
 * The strlcpy() function copies up to size-1 characters from the
 * NUL-terminated string src to dst, NUL-terminating the result.
 * Return: total length of the string tried to create.
 */
size_t
strlcpy(char *dst, const char *src, size_t size)
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

#ifndef HAVE_STRLCAT
/*
 * bsd'sh strlcat().
 * The strlcat() function appends the NUL-terminated string src to the end of
 * dst. It will append at most size - strlen(dst) - 1 bytes, NUL-terminating
 * the result.
 */
size_t
strlcat(char *dst, const char *src, size_t size)
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
/*
 * see strlcat(), but never cat more then n characters.
 */
size_t
strlncat(char *dst, const char *src, size_t size, size_t n)
{
	size_t len1 = strlen(dst);
	size_t len2 = strlen(src);
	size_t ret = len1 + len2;

	if (size <= len1)
		return size;
		
	if (len2 > n)
		len2 = n;

	if (len1 + len2 >= size)
		len2 = size - (len1 + 1);

	if (len2 > 0) {
		memcpy(dst + len1, src, len2);
		dst[len1 + len2] = 0;
	}

	return ret;
}
#endif

/* strldup(str,max) copies a string and ensures the new buffer
 * is at most 'max' size, including nul byte. The syntax is pretty
 * much identical to strlcpy() except that the buffer is newly
 * allocated.
 * If you wonder why not use strndup() instead?
 * I feel that mixing code with strlcpy() and strndup() would be
 * rather confusing since strlcpy() assumes buffer size including
 * the nul byte and strndup() assumes without the nul byte and
 * will write one character extra. Hence this strldup(). -- Syzop
 */
// WAS: #define strldup(buf, sz) (sz > 0 ? strndup(buf, sz-1) : NULL)
char *strldup(const char *src, size_t max)
{
	char *ptr;
	int n;

	if ((max == 0) || !src)
		return NULL;

	n = strlen(src);
	if (n > max-1)
		n = max-1;

	ptr = MyMallocEx(n+1);
	memcpy(ptr, src, n);
	ptr[n] = '\0';

	return ptr;
}

static const char Base64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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

/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 64 numbers into three 8 bit bytes in the target area.
   it returns the number of data bytes stored at the target, or -1 on error.
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
			for ((void)NULL; ch != '\0'; ch = *src++)
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
			for ((void)NULL; ch != '\0'; ch = *src++)
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

void	*MyMallocEx(size_t size)
{
	void *p = MyMalloc(size);

	bzero(p, size);
	return (p);
}

int file_exists(char* file)
{
	FILE *fd;
	fd = fopen(file, "r");
	if (!fd)
		return 0;
	fclose(fd);
	return 1;
}

/* Returns a unique filename in the specified directory
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

/* Returns the path portion of the given path/file
 * in the specified location (must be at least PATH_MAX
 * bytes).
 */
char *unreal_getpathname(char *filepath, char *path)
{
	char *end = filepath+strlen(filepath);

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

/* Returns the filename portion of the given path
 * The original string is not modified
 */
char *unreal_getfilename(char *path)
{
        int len = strlen(path);
        char *end;
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

/* Returns the special module tmp name for a given path
 * The original string is not modified
 */
char *unreal_getmodfilename(char *path)
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
char *unreal_mkcache(const char *url)
{
	static char tempbuf[PATH_MAX+1];
	char tmp2[33];
	
	snprintf(tempbuf, PATH_MAX, "%s/%s", CACHEDIR, md5hash(tmp2, url, strlen(url)));
	return tempbuf;
}

/* Returns 1 if a cached version of the url exists, otherwise 0. */
int has_cached_version(const char *url)
{
	return file_exists(unreal_mkcache(url));
}

/* Used to blow away result of bad copy or cancel file copy */
void cancel_copy(int srcfd, int destfd, const char* dest)
{
        close(srcfd);
        close(destfd);
        unlink(dest);
}

/* Copys the contents of the src file to the dest file.
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

/* Same as unreal_copyfile, but with an option to try hardlinking first */
int unreal_copyfileex(const char *src, const char *dest, int tryhardlink)
{
#ifndef _WIN32
	/* Try a hardlink first... */
	if (tryhardlink && !link(src, dest))
		return 1; /* success */
#endif
	return unreal_copyfile(src, dest);
}


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

char	*encode_ip(u_char *ip)
{
	static char buf[25];
	u_char *cp;
	struct in_addr ia; /* For IPv4 */
	u_char ia6[16]; /* For IPv6 */

	if (!ip)
		return "*";

	if (strchr(ip, ':'))
	{
		inet_pton(AF_INET6, ip, ia6);
		cp = (u_char *)ia6;
		if (cp[0] == 0 && cp[1] == 0 && cp[2] == 0 && cp[3] == 0 && cp[4] == 0
		    && cp[5] == 0 && cp[6] == 0 && cp[7] == 0 && cp[8] == 0
		    && cp[9] == 0 && cp[10] == 0xff
		    && cp[11] == 0xff)
			b64_encode((char *)&cp[12], sizeof(struct in_addr), buf, 25);
		else
			b64_encode((char *)cp, 16, buf, 25);
	}
	else
	{
		ia.s_addr = inet_addr(ip);
		b64_encode((char *)&ia.s_addr, sizeof(struct in_addr), buf, 25);
	}
	return buf;
}

char *decode_ip(char *buf)
{
	int len = strlen(buf);
	char targ[25];

	b64_decode(buf, targ, 25);
	if (len == 24) /* IPv6 */
	{
		static char result[64];
		return inetntop(AF_INET6, targ, result, 64);
	}
	else if (len == 8) /* IPv4 */
		return inet_ntoa(*(struct in_addr *)targ);
	else /* Error?? */
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

/* Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef HAVE_INET_NTOP
/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static const char *inet_ntop4(const u_char *src, char *dst, size_t size);
static const char *inet_ntop6(const u_char *src, char *dst, size_t size);

/* char *
 * inet_ntop(af, src, dst, size)
 *	convert a network format address to presentation format.
 * return:
 *	pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *	Paul Vixie, 1996.
 */
const char *inet_ntop(int af, const void *src, char *dst, size_t size)
{
	switch (af) {
	case AF_INET:
		return (inet_ntop4(src, dst, size));
	case AF_INET6:
		return (inet_ntop6(src, dst, size));
	default:
#ifndef _WIN32
		errno = EAFNOSUPPORT;
#else
		WSASetLastError(WSAEAFNOSUPPORT);
#endif
		return (NULL);
	}
	/* NOTREACHED */
}

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address, more or less like inet_ntoa()
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a u_char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *inet_ntop4(const u_char *src, char *dst, size_t size)
{
	static const char fmt[] = "%u.%u.%u.%u";
	char tmp[sizeof "255.255.255.255"];

	snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]);
	if ((size_t)strlen(tmp)+1 > size) {
#ifndef _WIN32
		errno = ENOSPC;
#else
		WSASetLastError(WSAENOBUFS);
#endif
		return (NULL);
	}
	strlcpy(dst, tmp, size);
	return (dst);
}

/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
static const char *inet_ntop6(const u_char *src, char *dst, size_t size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct { int base, len; } best, cur;
	uint32_t words[IN6ADDRSZ / INT16SZ];
	int i;

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, 0, sizeof words);
	for (i = 0; i < IN6ADDRSZ; i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	cur.base = -1;
	for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		} else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	if (size < (IN6ADDRSZ / INT16SZ)) return 0;
	tp = tmp;
	for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
		/* Are we inside the best run of 0x00? */
		if (best.base != -1 && i >= best.base &&
		    i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0 &&
		    (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			if (!inet_ntop4(src+12, tp, sizeof tmp - (tp - tmp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		snprintf(tp, sizeof(tmp)-strlen(tmp), "%x", words[i]);
		tp += strlen(tp);
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */
	if ((size_t) (tp - tmp) > size) {
#ifndef _WIN32
		errno = ENOSPC;
#else
		WSASetLastError(WSAENOBUFS);
#endif
		return (NULL);
	}
	strlcpy(dst, tmp, size);
	return (dst);
}

#endif /* !HAVE_INET_NTOP */

#ifndef HAVE_INET_PTON
/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

/* int
 * inet_pton(af, src, dst)
 *	convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * return:
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *	Paul Vixie, 1996.
 */
int inet_pton(int af, const char *src, void *dst)
{
	switch (af) {
	case AF_INET:
		return (inet_pton4(src, dst));
	case AF_INET6:
		return (inet_pton6(src, dst));
	default:
#ifndef _WIN32
		errno = EAFNOSUPPORT;
#else
		WSASetLastError(WSAEAFNOSUPPORT);
#endif
		return (-1);
	}
	/* NOTREACHED */
}
#endif /* !HAVE_INET_PTON */

/* int
 * inet_pton4(src, dst)
 *	like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *	1 if `src' is a valid dotted quad, else 0.
 * notice:
 *	does not touch `dst' unless it's returning 1.
 * author:
 *	Paul Vixie, 1996.
 */
int inet_pton4(const char *src, unsigned char *dst)
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	unsigned char tmp[INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr(digits, ch)) != NULL) {
			u_int new = *tp * 10 + (pch - digits);

			if (new > 255)
				return (0);
			*tp = new;
			if (! saw_digit) {
				if (++octets > 4)
					return (0);
				saw_digit = 1;
			}
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		} else
			return (0);
	}
	if (octets < 4)
		return (0);
	/* bcopy(tmp, dst, INADDRSZ); */
	memcpy(dst, tmp, INADDRSZ);
	return (1);
}

/* int
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 * credit:
 *	inspired by Mark Andrews.
 * author:
 *	Paul Vixie, 1996.
 */
int inet_pton6(const char *src, unsigned char *dst)
{
	static const char xdigits_l[] = "0123456789abcdef",
			  xdigits_u[] = "0123456789ABCDEF";
	unsigned char tmp[IN6ADDRSZ], *tp, *endp, *colonp;
	const char *xdigits, *curtok;
	int ch, saw_xdigit;
	u_int val;

	memset((tp = tmp), 0, IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return (0);
			saw_xdigit = 1;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return (0);
				colonp = tp;
				continue;
			}
			if (tp + INT16SZ > endp)
				return (0);
			*tp++ = (unsigned char) (val >> 8) & 0xff;
			*tp++ = (unsigned char) val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if (ch == '.' && ((tp + INADDRSZ) <= endp) &&
		    inet_pton4(curtok, tp) > 0) {
			tp += INADDRSZ;
			saw_xdigit = 0;
			break;	/* '\0' was seen by inet_pton4(). */
		}
		return (0);
	}
	if (saw_xdigit) {
		if (tp + INT16SZ > endp)
			return (0);
		*tp++ = (unsigned char) (val >> 8) & 0xff;
		*tp++ = (unsigned char) val & 0xff;
	}
	if (colonp != NULL) {
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return (0);
	/* bcopy(tmp, dst, IN6ADDRSZ); */
	memcpy(dst, tmp, IN6ADDRSZ);
	return (1);
}

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

char *sock_strerror(int error)
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

void buildvarstring(const char *inbuf, char *outbuf, size_t len, const char *name[], const char *value[])
{
	const char *i, *p;
	char *o;
	int left = len - 1;
	int cnt, found;

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

			if (!isalnum(*i))
			{
				/* What do we do with things like '$/' ? -- treat literal */
				i--;
				goto literal;
			}
			
			/* find termination */
			for (p=i; isalnum(*p); p++);
			
			/* find variable name in list */
			found = 0;
			for (cnt = 0; name[cnt]; cnt++)
				if (!strncasecmp(name[cnt], i, p - i))
				{
					/* Found */
					found = 1;

					if (!BadPtr(value[cnt]))
					{
						strlcpy(o, value[cnt], left);
						left -= strlen(value[cnt]); /* may become <0 */
						if (left <= 0)
							return; /* return - don't write \0 to 'o'. ensured by strlcpy already */
						o += strlen(value[cnt]); /* value entirely written */
					}

					break; /* done */
				}
			
			if (!found)
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
	}
	*o = '\0';
}

char *pcre2_version(void)
{
	static char buf[256];

	strlcpy(buf, "PCRE2 ", sizeof(buf));
	pcre2_config(PCRE2_CONFIG_VERSION, buf+6);
	return buf;
}
