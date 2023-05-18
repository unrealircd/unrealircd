/************************************************************************
 * UnrealIRCd - Unreal Internet Relay Chat Daemon - src/ircd_vars.c
 * (c) 2021- Bram Matthys and The UnrealIRCd team
 * License: GPLv2 or later
 */
#include "unrealircd.h"

/** @file
 * @brief UnrealIRCd global variables of the IRCd
 */

int SVSNOOP = 0;
time_t timeofday = 0;
struct timeval timeofday_tv;
int tainted = 0;
LoopStruct loop;
MODVAR IRCCounts irccounts;
MODVAR Client me;			/* That's me */
MODVAR char *me_hash;
char *configfile = NULL; 	/* Server configuration file */
int debuglevel = 0;		/* Server debug level */
int bootopt = 0;		/* Server boot option flags */
char *debugmode = "";		/*  -"-    -"-   -"-  */
int dorehash = 0;		/**< Rehash server on next socket loop */
int dorestart = 0;		/**< Restart server on next socket loop */
int doreloadcert = 0;		/**< Reload TLS certificate on next socket loop */
MODVAR int quick_close = 0;	/**< Quickly close client connections (1) or be friendly to TLS users (0) */
#ifndef _WIN32
char **myargv;
#else
LPCSTR cmdLine;
#endif
