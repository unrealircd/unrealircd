/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/common.h
 *   Copyright (C) 1990 Armin Gruner
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
 *
 *   $Id$
 */

#ifndef	__common_include__
#define __common_include__

#include <time.h>
#ifdef _WIN32
#include <malloc.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <io.h>
#endif
#include "types.h"
#include "config.h"
#ifdef	PARAMH
#include <sys/param.h>
#endif
#ifndef _WIN32
#include <stdbool.h>
#else
typedef int bool;
#define false 0
#define true 1
#endif

#include "sys.h"

#include "ircsprintf.h"
#include "list.h"

#ifdef DEVELOP_CVS
#define ID_Copyright(x) static char id_copyright[] = x
#define ID_Notes(x) static char id_notes[] = x
#else
#define ID_Copyright(x)
#define ID_Notes(x)
#endif

#define BMAGIC 0x4675636B596F754661736369737473

#define BASE_VERSION "UnrealIRCd"
#ifndef _WIN32
#define FDwrite(x,y,z) write(x, y, z)
#else
#define FDwrite(x,y,z) send(x, y, z, 0)
#endif
#ifndef NULL
#define NULL 0
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define FALSE (0)
#define TRUE  (!FALSE)

#ifndef UNSURE
#define UNSURE (2)
#endif

#if 0
#ifndef	MALLOCH
char *malloc(), *calloc();
void free();
#else
#include MALLOCH
#endif
#endif

#define TS time_t


extern int match(const char *, const char *);
#define mycmp(a,b) \
 ( (toupper(a[0])!=toupper(b[0])) || smycmp((a)+1,(b)+1) )
extern int smycmp(const char *, const char *);
#ifndef GLIBC2_x
extern int myncmp(const char *, const char *, int);
#endif

#ifdef NEED_STRTOK
extern char *strtok2(char *, char *);
#endif
#ifdef NEED_STRTOKEN
extern char *strtoken(char **, char *, char *);
#endif
#ifdef NEED_INET_ADDR
extern unsigned long inet_addr(char *);
#endif

#if defined(NEED_INET_NTOA) || defined(NEED_INET_NETOF) && !defined(_WIN32)
#include <netinet/in.h>
#endif
#ifdef NEED_INET_NTOA
extern char *inet_ntoa(struct in_addr);
#endif

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int, const void *, char *, size_t);
#endif

#ifndef HAVE_INET_PTON
int inet_pton(int af, const char *src, void *dst);
#endif

MODVAR int  global_count, max_global_count;
extern char *myctime(time_t);
extern char *strtoken(char **, char *, char *);

#define PRECISE_CHECK

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define DupString(x,y) do{int l=strlen(y);x=MyMalloc(l+1);(void)memcpy(x,y, l+1);}while(0)

extern MODVAR u_char tolowertab[], touppertab[];

#if defined(NICK_GB2312) || defined(NICK_GBK) || defined(NICK_GBK_JAP)
#define USE_LOCALE
#include <ctype.h>
#endif

#ifndef USE_LOCALE
#undef tolower
#define tolower(c) (tolowertab[(u_char)(c)])

#undef toupper
#define toupper(c) (touppertab[(u_char)(c)])

#undef isalpha
#undef isdigit
#undef isxdigit
#undef isalnum
#undef isprint
#undef isascii
#undef isgraph
#undef ispunct
#undef islower
#undef isupper
#undef isspace
#undef iscntrl
#endif
extern MODVAR unsigned char char_atribs[];

#define PRINT 1
#define CNTRL 2
#define ALPHA 4
#define PUNCT 8
#define DIGIT 16
#define SPACE 32
#define ALLOW 64
#define ALLOWN 128

#ifndef KLINE_TEMP
#define KLINE_PERM 0
#define KLINE_TEMP 1
#define KLINE_AKILL 2
#define KLINE_EXCEPT 3
#endif

#define isallowed(c) (char_atribs[(u_char)(c)]&ALLOW)
#ifndef USE_LOCALE
#define	iscntrl(c) (char_atribs[(u_char)(c)]&CNTRL)
#define isalpha(c) (char_atribs[(u_char)(c)]&ALPHA)
#define isspace(c) (char_atribs[(u_char)(c)]&SPACE)
#define islower(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) > 0x5f))
#define isupper(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) < 0x60))
#define isdigit(c) (char_atribs[(u_char)(c)]&DIGIT)
#define	isxdigit(c) (isdigit(c) || ('a' <= (c) && (c) <= 'f') || \
		     ('A' <= (c) && (c) <= 'F'))
