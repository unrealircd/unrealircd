/************************************************************************
 *   IRC - Internet Relay Chat, src/random.c
 *   (C) 2004-2019 Bram Matthys (Syzop) and the UnrealIRCd Team
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

#include "unrealircd.h"

#ifndef HAVE_ARC4RANDOM

#define KEYSTREAM_ONLY

/* chacha_private.h from openssh/openssh-portable
 * "chacha-merged.c version 20080118
 *  D. J. Bernstein
 *  Public domain."
 */

/* $OpenBSD: chacha_private.h,v 1.2 2013/10/04 07:02:27 djm Exp $ */

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct
{
  u32 input[16]; /* could be compressed */
} chacha_ctx;

#define U8C(v) (v##U)
#define U32C(v) (v##U)

#define U8V(v) ((u8)(v) & U8C(0xFF))
#define U32V(v) ((u32)(v) & U32C(0xFFFFFFFF))

#define ROTL32(v, n) \
  (U32V((v) << (n)) | ((v) >> (32 - (n))))

#define U8TO32_LITTLE(p) \
  (((u32)((p)[0])      ) | \
   ((u32)((p)[1]) <<  8) | \
   ((u32)((p)[2]) << 16) | \
   ((u32)((p)[3]) << 24))

#define U32TO8_LITTLE(p, v) \
  do { \
    (p)[0] = U8V((v)      ); \
    (p)[1] = U8V((v) >>  8); \
    (p)[2] = U8V((v) >> 16); \
    (p)[3] = U8V((v) >> 24); \
  } while (0)

#define ROTATE(v,c) (ROTL32(v,c))
#define XOR(v,w) ((v) ^ (w))
#define PLUS(v,w) (U32V((v) + (w)))
#define PLUSONE(v) (PLUS((v),1))

#define QUARTERROUND(a,b,c,d) \
  a = PLUS(a,b); d = ROTATE(XOR(d,a),16); \
  c = PLUS(c,d); b = ROTATE(XOR(b,c),12); \
  a = PLUS(a,b); d = ROTATE(XOR(d,a), 8); \
  c = PLUS(c,d); b = ROTATE(XOR(b,c), 7);

static const char sigma[16] = "expand 32-byte k";
static const char tau[16] = "expand 16-byte k";

static void
chacha_keysetup(chacha_ctx *x,const u8 *k,u32 kbits,u32 ivbits)
{
  const char *constants;

  x->input[4] = U8TO32_LITTLE(k + 0);
  x->input[5] = U8TO32_LITTLE(k + 4);
  x->input[6] = U8TO32_LITTLE(k + 8);
  x->input[7] = U8TO32_LITTLE(k + 12);
  if (kbits == 256) { /* recommended */
    k += 16;
    constants = sigma;
  } else { /* kbits == 128 */
    constants = tau;
  }
  x->input[8] = U8TO32_LITTLE(k + 0);
  x->input[9] = U8TO32_LITTLE(k + 4);
  x->input[10] = U8TO32_LITTLE(k + 8);
  x->input[11] = U8TO32_LITTLE(k + 12);
  x->input[0] = U8TO32_LITTLE(constants + 0);
  x->input[1] = U8TO32_LITTLE(constants + 4);
  x->input[2] = U8TO32_LITTLE(constants + 8);
  x->input[3] = U8TO32_LITTLE(constants + 12);
}

static void
chacha_ivsetup(chacha_ctx *x,const u8 *iv)
{
  x->input[12] = 0;
  x->input[13] = 0;
  x->input[14] = U8TO32_LITTLE(iv + 0);
  x->input[15] = U8TO32_LITTLE(iv + 4);
}

