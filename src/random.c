/************************************************************************
 *   IRC - Internet Relay Chat, random.c
 *   (C) 2003 Bram Matthys (Syzop) <syz@dds.nl>
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
 */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

#ifdef NOSPOOF
/* 
 * getrandom32, written by Syzop.
 * This function returns a random 32bit value
 */
u_int32_t getrandom32()
{
u_int32_t result;
#ifdef USE_SSL
int n;
#endif
#ifndef _WIN32
static struct timeval prevt;
struct timeval nowt;
#else
static struct _timeb prevt;
struct _timeb nowt;
#endif
#ifdef USE_SSL
 #if OPENSSL_VERSION_NUMBER >= 0x000907000
	if (EGD_PATH) {
		n = RAND_query_egd_bytes(EGD_PATH, (unsigned char *)&result, sizeof(result));
		if (n == sizeof(result))
			return result;
	}
 #endif
#endif

#ifndef _WIN32
	gettimeofday(&nowt, NULL);

	/* [random() may return <31 random bits, I assume it will be at least 16 bits]
	 * 31bits (low) ^ 16bits (high) ^ 19bits (low) ^ 19bits (high) */
	result = random() ^ (random() << 16) ^ nowt.tv_usec ^ (prevt.tv_usec << 13);
#else
	_ftime(&nowt);

	/* 15bits (0..14) ^ 15 bits (10..24) ^ 12 bits (20..31) ^
	 * 10bits (10..19) ^ 10 bits (20..29)
	 */
	result = rand() ^ (rand() << 10) ^ (rand() << 20) ^
	         (nowt.millitm << 10) ^ (prevt.millitm << 20);
#endif

	prevt = nowt;
	return result;
}
#endif

static unsigned int entropy_cfgcrc = 0, entropy_cfgsize = 0;
static time_t entropy_cfgmtime = 0;

void add_entropy_configfile(struct stat st, char *buf)
{
	entropy_cfgsize = (entropy_cfgsize << 4) ^ st.st_size;
	entropy_cfgmtime = (entropy_cfgmtime << 4) ^ st.st_mtime;
	entropy_cfgcrc = entropy_cfgcrc ^ (unsigned int)our_crc32(buf, strlen(buf));
	Debug((DEBUG_INFO, "add_entropy_configfile: cfgsize: %u", entropy_cfgsize));
	Debug((DEBUG_INFO, "add_entropy_configfile: cfgmtime: %u",
		(unsigned int)entropy_cfgmtime));
	Debug((DEBUG_INFO, "add_entropy_configfile: cfgcrc: %u", entropy_cfgcrc));
}

/*
 * init_random, written by Syzop
 * This function (hopefully) intialises the random generator securely
 */
void init_random()
{
unsigned int seed, egd = 0;
time_t now = TStime();
#ifdef USE_SSL
int n;
#endif
#ifndef _WIN32
struct timeval nowt;
unsigned int xrnd = 0;
int fd;
#else
MEMORYSTATUS mstat;
struct _timeb nowt;
#endif

#ifdef USE_SSL
 #if OPENSSL_VERSION_NUMBER >= 0x000907000
	if (EGD_PATH) {
		n = RAND_query_egd_bytes(EGD_PATH, (unsigned char *)&egd, sizeof(egd));
		Debug((DEBUG_INFO, 
			"init_random: RAND_query_egd_bytes() ret=%d, val=%.8x", n, egd));
	}
 #endif
#endif

	/* Grab non-OS specific "random" data */
	
	/* Grab OS specific "random" data */
#ifndef _WIN32
	gettimeofday(&nowt, NULL);
	fd = open("/dev/urandom", O_RDONLY);
	if (fd) {
		(void)read(fd, &xrnd, sizeof(int));
		Debug((DEBUG_INFO, "init_random: read from /dev/urandom: 0x%.8x", xrnd));
		close(fd);
	}
#else
	_ftime(&nowt);
	GlobalMemoryStatus (&mstat);
 #ifdef DEBUGMODE
    Debug((DEBUG_INFO, "init_random: mstat.dwAvailPhys=%u, mstat.dwAvailPageFile=%u\n",
    	mstat.dwAvailPhys, mstat.dwAvailPageFile));
 #endif
#endif	

	/* Build the seed (OS specific again) */
#ifndef _WIN32
	seed = now ^ nowt.tv_usec ^ getpid() ^ (entropy_cfgsize << 24) ^ 
	       (entropy_cfgmtime << 16) ^ entropy_cfgcrc ^ CLOAK_KEY1 ^ xrnd ^ egd;
#else
	seed = now ^ nowt.millitm ^ getpid() ^ mstat.dwAvailPhys ^ 
	       (mstat.dwAvailPageFile << 16) ^ (entropy_cfgsize << 8) ^
	       (entropy_cfgmtime << 24) ^ entropy_cfgcrc ^ CLOAK_KEY1 ^ egd;
#endif	

	Debug((DEBUG_INFO, "init_random: seeding to 0x%.8x (%u)", seed, seed));
	srand(seed);
#ifdef LRAND48
	srand48(seed);
#endif
#ifndef _WIN32
	srandom(seed);
#endif
}

