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
/* The following functions have been taken from Hybrid7-beta8 simply because
 * I didn't feel like writing my own when they had ones that work just fine :)
 */

#ifdef INET6
static int parse_v6_netmask(const char *, struct IN_ADDR *, int *);
#endif
static int parse_v4_netmask(const char *, struct IN_ADDR *, int *);

#define DigitParse(ch) if (ch >= '0' && ch <= '9') \
                         ch = ch - '0'; \
                       else if (ch >= 'A' && ch <= 'F') \
                         ch = ch - 'A' + '0'; \
                       else if (ch >= 'a' && ch <= 'f') \
                         ch = ch - 'a' + '0';

/* The mask parser/type determination code... */

/* int parse_v6_netmask(const char *, struct IN_ADDR*, int *);
 * Input: An possible IPV6 address as a string.
 * Output: An integer describing whether it is an IPV6 or hostmask,
 *         an address(if it is IPV6), a bitlength(if it is IPV6).
 * Side effects: None
 * Comments: Called from parse_netmask
 */
#ifdef INET6
static int
parse_v6_netmask(const char *text, struct IN_ADDR *addr, int *b)
{
  const char *p;
  char c;
  int d[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, dp = 0, nyble = 4, finsert =
    -1, bits = 0, deficit = 0;
  short dc[8];

  for (p = text; (c = *p); p++)
    if (isxdigit(c))
    {
      if (nyble == 0)
        return HM_HOST;
      DigitParse(c);
      d[dp] |= c << (4 * --nyble);
    }
    else if (c == ':')
    {
      if (p > text && *(p - 1) == ':')
      {
        if (finsert >= 0)
          return HM_HOST;
        finsert = dp;
      }
      else
      {
        /* If there were less than 4 hex digits, e.g. :ABC: shift right
         * so we don't interpret it as ABC0 -A1kmm */
        d[dp] = d[dp] >> 4 * nyble;
        nyble = 4;
        if (++dp >= 8)
          return HM_HOST;
      }
    }
    else if (c == '*')
    {
      /* * must be last, and * is ambiguous if there is a ::... -A1kmm */
      if (finsert >= 0 || *(p + 1) || dp == 0 || *(p - 1) != ':')
        return HM_HOST;
      bits = dp * 16;
    }
    else if (c == '/')
    {
      char *after;

      d[dp] = d[dp] >> 4 * nyble;
      dp++;
      if (p > text && *(p - 1) == ':')
        return HM_HOST;
      bits = strtoul(p + 1, &after, 10);
      if (bits == 0 || *after)
        return HM_HOST;
      if (bits > dp * 4 && !(finsert >= 0 && bits <= 128))
        return HM_HOST;
      break;
    }
    else
      return HM_HOST;

  d[dp] = d[dp] >> 4 * nyble;
  if (c == 0)
    dp++;
  if (finsert < 0 && bits == 0)
    bits = dp * 16;
  else if (bits == 0)
    bits = 128;
  /* How many words are missing? -A1kmm */
  deficit = bits / 16 + ((bits % 16) ? 1 : 0) - dp;
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
  if (bits < 128 && (bits % 16 != 0))
    dc[bits / 16] &= ~((1 << (15 - bits % 16)) - 1);
  for (dp = bits / 16 + (bits % 16 ? 1 : 0); dp < 8; dp++)
    dc[dp] = 0;
  /* And assign... -A1kmm */
  if (addr)
    for (dp = 0; dp < 8; dp++)
      /* The cast is a kludge to make netbsd work. */
      ((unsigned short *)&addr->s6_addr)[dp] = htons(dc[dp]);
  if (b)
    *b = bits;
  return HM_IPV6;
}
#endif

/* int parse_v4_netmask(const char *, struct IN_ADDR *, int *);
 * Input: An possible IPV4 address as a string.
 * Output: An integer describing whether it is an IPV4 or hostmask,
 *         an address(if it is IPV4), a bitlength(if it is IPV4).
 * Side effects: None
 * Comments: Called from parse_netmask
 */
static int
parse_v4_netmask(const char *text, struct IN_ADDR *addr, int *b)
{
#ifndef INET6
  const char *p;
  const char *digits[4];
  unsigned char addb[8]; /* will only use 4, but space for overflow [?]. -- Syzop*/
  int n = 0, bits = 0;
  char c;

  digits[n++] = text;

  for (p = text; (c = *p); p++)
    if (c >= '0' && c <= '9')   /* empty */
      ;
    else if (c == '.')
    {
      if (n >= 4)
        return HM_HOST;
      digits[n++] = p + 1;
    }
    else if (c == '*')
    {
      if (*(p + 1) || n == 0 || *(p - 1) != '.')
        return HM_HOST;
      bits = (n - 1) * 8;
      break;
    }
    else if (c == '/')
    {
      char *after;
      bits = strtoul(p + 1, &after, 10);
      if (!bits || *after)
        return HM_HOST;
      if (bits > n * 8)
        return HM_HOST;
      break;
    }
    else
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
  for (n = bits / 8 + (bits % 8 ? 1 : 0); n < 8; n++)
    addb[n] = 0;
  if (addr)
    addr->S_ADDR =
      htonl(addb[0] << 24 | addb[1] << 16 | addb[2] << 8 | addb[3]);
  if (b)
    *b = bits;
  return HM_IPV4;
#else
  u_char *cp;
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
      if (n >= 4)
        return HM_HOST;
      digits[n++] = p + 1;
    }
    else if (c == '*')
    {
      if (*(p + 1) || n == 0 || *(p - 1) != '.')
        return HM_HOST;
      bits = (n - 1) * 8;
      break;
    }
    else if (c == '/')
    {
      char *after;
      bits = strtoul(p + 1, &after, 10);
      if (!bits || *after)
        return HM_HOST;
      if (bits > n * 8)
        return HM_HOST;
      break;
    }
    else
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
  for (n = bits / 8 + (bits % 8 ? 1 : 0); n < 8; n++)
    addb[n] = 0;
  if (addr)
  {  
    cp = (u_char *)addr->s6_addr;
    for (n = 0; n <= 9; n++)
    	cp[n] = 0;
    cp[10] = 0xff;
    cp[11] = 0xff;
    cp[12] = addb[0];
    cp[13] = addb[1];
    cp[14] = addb[2];
    cp[15] = addb[3];
  }
  if (b)
    *b = bits;
  return HM_IPV4;
  
#endif
}

