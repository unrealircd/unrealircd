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
#include <windows.h>
#include <winsock.h>
#include <process.h>
#include <io.h>
#endif
#include "dynconf.h"
#include "ircsprintf.h"

#ifdef	PARAMH
#include <sys/param.h>
#endif

#if !defined(IN_ADDR)
#include "sys.h"
#endif

#ifdef DEVELOP_CVS
#define ID_Copyright(x) static char id_copyright[] = x
#define ID_Notes(x) static char id_notes[] = x
#else
#define ID_Copyright(x)
#define ID_Notes(x)
#endif

#define BMAGIC 0x4675636B596F754661736369737473

#define BASE_VERSION "Unreal"
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


extern int match(char *, char *);
#define mycmp(a,b) \
 ( (toupper(a[0])!=toupper(b[0])) || smycmp((a)+1,(b)+1) )
extern int smycmp(char *, char *);
#ifndef GLIBC2_x
extern int myncmp(char *, char *, int);
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
extern char *inet_ntoa(struct IN_ADDR);
#endif

#ifdef NEED_INET_NETOF
extern int inet_netof(struct IN_ADDR);
#endif

int  global_count, max_global_count;
extern char *myctime(time_t);
extern char *strtoken(char **, char *, char *);

#define PRECISE_CHECK

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define DupString(x,y) do{x=MyMalloc(strlen(y)+1);(void)strcpy(x,y);}while(0)

extern u_char tolowertab[], touppertab[];

#if defined(CHINESE_NICK) || defined(JAPANESE_NICK)
#define USE_LOCALE
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
extern unsigned char char_atribs[];

#define PRINT 1
#define CNTRL 2
#define ALPHA 4
#define PUNCT 8
#define DIGIT 16
#define SPACE 32
#define ALLOW 64

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

extern struct SLink *find_user_link( /* struct SLink *, struct Client * */ );

#define EVENT_HASHVALUE 337
#define EVENT_CHECKIT match
#define EVENT_CRC unreallogo
/*
 * Protocol support text.  DO NO CHANGE THIS unless you know what
 * you are doing.
 */

#ifdef ZIP_LINKS
#define ZIPSTUFF " ZIP"
#else
#define ZIPSTUFF ""
#endif

/* IRCu/Hybrid/Unreal way now :) -Stskeeps */

#define PROTOCTL_CLIENT           \
		" MAP"            \
		" KNOCK"          \
		" SAFELIST"       \
		" HCN"	          \
		" WALLCHOPS"	  \
		" WATCH=%i"       \
		" SILENCE=%i"     \
		" MODES=%i"       \
		" MAXCHANNELS=%i" \
		" MAXBANS=%i"     \
		" NICKLEN=%i"     \
		" TOPICLEN=%i"    \
		" KICKLEN=%i"     \
		" CHANTYPES=%s"    \
		" PREFIX=%s"     \
		" CHANMODES=%s,%s,%s,%s" \
		" NETWORK=%s" \
		" :are supported by this server"

#define PROTOCTL_PARAMETERS MAXWATCH, \
                            MAXSILES, \
                            MAXMODEPARAMS, \
                            MAXCHANNELSPERUSER, \
                            MAXBANS, \
                            NICKLEN, \
                            TOPICLEN, \
                            TOPICLEN, \
                            "#",      \
                            "(ohv)@%+", \
                            "ohvbeqa", \
                            "k", \
			    "lfL", \
			    "psmntirRcOAQKVHGCuzN", \
			    ircnet005
			    
/* Server-Server PROTOCTL -Stskeeps */
#define PROTOCTL_SERVER "NOQUIT" \
                        " TOKEN" \
                        " NICKv2" \
                        " SJOIN" \
                        " SJOIN2" \
                        " UMODE2" \
                        " VL" \
                        " SJ3" \
                        " NS" \
                        " SJB64" \
                        ZIPSTUFF

#ifdef _WIN32
/*
 * Used to display a string to the GUI interface.
 * Windows' internal strerror() function doesn't work with socket errors.
 */
extern int DisplayString(HWND hWnd, char *InBuf, ...);
#undef	strerror
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

TS   now;

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


#endif /* __common_include__ */
