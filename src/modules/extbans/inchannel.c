/*
 * Extended ban: "in channel?" (+b ~c:#chan)
 * (C) Copyright 2003-.. Bram Matthys (Syzop) and the UnrealIRCd team
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

ModuleHeader MOD_HEADER(inchannel)
= {
	"chanmodes/extbans/inchannel",
	"4.0",
	"ExtBan ~c - banned when in specified channel",
	"3.2-b8-1",
	NULL 
};

/* Forward declarations */
int extban_inchannel_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2);
char *extban_inchannel_conv_param(char *para);
int extban_inchannel_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type);

/** Called upon module init */
MOD_INIT(inchannel)
{
	ExtbanInfo req;
	
	req.flag = 'c';
	req.is_ok = extban_inchannel_is_ok;
	req.conv_param = extban_inchannel_conv_param;
	req.is_banned = extban_inchannel_is_banned;
	req.options = EXTBOPT_INVEX; /* for +I too */
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD(inchannel)
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD(inchannel)
{
	return MOD_SUCCESS;
}

char *extban_inchannel_conv_param(char *para)
{
	static char retbuf[CHANNELLEN+6];
	char *chan, *p, symbol='\0';

	strlcpy(retbuf, para, sizeof(retbuf));
	chan = retbuf+3;

	if ((*chan == '+') || (*chan == '%') || (*chan == '%') ||
	    (*chan == '@') || (*chan == '&') || (*chan == '~'))
	    chan++;

	if ((*chan != '#') && (*chan != '*') && (*chan != '?'))
		return NULL;

	if (strlen(chan) > CHANNELLEN)
		chan[CHANNELLEN] = '\0';

	clean_channelname(chan);

	p = strchr(chan, ':'); /* ~r:#chan:*.blah.net is not allowed (for now) */
	if (p)
		*p = '\0';

	/* on a sidenote '#' is allowed because it's a valid channel (atm) */
	return retbuf;
}

/* The only purpose of this function is a temporary workaround to prevent a desynch.. pfff */
int extban_inchannel_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2)
{
	char *p;

	if ((checkt == EXBCHK_PARAM) && MyClient(sptr) && (what == MODE_ADD) && (strlen(para) > 3))
	{
		p = para + 3;
		if ((*p == '+') || (*p == '%') || (*p == '%') ||
		    (*p == '@') || (*p == '&') || (*p == '~'))
		    p++;

		if (*p != '#')
		{
			sendnotice(sptr, "Please use a # in the channelname (eg: ~c:#*blah*)");
			return 0;
		}
	}
	return 1;
}

static int extban_inchannel_compareflags(char symbol, int flags)
{
	int require=0;

	if (symbol == '+')
		require = CHFL_VOICE|CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '%')
		require = CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '@')
		require = CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '&')
		require = CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '~')
		require = CHFL_CHANOWNER;

	if (flags & require)
		return 1;

	return 0;
}

int extban_inchannel_is_banned(aClient *sptr, aChannel *chptr, char *ban, int type)
{
	Membership *lp;
	char *p = ban+3, symbol = '\0';

	if (*p != '#')
	{
		symbol = *p;
		p++;
	}

	for (lp = sptr->user->channel; lp; lp = lp->next)
	{
		if (!match_esc(p, lp->chptr->chname))
		{
			/* Channel matched, check symbol if needed (+/%/@/etc) */
			if (symbol)
			{
				if (extban_inchannel_compareflags(symbol, lp->flags))
					return 1;
			} else
				return 1;
		}
	}

	return 0;
}

