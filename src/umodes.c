/************************************************************************
 *   IRC - Internet Relay Chat, s_unreal.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

extern char umodestring[UMODETABLESZ+1];

aUMtable *Usermode_Table = NULL;
short	 Usermode_highest = 0;

long UMODE_INVISIBLE = 0L; /*  0x0001	 makes user invisible */
long UMODE_OPER = 0L;      /*  0x0002	 Operator */
long UMODE_WALLOP = 0L;    /*  0x0004	 send wallops to them */
long UMODE_FAILOP = 0L;    /*  0x0008	 Shows some global messages */
long UMODE_HELPOP = 0L;    /*  0x0010	 Help system operator */
long UMODE_REGNICK = 0L;   /*  0x0020	 Nick set by services as registered */
long UMODE_SADMIN = 0L;    /*  0x0040	 Services Admin */
long UMODE_ADMIN = 0L;     /*  0x0080	 Admin */
long UMODE_SERVNOTICE = 0L;/* 0x0100	 server notices such as kill */
long UMODE_LOCOP = 0L;     /* 0x0200	 Local operator -- SRB */
long UMODE_RGSTRONLY = 0L; /* 0x0400  Only reg nick message */
long UMODE_WEBTV = 0L;     /* 0x0800  WebTV Client */
long UMODE_SERVICES = 0L;  /* 0x4000	 services */
long UMODE_HIDE = 0L;	     /* 0x8000	 Hide from Nukes */
long UMODE_NETADMIN = 0L;  /* 0x10000	 Network Admin */
long UMODE_COADMIN = 0L;   /* 0x80000	 Co Admin */
long UMODE_WHOIS = 0L;     /* 0x100000	 gets notice on /whois */
long UMODE_KIX = 0L;       /* 0x200000	 usermode +q */
long UMODE_BOT = 0L;       /* 0x400000	 User is a bot */
long UMODE_SECURE = 0L;    /*	0x800000	 User is a secure connect */
long UMODE_HIDING = 0L;    /* 0x2000000	 Totally invisible .. */
long UMODE_VICTIM = 0L;    /* 0x8000000	 Intentional Victim */
long UMODE_DEAF = 0L;      /* 0x10000000       Deaf */
long UMODE_HIDEOPER = 0L;  /* 0x20000000	 Hide oper mode */
long UMODE_SETHOST = 0L;   /* 0x40000000	 used sethost */
long UMODE_STRIPBADWORDS = 0L; /* 0x80000000	 */



void	umode_init(void)
{
	long val = 1;
	int	i;
	Usermode_Table = (aUMtable *)MyMalloc(sizeof(aUMtable) * UMODETABLESZ);
	bzero(Usermode_Table, sizeof(aUMtable) * UMODETABLESZ);
	for (i = 0; i < UMODETABLESZ; i++)
	{
		Usermode_Table[i].mode = val;
		val *= 2;
	}
	Usermode_highest = 0;
	/* Set up modes */
	UMODE_INVISIBLE = umode_get('i'); /*  0x0001	/* makes user invisible */
	UMODE_OPER = umode_get('o');      /*  0x0002	 Operator */
	UMODE_WALLOP = umode_get('w');    /*  0x0004	 send wallops to them */
	UMODE_FAILOP = umode_get('g');    /*  0x0008	 Shows some global messages */
	UMODE_HELPOP = umode_get('h');    /*  0x0010	 Help system operator */
	UMODE_REGNICK = umode_get('r');   /*  0x0020	 Nick set by services as registered */
	UMODE_SADMIN = umode_get('a');    /*  0x0040	 Services Admin */
	UMODE_ADMIN = umode_get('A');     /*  0x0080	 Admin */
	UMODE_SERVNOTICE = umode_get('s');/* 0x0100	 server notices such as kill */
	UMODE_LOCOP = umode_get('O');     /* 0x0200	 Local operator -- SRB */
	UMODE_RGSTRONLY = umode_get('R'); /* 0x0400  Only reg nick message */
	UMODE_WEBTV = umode_get('V');     /* 0x0800  WebTV Client */
	UMODE_SERVICES = umode_get('S');  /* 0x4000	 services */
	UMODE_HIDE = umode_get('x');	     /* 0x8000	 Hide from Nukes */
	UMODE_NETADMIN = umode_get('N');  /* 0x10000	 Network Admin */
	UMODE_COADMIN = umode_get('C');   /* 0x80000	 Co Admin */
	UMODE_WHOIS = umode_get('W');     /* 0x100000	 gets notice on /whois */
	UMODE_KIX = umode_get('q');       /* 0x200000	 usermode +q */
	UMODE_BOT = umode_get('B');       /* 0x400000	 User is a bot */
	UMODE_SECURE = umode_get('z');    /*	0x800000	 User is a secure connect */
	UMODE_HIDING = umode_get('I');    /* 0x2000000	 Totally invisible .. */
	UMODE_VICTIM = umode_get('v');    /* 0x8000000	 Intentional Victim */
	UMODE_DEAF = umode_get('d');      /* 0x10000000       Deaf */
	UMODE_HIDEOPER = umode_get('H');  /* 0x20000000	 Hide oper mode */
	UMODE_SETHOST = umode_get('t');   /* 0x40000000	 used sethost */
	UMODE_STRIPBADWORDS = umode_get('G'); /* 0x80000000	 */
}

void make_umodestr(void)
{
	int i;
	char *m;

	m = umodestring;
	for (i = 0; i <= Usermode_highest; i++)
	{
		if (Usermode_Table[i].flag)
			*m++ = Usermode_Table[i].flag;
	}
	*m = '\0';
}
long	umode_get(char ch)
{
	short	 i = 0;
	short	 j = 0;
	while (i < UMODETABLESZ)
	{
		if (!Usermode_Table[i].flag)
		{
			break;
		}
		i++;
	}
	if (i != UMODETABLESZ)
	{
		Usermode_Table[i].flag = ch;
		Debug((DEBUG_DEBUG, "umode_get(%c) returning %04x",
			ch, Usermode_Table[i].mode));
		/* Update usermode table highest */
		for (j = 0; j < UMODETABLESZ; j++)
			if (Usermode_Table[i].flag)
				if (i > Usermode_highest)
					Usermode_highest = i;
		make_umodestr();
		return (Usermode_Table[i].mode);
	}
	else
	{
		Debug((DEBUG_DEBUG, "umode_get failed, no space"));
		return (0);
	}
}


int	umode_delete(char ch, long val)
{
	int i = 0;
	Debug((DEBUG_DEBUG, "umode_delete %c, %li",
		ch, val));	
	
	while (i < UMODETABLESZ)
	{
		if ((Usermode_Table[i].flag == ch) && (Usermode_Table[i].mode == val))
		{
			Usermode_Table[i].flag = '\0';
			return 1;
		}	
		i++;
	}
	return -1;
}