static void
chacha_encrypt_bytes(chacha_ctx *x,const u8 *m,u8 *c,u32 bytes)
{
  u32 x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  u32 j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15;
  u8 *ctarget = NULL;
  u8 tmp[64];
  u_int i;

  if (!bytes) return;

  j0 = x->input[0];
  j1 = x->input[1];
  j2 = x->input[2];
  j3 = x->input[3];
  j4 = x->input[4];
  j5 = x->input[5];
  j6 = x->input[6];
  j7 = x->input[7];
  j8 = x->input[8];
  j9 = x->input[9];
  j10 = x->input[10];
  j11 = x->input[11];
  j12 = x->input[12];
  j13 = x->input[13];
  j14 = x->input[14];
  j15 = x->input[15];

  for (;;) {
    if (bytes < 64) {
      for (i = 0;i < bytes;++i) tmp[i] = m[i];
      m = tmp;
      ctarget = c;
      c = tmp;
    }
    x0 = j0;
    x1 = j1;
    x2 = j2;
    x3 = j3;
    x4 = j4;
    x5 = j5;
    x6 = j6;
    x7 = j7;
    x8 = j8;
    x9 = j9;
    x10 = j10;
    x11 = j11;
    x12 = j12;
    x13 = j13;
    x14 = j14;
    x15 = j15;
    for (i = 20;i > 0;i -= 2) {
      QUARTERROUND( x0, x4, x8,x12)
      QUARTERROUND( x1, x5, x9,x13)
      QUARTERROUND( x2, x6,x10,x14)
      QUARTERROUND( x3, x7,x11,x15)
      QUARTERROUND( x0, x5,x10,x15)
      QUARTERROUND( x1, x6,x11,x12)
      QUARTERROUND( x2, x7, x8,x13)
      QUARTERROUND( x3, x4, x9,x14)
    }
    x0 = PLUS(x0,j0);
    x1 = PLUS(x1,j1);
    x2 = PLUS(x2,j2);
    x3 = PLUS(x3,j3);
    x4 = PLUS(x4,j4);
    x5 = PLUS(x5,j5);
    x6 = PLUS(x6,j6);
    x7 = PLUS(x7,j7);
    x8 = PLUS(x8,j8);
    x9 = PLUS(x9,j9);
    x10 = PLUS(x10,j10);
    x11 = PLUS(x11,j11);
    x12 = PLUS(x12,j12);
    x13 = PLUS(x13,j13);
    x14 = PLUS(x14,j14);
    x15 = PLUS(x15,j15);

#ifndef KEYSTREAM_ONLY
    x0 = XOR(x0,U8TO32_LITTLE(m + 0));
    x1 = XOR(x1,U8TO32_LITTLE(m + 4));
    x2 = XOR(x2,U8TO32_LITTLE(m + 8));
    x3 = XOR(x3,U8TO32_LITTLE(m + 12));
    x4 = XOR(x4,U8TO32_LITTLE(m + 16));
    x5 = XOR(x5,U8TO32_LITTLE(m + 20));
    x6 = XOR(x6,U8TO32_LITTLE(m + 24));
    x7 = XOR(x7,U8TO32_LITTLE(m + 28));
    x8 = XOR(x8,U8TO32_LITTLE(m + 32));
    x9 = XOR(x9,U8TO32_LITTLE(m + 36));
    x10 = XOR(x10,U8TO32_LITTLE(m + 40));
    x11 = XOR(x11,U8TO32_LITTLE(m + 44));
    x12 = XOR(x12,U8TO32_LITTLE(m + 48));
    x13 = XOR(x13,U8TO32_LITTLE(m + 52));
    x14 = XOR(x14,U8TO32_LITTLE(m + 56));
    x15 = XOR(x15,U8TO32_LITTLE(m + 60));
#endif

    j12 = PLUSONE(j12);
    if (!j12) {
      j13 = PLUSONE(j13);
      /* stopping at 2^70 bytes per nonce is user's responsibility */
    }

    U32TO8_LITTLE(c + 0,x0);
    U32TO8_LITTLE(c + 4,x1);
    U32TO8_LITTLE(c + 8,x2);
    U32TO8_LITTLE(c + 12,x3);
    U32TO8_LITTLE(c + 16,x4);
    U32TO8_LITTLE(c + 20,x5);
    U32TO8_LITTLE(c + 24,x6);
    U32TO8_LITTLE(c + 28,x7);
    U32TO8_LITTLE(c + 32,x8);
    U32TO8_LITTLE(c + 36,x9);
    U32TO8_LITTLE(c + 40,x10);
    U32TO8_LITTLE(c + 44,x11);
    U32TO8_LITTLE(c + 48,x12);
    U32TO8_LITTLE(c + 52,x13);
    U32TO8_LITTLE(c + 56,x14);
    U32TO8_LITTLE(c + 60,x15);

    if (bytes <= 64) {
      if (bytes < 64) {
        for (i = 0;i < bytes;++i) ctarget[i] = c[i];
      }
      x->input[12] = j12;
      x->input[13] = j13;
      return;
    }
    bytes -= 64;
    c += 64;
#ifndef KEYSTREAM_ONLY
    m += 64;
#endif
  }
}

