/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/help.c
 *   Copyright (C) 2001 codemastr (Dominick Meglio) <codemastr@unrealircd.com>
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
#include "h.h"
#include "proto.h"

#define HDR(str) sendto_one(sptr, ":%s 290 %s :%s", me.name, sptr->name, str);
#define SND(str) sendto_one(sptr, ":%s 292 %s :%s", me.name, sptr->name, str);

ConfigItem_help *Find_Help(char *command) {
	ConfigItem_help *help;
	if (!command) {
		for (help = conf_help; help; help = (ConfigItem_help *)help->next) {
			if (help->command == NULL)
				return help;
		}
		return NULL;
	}
	for (help = conf_help; help; help = (ConfigItem_help *)help->next) {
		if (help->command == NULL)
			continue;
		else if (!stricmp(command,help->command))
			return help;
	}
	return NULL;
}

int  parse_help(aClient *sptr, char *name, char *help)
{
	ConfigItem_help *helpitem;
	aMotd *text;
	if (BadPtr(help))
	{
		helpitem = Find_Help(NULL);
		if (!helpitem)
			return 1;
		SND(" -");
		HDR("        ***** UnrealIRCd Help System *****");
		SND(" -");
		text = helpitem->text;
		while (text) {
			SND(text->line);
			text = text->next;
		}
		SND(" -");
		return 1;
		
	}
	helpitem = Find_Help(help);
	if (!helpitem) {
		SND(" -");
		HDR("        ***** No Help Available *****");
		SND(" -");
		SND("   We're sorry, we don't have help available for the command you requested.");
		SND(" -");
		sendto_one(sptr,":%s 292 %s : ***** Go to %s if you have any further questions *****",
		    me.name, sptr->name, helpchan);
		SND(" -");
		return 0;
	}
	text = helpitem->text;
	SND(" -");
	sendto_one(sptr,":%s 290 %s :***** %s *****",
	    me.name, sptr->name, helpitem->command);
	SND(" -");
	while (text) {
		SND(text->line);
		text = text->next;
	}
	SND(" -");
	return 1;
}
