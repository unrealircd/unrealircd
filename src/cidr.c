/***********************************************************************
 *  Unreal Internet Relay Chat Daemon
 *
 *  All parts of this program are Copyright(C) 2001(or later).
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * $Id$ 
 */

#include <stdlib.h>
#include <string.h>
#include "struct.h"
#include "h.h"
#include "inet.h"
/* The following functions have been taken from Hybrid-7.0.1 simply because
 * I didn't feel like writing my own when they had ones that work just fine :)
 * However, several bugs were found and some stuff was moved around to work
 * better.
 */

#ifdef INET6
static int parse_v6_netmask(const char *, struct IN_ADDR *addr, short int *b);
#endif
static int parse_v4_netmask(const char *, struct IN_ADDR *addr, short int *b);

#define DigitParse(ch) do { \
                       if (ch >= '0' && ch <= '9') \
                         ch = ch - '0'; \
                       else if (ch >= 'A' && ch <= 'F') \
                         ch = ch - 'A' + 10; \
                       else if (ch >= 'a' && ch <= 'f') \
                         ch = ch - 'a' + 10; \
                       } while(0);

/* The mask parser/type determination code... */

/* int parse_v6_netmask(const char *, struct IN_ADDR*, short int *);
 * Input: An possible IPV6 address as a string.
 * Output: An integer describing whether it is an IPV6 or hostmask,
 *         an address(if it is IPV6), a bitlength(if it is IPV6).
 * Side effects: None
 * Comments: Called from parse_netmask
 */
/* Fixed so ::/0 (any IPv6 address) is valid
   Also a bug in DigitParse above.
   -Gozem 2002-07-19 gozem@linux.nu
 */