#define isalnum(c) (char_atribs[(u_char)(c)]&(DIGIT|ALPHA))
#define isprint(c) (char_atribs[(u_char)(c)]&PRINT)
#define isascii(c) ((u_char)(c) >= 0 && (u_char)(c) <= 0x7f)
#define isgraph(c) ((char_atribs[(u_char)(c)]&PRINT) && ((u_char)(c) != 0x32))
#define ispunct(c) (!(char_atribs[(u_char)(c)]&(CNTRL|ALPHA|DIGIT)))
#endif
#define iswseperator(c) (!isalnum(c) && !((u_char)c >= 128))

#ifndef MALLOCD
#define MyFree free
#define MyMalloc malloc
#define MyRealloc realloc
#else
#define MyFree(x) do {debug(DEBUG_MALLOC, "%s:%i: free %02x", __FILE__, __LINE__, x); free(x); } while(0)
#define MyMalloc(x) StsMalloc(x, __FILE__, __LINE__)
#define MyRealloc realloc
static char *StsMalloc(size_t size, char *file, long line)
{
	void *x;
	
	x = malloc(size);
	debug(DEBUG_MALLOC, "%s:%i: malloc %02x", file, line, x);
	return x;
}

#endif

#define safestrdup(x,y) do { if (x) MyFree(x); if (!y) x = NULL; else x = strdup(y); } while(0)
#define safefree(x) do { if (x) MyFree(x); x = NULL; } while(0)

extern struct SLink *find_user_link( /* struct SLink *, struct Client * */ );

/*
 * Protocol support text.  DO NO CHANGE THIS unless you know what
 * you are doing.
 */

/* IRCu/Hybrid/Unreal way now :) -Stskeeps */

#define EXPAR1	extchmstr[0]
#define EXPAR2	extchmstr[1]
#define EXPAR3	extchmstr[2]
#define EXPAR4	extchmstr[3]

#ifdef PREFIX_AQ
#define CHPFIX        "(qaohv)~&@%+"
#define CHPAR1        "beI"
#else
#define CHPFIX        "(ohv)@%+"
#define CHPAR1        "beIqa"
#endif /* PREFIX_AQ */

#define CHPAR2        "k"
#define CHPAR3        "l"
#define CHPAR4        "psmntir"


/* Server-Server PROTOCTL -Stskeeps
 * This is the FIRST line only, please check send_proto() for more. -- Syzop
 * Also take MAXPARA into account !
 */
#define PROTOCTL_SERVER "NOQUIT" \
                        " NICKv2" \
                        " SJOIN" \
                        " SJOIN2" \
                        " UMODE2" \
                        " VL" \
                        " SJ3" \
                        " TKLEXT" \
                        " TKLEXT2" \
                        " NICKIP" \
                        " ESVID"

#ifdef _WIN32
/*
 * Used to display a string to the GUI interface.
 * Windows' internal strerror() function doesn't work with socket errors.
 */
extern int DisplayString(HWND hWnd, char *InBuf, ...);
#else
typedef int SOCKET;
#define INVALID_SOCKET -1
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
extern char *malloc_options;
#endif

extern int lu_noninv, lu_inv, lu_serv, lu_oper,
    lu_unknown, lu_channel, lu_lu, lu_lulocal, lu_lserv,
    lu_clu, lu_mlu, lu_cglobalu, lu_mglobalu;

MODVAR TS   now;

#ifndef _WIN32
#if defined(__STDC__)
#define __const         const
#define __signed        signed
#define __volatile      volatile
#ifndef __GNUC__
#define __inline
#endif

#else
#ifndef __GNUC__
#define __const
#define __inline
#define __signed
#define __volatile
#ifndef NO_ANSI_KEYWORDS
#define const                           /* delete ANSI C keywords */
#define inline
#define signed
#define volatile
#endif
#endif
#endif
#else
#define inline __inline
#endif

#define READBUF_SIZE 8192

#endif /* __common_include__ */
