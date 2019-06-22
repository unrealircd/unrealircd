/*
 * Unreal Internet Relay Chat Daemon, src/s_kline.c
 * Small non-modulized stuff for the TKL layer
 * (C) Copyright 1999-2005 The UnrealIRCd Team
 *
 * See file AUTHORS in IRC package for additional names of
 * the programmers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

// FIXME: with so little here, why have a file for it? Move it!

MODVAR aTKline *tklines[TKLISTLEN];
MODVAR aTKline *tklines_ip_hash[TKLIPHASHLEN1][TKLIPHASHLEN2];
int MODVAR spamf_ugly_vchanoverride = 0;

void tkl_init(void)
{
	memset(tklines, 0, sizeof(tklines));
	memset(tklines_ip_hash, 0, sizeof(tklines_ip_hash));
}