/* The following is taken from arc4random.c
 * from openssh/openssh-portable/, which has an ISC license,
 * which is GPL compatible.
 */

/* OPENBSD ORIGINAL: lib/libc/crypto/arc4random.c */
/* $OpenBSD: arc4random.c,v 1.25 2013/10/01 18:34:57 markus Exp $	*/
/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Modified for UnrealIRCd by Bram Matthys ("Syzop") in 2019.
 * Things like taking out #if (n)def's for openssl (which we always
 * compile with), re-indenting, removing various stuff, etc.
 */



#define KEYSZ	32
#define IVSZ	8
#define BLOCKSZ	64
#define RSBUFSZ	(16*BLOCKSZ)
static int rs_initialized;
static chacha_ctx rs;		/* chacha context for random keystream */
static u_char rs_buf[RSBUFSZ];	/* keystream blocks */
static size_t rs_have;		/* valid bytes at end of rs_buf */
static size_t rs_count;		/* bytes till reseed */

static inline void _rs_rekey(u_char *dat, size_t datlen);

static inline void _rs_init(u_char *buf, size_t n)
{
	if (n < KEYSZ + IVSZ)
		return;
	chacha_keysetup(&rs, buf, KEYSZ * 8, 0);
	chacha_ivsetup(&rs, buf + KEYSZ);
}

static void _rs_stir(void)
{
	u_char rnd[KEYSZ + IVSZ];

	if (RAND_bytes(rnd, sizeof(rnd)) <= 0)
	{
		unreal_log(ULOG_FATAL, "random", "RANDOM_OUT_OF_BYTES", NULL,
		           "Could not obtain random bytes, error $tls_error_code",
		           log_data_integer("tls_error_code", ERR_get_error()));
		abort();
	}

	if (!rs_initialized) {
		rs_initialized = 1;
		_rs_init(rnd, sizeof(rnd));
	} else
		_rs_rekey(rnd, sizeof(rnd));
#ifdef HAVE_EXPLICIT_BZERO
	explicit_bzero(rnd, sizeof(rnd));
#else
	/* Not terribly important in this case */
	memset(rnd, 0, sizeof(rnd));
#endif
	/* invalidate rs_buf */
	rs_have = 0;
	memset(rs_buf, 0, RSBUFSZ);

	rs_count = 1600000;
}

static inline void _rs_stir_if_needed(size_t len)
{
	if (rs_count <= len || !rs_initialized) {
		_rs_stir();
	} else
		rs_count -= len;
}

static inline void _rs_rekey(u_char *dat, size_t datlen)
{
#ifndef KEYSTREAM_ONLY
	memset(rs_buf, 0,RSBUFSZ);
#endif
	/* fill rs_buf with the keystream */
	chacha_encrypt_bytes(&rs, rs_buf, rs_buf, RSBUFSZ);
	/* mix in optional user provided data */
	if (dat) {
		size_t i, m;

		m = MIN(datlen, KEYSZ + IVSZ);
		for (i = 0; i < m; i++)
			rs_buf[i] ^= dat[i];
	}
	/* immediately reinit for backtracking resistance */
	_rs_init(rs_buf, KEYSZ + IVSZ);
	memset(rs_buf, 0, KEYSZ + IVSZ);
	rs_have = RSBUFSZ - KEYSZ - IVSZ;
}

