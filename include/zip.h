/************************************************************************
 *   IRC - Internet Relay Chat, include/s_zip.h
 *   Copyright (C) 1992 Darren Reed
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
 *
 * "s_zip.h". - Headers file.
 *
 * $Id$
 *
 */
#ifndef INCLUDED_s_zip_h
#define INCLUDED_s_zip_h
#include <zlib.h>         /* z_stream */
#endif

struct Client;

#ifdef ZIP_LINKS
/* the minimum amount of data needed to trigger compression */
#define ZIP_MINIMUM     4096

/* the maximum amount of data to be compressed (can actually be a bit more) */
#define ZIP_MAXIMUM     8192    /* WARNING: *DON'T* CHANGE THIS!!!! */

struct Zdata {
  z_stream*   in;            /* input zip stream data */
  z_stream*   out;           /* output zip stream data */
  char        inbuf[ZIP_MAXIMUM]; /* incoming zipped buffer */
  char        outbuf[ZIP_MAXIMUM]; /* outgoing (unzipped) buffer */
  int         incount;        /* size of inbuf content */
  int         outcount;       /* size of outbuf content */
};

#endif /* ZIP_LINKS */


extern int     zip_init (struct Client *);
extern void    zip_free (struct Client *);
extern char*   unzip_packet (struct Client *, char *, int *);
extern char*   zip_buffer (struct Client *, char *, int *, int);

#endif /* INCLUDED_s_zip_h */
