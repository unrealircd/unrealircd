/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/class.h
 *   Copyright (C) 1990 Darren Reed
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

#ifndef	__class_include__
#define __class_include__

typedef struct Class {
	int  class;
	int  conFreq;
	int  pingFreq;
	int  maxLinks;
	long maxSendq;
	int  links;
	struct Class *next;
} aClass;

#define	Class(x)	((x)->class)
#define	ConFreq(x)	((x)->conFreq)
#define	PingFreq(x)	((x)->pingFreq)
#define	MaxLinks(x)	((x)->maxLinks)
#define	MaxSendq(x)	((x)->maxSendq)
#define	Links(x)	((x)->links)

#define	ConfLinks(x)	(Class(x)->links)
#define	ConfMaxLinks(x)	(Class(x)->maxLinks)
#define	ConfClass(x)	(Class(x)->class)
#define	ConfConFreq(x)	(Class(x)->conFreq)
#define	ConfPingFreq(x)	(Class(x)->pingFreq)
#define	ConfSendq(x)	(Class(x)->maxSendq)

#define	FirstClass() 	classes
#define	NextClass(x)	((x)->next)

extern aClass *classes;

extern aClass *find_class(int);
extern int get_conf_class(aConfItem *);
extern int get_client_class(aClient *);
extern int get_client_ping(aClient *);
extern int get_con_freq(aClass *);
extern void add_class(int, int, int, int, long);
extern void check_class(void);
extern void initclass(void);

#endif /* __class_include__ */
