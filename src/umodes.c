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

/* cptr->umodes (32 bits): 26 used, 6 free */
long UMODE_INVISIBLE = 0L;     /* makes user invisible */
long UMODE_OPER = 0L;          /* Operator */
long UMODE_WALLOP = 0L;        /* send wallops to them */
long UMODE_FAILOP = 0L;        /* Shows some global messages */
long UMODE_HELPOP = 0L;        /* Help system operator */
long UMODE_REGNICK = 0L;       /* Nick set by services as registered */
long UMODE_SADMIN = 0L;        /* Services Admin */
long UMODE_ADMIN = 0L;         /* Admin */
long UMODE_SERVNOTICE = 0L;    /* server notices such as kill */
long UMODE_LOCOP = 0L;         /* Local operator -- SRB */
long UMODE_RGSTRONLY = 0L;     /* Only reg nick message */
long UMODE_WEBTV = 0L;         /* WebTV Client */
long UMODE_SERVICES = 0L;      /* services */
long UMODE_HIDE = 0L;          /* Hide from Nukes */
long UMODE_NETADMIN = 0L;      /* Network Admin */
long UMODE_COADMIN = 0L;       /* Co Admin */
long UMODE_WHOIS = 0L;         /* gets notice on /whois */
long UMODE_KIX = 0L;           /* usermode +q */
long UMODE_BOT = 0L;           /* User is a bot */
long UMODE_SECURE = 0L;        /* User is a secure connect */
long UMODE_VICTIM = 0L;        /* Intentional Victim */
long UMODE_DEAF = 0L;          /* Deaf */
long UMODE_HIDEOPER = 0L;      /* Hide oper mode */
long UMODE_SETHOST = 0L;       /* Used sethost */
long UMODE_STRIPBADWORDS = 0L; /* Strip badwords */
long UMODE_HIDEWHOIS = 0L;     /* Hides channels in /whois */

long AllUmodes;		/* All umodes */
long SendUmodes;	/* All umodes which are sent to other servers (global umodes) */

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
	UMODE_INVISIBLE = umode_gget('i'); /*  0x0001	 makes user invisible */
	UMODE_OPER = umode_gget('o');      /*  0x0002	 Operator */
	UMODE_WALLOP = umode_gget('w');    /*  0x0004	 send wallops to them */
	UMODE_FAILOP = umode_gget('g');    /*  0x0008	 Shows some global messages */
	UMODE_HELPOP = umode_gget('h');    /*  0x0010	 Help system operator */
	UMODE_REGNICK = umode_gget('r');   /*  0x0020	 Nick set by services as registered */
	UMODE_SADMIN = umode_gget('a');    /*  0x0040	 Services Admin */
	UMODE_ADMIN = umode_gget('A');     /*  0x0080	 Admin */
	UMODE_SERVNOTICE = umode_lget('s');/* 0x0100	 server notices such as kill */
	UMODE_LOCOP = umode_lget('O');     /* 0x0200	 Local operator -- SRB */
	UMODE_RGSTRONLY = umode_gget('R'); /* 0x0400  Only reg nick message */
	UMODE_WEBTV = umode_gget('V');     /* 0x0800  WebTV Client */
	UMODE_SERVICES = umode_gget('S');  /* 0x4000	 services */
	UMODE_HIDE = umode_gget('x');	     /* 0x8000	 Hide from Nukes */
	UMODE_NETADMIN = umode_gget('N');  /* 0x10000	 Network Admin */
	UMODE_COADMIN = umode_gget('C');   /* 0x80000	 Co Admin */
	UMODE_WHOIS = umode_gget('W');     /* 0x100000	 gets notice on /whois */
	UMODE_KIX = umode_gget('q');       /* 0x200000	 usermode +q */
	UMODE_BOT = umode_gget('B');       /* 0x400000	 User is a bot */
	UMODE_SECURE = umode_gget('z');    /*	0x800000	 User is a secure connect */
	UMODE_VICTIM = umode_gget('v');    /* 0x8000000	 Intentional Victim */
	UMODE_DEAF = umode_gget('d');      /* 0x10000000       Deaf */
	UMODE_HIDEOPER = umode_gget('H');  /* 0x20000000	 Hide oper mode */
	UMODE_SETHOST = umode_gget('t');   /* 0x40000000	 used sethost */
	UMODE_STRIPBADWORDS = umode_gget('G'); /* 0x80000000	 */
	UMODE_HIDEWHOIS = umode_gget('p'); /* Hides channels in /whois */
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

/* umode_get:
 * Add a usermode with character 'ch', if global is set to 1 the usermode is global
 * (sent to other servers) otherwise it's a local usermode
 */
long	umode_get(char ch, int global, int (*allowed)(aClient *sptr))
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
		Usermode_Table[i].allowed = allowed;
		Debug((DEBUG_DEBUG, "umode_get(%c) returning %04x",
			ch, Usermode_Table[i].mode));
		/* Update usermode table highest */
		for (j = 0; j < UMODETABLESZ; j++)
			if (Usermode_Table[i].flag)
				if (i > Usermode_highest)
					Usermode_highest = i;
		make_umodestr();
		AllUmodes |= Usermode_Table[i].mode;
		if (global)
			SendUmodes |= Usermode_Table[i].mode;
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
			AllUmodes &= ~val;
			SendUmodes &= ~val;
			return 1;
		}	
		i++;
	}
	return -1;
}

int umode_allow_all(aClient *sptr)
{
	return 1;
}

int umode_allow_opers(aClient *sptr)
{
	return IsAnOper(sptr) ? 1 : 0;
}

