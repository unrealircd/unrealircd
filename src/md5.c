/*
 * Copyright UnrealIRCd 2015
 * A small wrapper around OpenSSL MD5 implementation
 */

#include "config.h"
#include <openssl/md5.h>
#include <stdio.h>

/** Generates an MD5 checksum.
 * @param mdout[out] Buffer to store result in, the result will be 16 bytes in binary
 *                   (not ascii printable!).
 * @param src[in]    The input data used to generate the checksum.
 * @param n[in]      Length of data.
 */
void DoMD5(unsigned char *mdout, const unsigned char *src, unsigned long n)
{
MD5_CTX hash;

	MD5_Init(&hash);
	MD5_Update(&hash, src, n);
	MD5_Final(mdout, &hash);
}

/** Generates an MD5 checksum - ASCII printable string (0011223344..etc..).
 * @param dst[out]  Buffer to store result in, this will be the result will be
 *                  32 characters + nul terminator, so needs to be at least 33 characters.
 * @param src[in]   The input data used to generate the checksum.
 * @param n[in]     Length of data.
 */
char *md5hash(unsigned char *dst, const unsigned char *src, unsigned long n)
{
unsigned char tmp[16];
	
	DoMD5(tmp, src, n);
	sprintf(dst, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7], tmp[8],
		tmp[9], tmp[10], tmp[11], tmp[12], tmp[13], tmp[14], tmp[15]);
	return dst;
}