/* int parse_netmask(const char *, struct IN_ADDR *, int *);
 * Input: A hostmask, or an IPV4/6 address.
 * Output: An integer describing whether it is an IPV4, IPV6 address or a
 *         hostmask, an address(if it is an IP mask),
 *         a bitlength(if it is IP mask).
 * Side effects: None
 */
int
parse_netmask(const char *text, struct IN_ADDR *addr, int *b)
{
#ifdef INET6
  if (strchr(text, ':'))
    return parse_v6_netmask(text, addr, b);
#endif
  if (strchr(text, '.'))
    return parse_v4_netmask(text, addr, b);
  return HM_HOST;
}

/* The address matching stuff... */
/* int match_ipv6(struct IN_ADDR *, struct IN_ADDR *, int)
 * Input: An IP address, an IP mask, the number of bits in the mask.
 * Output: if match, 0 else 1
 * Side effects: None
 */
#ifdef INET6
int
match_ipv6(struct IN_ADDR *addr, struct IN_ADDR *mask, int bits)
{
  int i, m, n = bits / 8;

  for (i = 0; i < n; i++)
    if (addr->S_ADDR[i] != mask->S_ADDR[i])
      return 1;
  if ((m = bits % 8) == 0)
    return 0;
  if ((addr->S_ADDR[n] & ~((1 << (8 - m)) - 1)) ==
      mask->S_ADDR[n])
    return 0;
  return 1;
}
#endif

/* int match_ipv4(struct IN_ADDR *, struct IN_ADDR *, int)
 * Input: An IP address, an IP mask, the number of bits in the mask.
 * Output: if match, 0 else 1
 * Side Effects: None
 */
int
match_ipv4(struct IN_ADDR *addr, struct IN_ADDR *mask, int bits)
{
#ifndef INET6
  if ((ntohl(addr->S_ADDR) & ~((1 << (32 - bits)) - 1)) !=
      ntohl(mask->S_ADDR))

    return 1;
  return 0;
#else
  struct in_addr ipv4addr, ipv4mask;
  
  ipv4addr.s_addr = inet_addr((char *)Inet_ia2p(addr));
  ipv4mask.s_addr = inet_addr((char *)Inet_ia2p(mask));
  if ((ntohl(ipv4addr.s_addr) & ~((1 << (32 - bits)) - 1)) !=
      ntohl(ipv4mask.s_addr))

    return 1;
  return 0;
  
#endif
}