static inline void _rs_random_buf(void *_buf, size_t n)
{
	u_char *buf = (u_char *)_buf;
	size_t m;

	_rs_stir_if_needed(n);
	while (n > 0) {
		if (rs_have > 0) {
			m = MIN(n, rs_have);
			memcpy(buf, rs_buf + RSBUFSZ - rs_have, m);
			memset(rs_buf + RSBUFSZ - rs_have, 0, m);
			buf += m;
			n -= m;
			rs_have -= m;
		}
		if (rs_have == 0)
			_rs_rekey(NULL, 0);
	}
}

static inline void _rs_random_u32(uint32_t *val)
{
	_rs_stir_if_needed(sizeof(*val));
	if (rs_have < sizeof(*val))
		_rs_rekey(NULL, 0);
	memcpy(val, rs_buf + RSBUFSZ - rs_have, sizeof(*val));
	memset(rs_buf + RSBUFSZ - rs_have, 0, sizeof(*val));
	rs_have -= sizeof(*val);
	return;
}

void arc4random_stir(void)
{
	_rs_stir();
}

void arc4random_doaddrandom(u_char *dat, int datlen)
{
	int m;

	if (!rs_initialized)
		_rs_stir();
	while (datlen > 0) {
		m = MIN(datlen, KEYSZ + IVSZ);
		_rs_rekey(dat, m);
		dat += m;
		datlen -= m;
	}
}

#endif /* !HAVE_ARC4RANDOM */

/* UnrealIRCd-specific functions follow */

static void arc4_addrandom(void *dat, int datlen)
{
	arc4random_doaddrandom((unsigned char *)dat, datlen);
	return;
}

void add_entropy_configfile(struct stat *st, char *buf)
{
	char sha256buf[SHA256_DIGEST_LENGTH];

	arc4_addrandom(&st->st_size, sizeof(st->st_size));
	arc4_addrandom(&st->st_mtime, sizeof(st->st_mtime));
	sha256hash_binary(sha256buf, buf, strlen(buf));
	arc4_addrandom(sha256buf, sizeof(sha256buf));
}

/*
 * init_random, written by Syzop.
 * This function tries to initialize the arc4 random number generator securely.
 */
void init_random()
{
	struct {
#ifndef _WIN32
		struct timeval nowt;		/* time */
		char rnd[32];			/* /dev/urandom */
#else
		struct _timeb nowt;		/* time */
		MEMORYSTATUS mstat;		/* memory status */
#endif
	} rdat;

#ifndef _WIN32
	int fd;
#endif

	_rs_stir();

	/* Grab OS specific "random" data */
#ifndef _WIN32
	gettimeofday(&rdat.nowt, NULL);
	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0)
	{
		int n = read(fd, &rdat.rnd, sizeof(rdat.rnd));
		close(fd);
	}
#else
	_ftime(&rdat.nowt);
	GlobalMemoryStatus (&rdat.mstat);
#endif	

	arc4_addrandom(&rdat, sizeof(rdat));

	/* NOTE: addtional entropy is added by add_entropy_* function(s) */
}

uint32_t arc4random(void)
{
	uint32_t val;

	_rs_random_u32(&val);
	return val;
}

/** Get 8 bits (1 byte) of randomness */
u_char getrandom8()
{
	return arc4random() & 0xff;
}

/** Get 16 bits (2 bytes) of randomness */
uint16_t getrandom16()
{
	return arc4random() & 0xffff;
}

/** Get 32 bits (4 bytes) of randomness */
uint32_t getrandom32()
{
	return arc4random();
}

/** Generate an alphanumeric string (eg: XzHe5G).
 * @param buf      The buffer
 * @param numbytes The number of random bytes.
 * @note  Note that numbytes+1 bytes are written to buf.
 *        In other words, numbytes does NOT include the NUL byte.
 */
void gen_random_alnum(char *buf, int numbytes)
{
	for (; numbytes > 0; numbytes--)
	{
		unsigned char v = getrandom8() % (26+26+10);
		if (v < 26)
			*buf++ = 'a'+v;
		else if (v < 52)
			*buf++ = 'A'+(v-26);
		else
			*buf++ = '0'+(v-52);
	}
	*buf = '\0';
}
