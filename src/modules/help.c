/*
 *   IRC - Internet Relay Chat, src/modules/out.c
 *   (C) 2004 The UnrealIRCd Team
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

#include "unrealircd.h"

CMD_FUNC(cmd_help);

#define MSG_HELP 	"HELP"	
#define MSG_HELPOP	"HELPOP"

ModuleHeader MOD_HEADER
  = {
	"help",
	"5.0",
	"command /help", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_HELP, cmd_help, 1, CMD_USER);
	CommandAdd(modinfo->handle, MSG_HELPOP, cmd_help, 1, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

// RPL_HELPSTART
#define HDR(subject, str) sendto_one(client, NULL, ":%s 704 %s %s :%s", me.name, client->name, subject, str);
// RPL_HELPTXT
#define SND(subject, str) sendto_one(client, NULL, ":%s 705 %s %s :%s", me.name, client->name, subject, str);
// RPL_ENDOFHELP
#define FTR(subject, str) sendto_one(client, NULL, ":%s 706 %s %s :%s", me.name, client->name, subject, str);

ConfigItem_help *find_Help(const char *command)
{
	ConfigItem_help *help;

	if (!command)
	{
		for (help = conf_help; help; help = help->next)
		{
			if (help->command == NULL)
				return help;
		}
		return NULL;
	}
	for (help = conf_help; help; help = help->next)
	{
		if (help->command == NULL)
			continue;
		else if (!strcasecmp(command,help->command))
			return help;
	}
	return NULL;
}

void parse_help(Client *client, const char *help)
{
	ConfigItem_help *helpitem;
	MOTDLine *text;
	if (BadPtr(help))
	{
		helpitem = find_Help(NULL);
		if (!helpitem)
			return;
		HDR("*", "        ***** UnrealIRCd Help System *****");
		SND("*", " -");
		text = helpitem->text;
		while (text) {
			SND("*", text->line);
			text = text->next;
		}
		FTR("*", " -");
		return;
		
	}
	helpitem = find_Help(help);
	if (!helpitem) {
		HDR("*", "        ***** No Help Available *****");
		SND("*", " -");
		SND("*", "   We're sorry, we don't have help available for the command you requested.");
		SND("*", " -");
		sendto_one(client, NULL, ":%s 705 %s * : ***** Go to %s if you have any further questions *****",
		    me.name, client->name, helpchan);
		FTR("*", " -");
		return;
	}
	text = helpitem->text;
	sendto_one(client, NULL, ":%s 704 %s %s :***** %s *****",
	    me.name, client->name, help, helpitem->command);
	SND(help, " -");
	while (text) {
		SND(help, text->line);
		text = text->next;
	}
	FTR(help, " -");
}

/*
** cmd_help (help/write to +h currently online) -Donwulff
**	parv[1] = optional message text
*/
CMD_FUNC(cmd_help)
{
	const char *helptopic;

	if (!MyUser(client))
		return; /* never remote */

	helptopic = parc > 1 ? parv[1] : NULL;
	
	if (helptopic && (*helptopic == '?'))
		helptopic++;

	parse_help(client, BadPtr(helptopic) ? NULL : helptopic);
}
