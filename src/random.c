/************************************************************************
 *   IRC - Internet Relay Chat, random.c
 *   (C) 2004-2010 Bram Matthys (Syzop) and the UnrealIRCd Team
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

/*
 * Based on Arc4 random number generator for FreeBSD/OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project (for instance by leaving this copyright notice
 * intact).
 *
 * This code is derived from section 17.1 of Applied Cryptography, second edition.
 *
 * *BSD code modified by Syzop to suit our needs (unreal'ized, windows, etc)
 */

struct arc4_stream {
	u_char i;
	u_char j;
	u_char s[256];
};

static struct arc4_stream rs;

static void arc4_init(void)
{
int n;

	for (n = 0; n < 256; n++)
		rs.s[n] = n;
	rs.i = 0;
	rs.j = 0;
}

static inline void arc4_doaddrandom(u_char *dat, int datlen)
{
int n;
u_char si;
#ifdef DEBUGMODE
int i;
char outbuf[512], *p = outbuf;
	*p = '\0';
	for (i=0; i < datlen; i++)
	{
		sprintf(p, "%.2X/", dat[i]);
		p += 3;
		if (p > outbuf + 500)
		{
			strcpy(p, "....");
			break;
		}
	}
	if (strlen(outbuf) > 0)
		outbuf[strlen(outbuf)-1] = '\0';
	Debug((DEBUG_DEBUG, "arc4_addrandom() called, datlen=%d, data dump: %s", datlen, outbuf));
#endif

	rs.i--;
	for (n = 0; n < 256; n++) {
		rs.i = (rs.i + 1);
		si = rs.s[rs.i];
		rs.j = (rs.j + si + dat[n % datlen]);
		rs.s[rs.i] = rs.s[rs.j];
		rs.s[rs.j] = si;
	}
}

static inline void arc4_addrandom(void *dat, int datlen)
{
	arc4_doaddrandom((unsigned char *)dat, datlen);
	return;
}


u_char getrandom8()
{
u_char si, sj;

	rs.i = (rs.i + 1);
	si = rs.s[rs.i];
	rs.j = (rs.j + si);
	sj = rs.s[rs.j];
	rs.s[rs.i] = sj;
	rs.s[rs.j] = si;
	return (rs.s[(si + sj) & 0xff]);
}

u_int16_t getrandom16()
{
u_int16_t val;
	val = getrandom8() << 8;
	val |= getrandom8();
	return val;
}

u_int32_t getrandom32()
{
u_int32_t val;

	val = getrandom8() << 24;
	val |= getrandom8() << 16;
	val |= getrandom8() << 8;
	val |= getrandom8();
	return val;
}

void add_entropy_configfile(struct stat *st, char *buf)
{
unsigned char mdbuf[16];

	arc4_addrandom(&st->st_size, sizeof(st->st_size));
	arc4_addrandom(&st->st_mtime, sizeof(st->st_mtime));
	DoMD5(mdbuf, buf, strlen(buf));
	arc4_addrandom(&mdbuf, sizeof(mdbuf));
}

/*
 * init_random, written by Syzop.
 * This function tries to initialize the arc4 random number generator securely.
 */
void init_random()
{
struct {
	char egd[32];			/* from EGD */
#ifndef _WIN32
	struct timeval nowt;	/* time */
	char rnd[32];			/* /dev/urandom */
#else
	MEMORYSTATUS mstat;		/* memory status */
	struct _timeb nowt;		/* time */
#endif
} rdat;

#ifndef _WIN32
int fd;
#else
MEMORYSTATUS mstat;
#endif

	arc4_init();

	/* Grab non-OS specific "random" data */
#if OPENSSL_VERSION_NUMBER >= 0x000907000 && defined(HAVE_RAND_EGD)
	if (EGD_PATH) {
		RAND_query_egd_bytes(EGD_PATH, rdat.egd, sizeof(rdat.egd));
	}
#endif

	/* Grab OS specific "random" data */
#ifndef _WIN32
	gettimeofday(&rdat.nowt, NULL);
	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0)
	{
		int n = read(fd, &rdat.rnd, sizeof(rdat.rnd));
		Debug((DEBUG_INFO, "init_random: read from /dev/urandom returned %d", n));
		close(fd);
	}
	/* TODO: more!?? */
#else
	_ftime(&rdat.nowt);
	GlobalMemoryStatus (&rdat.mstat);
#endif	

	arc4_addrandom(&rdat, sizeof(rdat));

	/* NOTE: addtional entropy is added by add_entropy_* function(s) */
}
