/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/auth.h
 *   Copyright (C) 2001 Carsten V. Munk (stskeeps@tspre.org)
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

typedef	struct {
	char	*data;
	short	type;
} anAuthStruct;

#define AUTHTYPE_PLAINTEXT  0
#define AUTHTYPE_UNIXCRYPT  1
#define AUTHTYPE_MD5        2
#define AUTHTYPE_SHA1	    3 
#define AUTHTYPE_SSL_PUBKEY 4
#define AUTHTYPE_RIPEMD160  5

#ifdef USE_SSL
#define AUTHENABLE_MD5
#define AUTHENABLE_SHA1
#define AUTHENABLE_SSL_PUBKEY
#define AUTHENABLE_RIPEMD160
/* OpenSSL provides a crypt() */
#ifndef AUTHENABLE_UNIXCRYPT
#define AUTHENABLE_UNIXCRYPT
#endif
#endif

	