#ifdef INET6
static int parse_v6_netmask(const char *text, struct IN_ADDR *addr, short int *b)
{
	const char *p;
	char c;
	int d[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int dp = 0;
	int nyble = 4;
	int finsert = -1;
	short int bits = 128;
	int deficit = 0;
	unsigned short dc[8];

	for (p = text; (c = *p); p++)
		/* Parse a digit */
		if (isxdigit(c))
		{
			if (nyble == 0)
				return HM_HOST;
			DigitParse(c);
			d[dp] |= c << (4 * --nyble);
		}
		else if (c == ':')
		{
			/* It's a :: */
			if (p > text && *(p - 1) == ':')
			{
				if (finsert >= 0) /* Error: already has a :: */
					return HM_HOST;
				finsert = dp;
			}
			/* Just a regular : */
			else
			{
				/* If there were less than 4 hex digits, e.g. :ABC: shift right
				 * so we don't interpret it as ABC0 -A1kmm */
				d[dp] = d[dp] >> 4 * nyble;
				nyble = 4;
				if (++dp >= 8) /* Error: more than 8 segments */
					return HM_HOST;
			}
		}
		/* Wildcard */
		else if (c == '*')
		{
			/* Error: there was a ::, or it is not the last segment */
			if (finsert >= 0 || *(p + 1) || dp == 0 || *(p - 1) != ':')
				return HM_HOST;
			bits = dp * 16;
		}
		/* Bit section */
		else if (c == '/')
		{
			char *after;

			d[dp] = d[dp] >> 4 * nyble;
			dp++;
			bits = strtoul(p + 1, &after, 10);
			if (bits < 0 || *after) /* Error: bits is invalid or not the end */
				return HM_HOST;
			/* Error: Bits is greater than the number of bits given
			 * and there is no :: */
			if (bits > dp * 16 && !(finsert >= 0 && bits <= 128))
				return HM_HOST;
			break;
		}
		else /* Error: Illegal character */
			return HM_HOST;

	/* This is handled above if it was a / */
	if (c != '/')
		d[dp] = d[dp] >> 4 * nyble;
	if (c == 0)
		dp++;

	/* If there was no bit section, set the number of bits */
	if (finsert < 0 && bits == 0)
		bits = dp * 16;
	/* How many words are missing? -A1kmm */
	/* The original check was wrong -- codemastr */
	deficit = 8 - dp;

	/* Now fill in the gaps(from ::) in the copied table... -A1kmm */
	for (dp = 0, nyble = 0; dp < 8; dp++)
	{
		if (nyble == finsert && deficit)
		{
			dc[dp] = 0;
			deficit--;
		}
		else
			dc[dp] = d[nyble++];
	}
	/* Set unused bits to 0... -A1kmm */
	/* This check was wrong as well -- codemastr */
	if (bits < 128 && (bits % 16 != 0))
		dc[bits / 16] &= ~((1 << (16 - bits % 16)) - 1);
	for (dp = bits / 16 + (bits % 16 ? 1 : 0); dp < 8; dp++)
		dc[dp] = 0;
	/* And assign... -A1kmm */
	if (addr)
		for (dp = 0; dp < 8; dp++)
			/* The cast is a kludge to make netbsd work. */
			((unsigned short *)&addr->s6_addr)[dp] = htons(dc[dp]);
	if (b != NULL)
		*b = bits;
	return HM_IPV6;
}
#endif

/* int parse_v4_netmask(const char *, struct IN_ADDR *, short int *);
 * Input: A possible IPV4 address as a string.
 * Output: An integer describing whether it is an IPV4 or hostmask,
 *         an address(if it is IPV4), a bitlength(if it is IPV4).
 * Side effects: None
 * Comments: Called from parse_netmask
 */
static int parse_v4_netmask(const char *text, struct IN_ADDR *addr, short int *b)
{
	const char *p;
	const char *digits[4];
	unsigned char addb[4];
	int n = 0, bits = 0;
	char c;

	digits[n++] = text;

	for (p = text; (c = *p); p++)
		if (c >= '0' && c <= '9')   /* empty */
			;
	else if (c == '.')
	{
		if (n >= 4) /* Error: More than four sections */
			return HM_HOST;
		digits[n++] = p + 1;
	}
	else if (c == '*')
	{
		if (*(p + 1) || n == 0 || *(p - 1) != '.') /* Error: * is not at the end
							    * or not its own section */
			return HM_HOST;
		bits = (n - 1) * 8;
		break;
	}
	else if (c == '/')
	{
		char *after;
		bits = strtoul(p + 1, &after, 10);
		if (!bits || *after) /* Error: Invalid number or not end */
			return HM_HOST;
		if (bits > n * 8) /* Error: More than the bits given */
			return HM_HOST;
		break;
	}
	else /* Error: Illegal character */
		return HM_HOST;

	if (n < 4 && bits == 0)
		bits = n * 8;
	if (bits)
		while (n < 4)
			digits[n++] = "0";
	for (n = 0; n < 4; n++)
		addb[n] = strtoul(digits[n], NULL, 10);
	if (bits == 0)
		bits = 32;
	/* Set unused bits to 0... -A1kmm */
	if (bits < 32 && bits % 8)
		addb[bits / 8] &= ~((1 << (8 - bits % 8)) - 1);
	for (n = bits / 8 + (bits % 8 ? 1 : 0); n < 4; n++)
		addb[n] = 0;
	if (addr)
	{
#ifndef INET6
		addr->s_addr = htonl(addb[0] << 24 | addb[1] << 16 | addb[2] << 8 | addb[3]);
#else
		for (n = 0; n <= 9; n++)
			addr->s6_addr[n] = 0;
		addr->s6_addr[10] = 0xff;
		addr->s6_addr[11] = 0xff;
		addr->s6_addr[12] = addb[0];
		addr->s6_addr[13] = addb[1];
		addr->s6_addr[14] = addb[2];
		addr->s6_addr[15] = addb[3];
#endif
	}
	if (b)
		*b = bits;
	return HM_IPV4;
}

/* int parse_netmask(const char *, struct irc_netmask *);
 * Input: A hostmask, or an IPV4/6 address.
 * Output: An integer describing whether it is an IPV4, IPV6 address or a
 *         hostmask, an address(if it is an IP mask),
 *         a bitlength(if it is IP mask).
 * Side effects: None
 */
int parse_netmask(const char *text, struct irc_netmask *netmask)
{
	char *c;
	const char *host;

	/* So a user@ip can be specified -- codemastr */
	if ((c = strchr(text, '@')) && *(c+1))
		host = c+1;
	else
		host = text;
#ifdef INET6
	if (strchr(host, ':'))
		return parse_v6_netmask(host, &netmask->mask, &netmask->bits);
	else
#endif
	if (strchr(host, '.'))
		return parse_v4_netmask(host, &netmask->mask, &netmask->bits);
	else
	{
		/* Well, lets just try and see?
		 * This is here because ffff/10, for example,
		 * is valid for our purposes */
		if (parse_v4_netmask(host, &netmask->mask, &netmask->bits) == HM_IPV4)
			return HM_IPV4;
#ifdef INET6
		return parse_v6_netmask(host, &netmask->mask, &netmask->bits);
#endif
	}
	return HM_HOST;
}

/* The address matching stuff... */
/* int match_ipv6(struct IN_ADDR *, struct IN_ADDR *, int)
 * Input: An IP address, an IP mask, the number of bits in the mask.
 * Output: if match, 1 else 0
 * Side effects: None
 */
#ifdef INET6
int match_ipv6(struct IN_ADDR *addr, struct IN_ADDR *mask, int bits)
{
	int i, m, n = bits / 8;

	for (i = 0; i < n; i++)
		if (addr->s6_addr[i] != mask->s6_addr[i])
			return 0;
	if ((m = bits % 8) == 0)
		return 1;
	if ((addr->s6_addr[n] & ~((1 << (8 - m)) - 1)) == mask->s6_addr[n])
		return 1;
	return 0;
}
#endif

/* int match_ipv4(struct IN_ADDR *, struct IN_ADDR *, int)
 * Input: An IP address, an IP mask, the number of bits in the mask.
 * Output: if match, 1 else 0
 * Side Effects: None
 */
int match_ipv4(struct IN_ADDR *addr, struct IN_ADDR *mask, int bits)
{
#ifndef INET6
	if ((ntohl(addr->s_addr) & ~((1 << (32 - bits)) - 1)) == ntohl(mask->s_addr))
		return 0;
	return 1;
#else
	struct in_addr ipv4addr, ipv4mask;
	u_char *cp;
	cp = (u_char *)((struct IN_ADDR *)addr)->s6_addr;

	/* Make sure the address is IPv4 */
	if (cp[0] == 0 && cp[1] == 0 && cp[2] == 0 && cp[3] == 0 && cp[4] == 0
            && cp[5] == 0 && cp[6] == 0 && cp[7] == 0 && cp[8] == 0
            && cp[9] == 0 && cp[10] == 0xff && cp[11] == 0xff)
	{
		/* Convert the v6 representation to v4 */
		bcopy(&addr->s6_addr[12], &ipv4addr, sizeof(struct in_addr));
		bcopy(&mask->s6_addr[12], &ipv4mask, sizeof(struct in_addr));
		if ((ntohl(ipv4addr.s_addr) & ~((1 << (32 - bits)) - 1)) ==
		    ntohl(ipv4mask.s_addr))
			return 1;
	}
	return 0;
#endif
}

/* int match_ip(aClient *sptr, int type, char *mask, struct irc_netmask netmask)
 * Input: a Client, a netmask type, a string mask, and a netmask struct
 * Output: if match, 1 else 0
 * Side Effects: None
 */
#if 0
int match_ip(struct IN_ADDR addr, char *uhost, char *mask, struct irc_netmask *netmask)
{
	if (!netmask)
		return (!match(mask, uhost));

	switch (netmask->type)
	{
		case HM_HOST:
			return (!match(mask, uhost));
		case HM_IPV4:
			return match_ipv4(&addr, &netmask->mask, netmask->bits);
#ifdef INET6
		case HM_IPV6:
			return match_ipv6(&addr, &netmask->mask, netmask->bits);
#endif
		default:
			return 0;
	}
}
#endif
int match_ip(struct IN_ADDR addr, char *uhost, char *mask, struct irc_netmask *netmask)
{
	char *end;

	if (!netmask)
		return (!match(mask, uhost));

	/* If it is an IP mask, we need to extract the user portion of both 
         * and run a match. 
         */
	if (mask && (end = strchr(mask, '@')))
	{
		char username[USERLEN+1], usermask[USERLEN+1];
		strlcpy(usermask, mask, end-mask+1 > USERLEN+1 ? USERLEN+1 : end-mask+1);
		if ((end = strchr(uhost, '@')))
		{
			strlcpy(username, uhost, end-uhost+1 > USERLEN+1 ? USERLEN+1 : end-uhost+1);
			if (match(usermask, username))
				return 0;
		}
	}

	switch (netmask->type)
	{
		case HM_HOST:
			return (!match(mask, uhost));
		case HM_IPV4:
			return match_ipv4(&addr, &netmask->mask, netmask->bits);
#ifdef INET6
		case HM_IPV6:
			return match_ipv6(&addr, &netmask->mask, netmask->bits);
#endif
		default:
			return 0;
	}
}


