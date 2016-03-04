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

CMD_FUNC(m_help);

#define MSG_HELP 	"HELP"	
#define MSG_HELPOP	"HELPOP"

ModuleHeader MOD_HEADER(m_help)
  = {
	"m_help",
	"4.0",
	"command /help", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_help)
{
	CommandAdd(modinfo->handle, MSG_HELP, m_help, 1, M_USER);
	CommandAdd(modinfo->handle, MSG_HELPOP, m_help, 1, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_help)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_help)
{
	return MOD_SUCCESS;
}

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
	aMotdLine *text;
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

/*
** m_help (help/write to +h currently online) -Donwulff
**	parv[1] = optional message text
*/
CMD_FUNC(m_help)
{
	char *helptopic, *s;
	Link *tmpl;

	if (!MyClient(sptr))
		return 0; /* never remote */

	helptopic = parc > 1 ? parv[1] : NULL;
	
	if (helptopic && (*helptopic == '?'))
		helptopic++;

	parse_help(sptr, sptr->name, BadPtr(helptopic) ? NULL : helptopic);

	return 0;
}
