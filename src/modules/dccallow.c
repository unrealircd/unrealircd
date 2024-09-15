/*
 *   IRC - Internet Relay Chat, src/modules/m_dccallow
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

CMD_FUNC(cmd_dccallow);

#define MSG_DCCALLOW 	"DCCALLOW"

ModuleHeader MOD_HEADER
  = {
	"dccallow",
	"5.0",
	"command /dccallow", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

int dccallow_user_quit(Client *client, MessageTag *mtags, const char *comment);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_DCCALLOW, cmd_dccallow, 1, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, dccallow_user_quit);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, dccallow_user_quit);
	ISupportAdd(modinfo->handle, "USERIP", NULL);
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

/** Delete all DCCALLOW references.
 * Ultimately, this should be moved to modules/dccallow.c
 */
void remove_dcc_references(Client *client)
{
	Client *acptr;
	Link *lp, *nextlp;
	Link **lpp, *tmp;
	int found;

	lp = client->user->dccallow;
	while(lp)
	{
		nextlp = lp->next;
		acptr = lp->value.client;
		for(found = 0, lpp = &(acptr->user->dccallow); *lpp; lpp=&((*lpp)->next))
		{
			if (lp->flags == (*lpp)->flags)
				continue; /* match only opposite types for sanity */
			if ((*lpp)->value.client == client)
			{
				if ((*lpp)->flags == DCC_LINK_ME)
				{
					sendto_one(acptr, NULL, ":%s %d %s :%s has been removed from "
						"your DCC allow list for signing off",
						me.name, RPL_DCCINFO, acptr->name, client->name);
				}
				tmp = *lpp;
				*lpp = tmp->next;
				free_link(tmp);
				found++;
				break;
			}
		}

		if (!found)
		{
			unreal_log(ULOG_WARNING, "main", "BUG_REMOVE_DCC_REFERENCES", acptr,
			           "[BUG] remove_dcc_references: $client was in dccallowme "
			           "list of $existing_client but not in dccallowrem list!",
			           log_data_client("existing_client", client));
		}

		free_link(lp);
		lp = nextlp;
	}
}

/** Clean up dccallow list and (if needed) notify other clients
 * that have this person on DCCALLOW that the user just left/got removed.
 */
int dccallow_user_quit(Client *client, MessageTag *mtags, const char *comment)
{
	remove_dcc_references(client);
	return 0;
}

/* cmd_dccallow:
 * HISTORY:
 * Taken from bahamut 1.8.1
 */
CMD_FUNC(cmd_dccallow)
{
	char request[BUFSIZE];
	Link *lp;
	char *p, *s;
	Client *friend;
	int didlist = 0, didhelp = 0, didanything = 0;
	char **ptr;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("WHOIS");
	static char *dcc_help[] =
	{
		"/DCCALLOW [<+|->nick[,<+|->nick, ...]] [list] [help]",
		"You may allow DCCs of files which are otherwise blocked by the IRC server",
		"by specifying a DCC allow for the user you want to recieve files from.",
		"For instance, to allow the user Bob to send you file.exe, you would type:",
		"/DCCALLOW +bob",
		"and Bob would then be able to send you files. Bob will have to resend the file",
		"if the server gave him an error message before you added him to your allow list.",
		"/DCCALLOW -bob",
		"Will do the exact opposite, removing him from your dcc allow list.",
		"/dccallow list",
		"Will list the users currently on your dcc allow list.",
		NULL
	};

	if (!MyUser(client))
		return;
	
	if (parc < 2)
	{
		sendnotice(client, "No command specified for DCCALLOW. "
			"Type '/DCCALLOW HELP' for more information.");
		return;
	}

	strlcpy(request, parv[1], sizeof(request));
	for (p = NULL, s = strtoken(&p, request, ", "); s; s = strtoken(&p, NULL, ", "))
	{
		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, s, maxtargets, "DCCALLOW");
			break;
		}
		if (*s == '+')
		{
			didanything = 1;
			if (!*++s)
				continue;
			
			friend = find_user(s, NULL);
			
			if (friend == client)
				continue;
			
			if (!friend)
			{
				sendnumeric(client, ERR_NOSUCHNICK, s);
				continue;
			}
			add_dccallow(client, friend);
		} else
		if (*s == '-')
		{
			didanything = 1;
			if (!*++s)
				continue;
			
			friend = find_user(s, NULL);
			if (friend == client)
				continue;
			if (!friend)
			{
				sendnumeric(client, ERR_NOSUCHNICK, s);
				continue;
			}
			del_dccallow(client, friend);
		} else
		if (!didlist && !strncasecmp(s, "list", 4))
		{
			didanything = didlist = 1;
			sendnumeric(client, RPL_DCCINFO, "The following users are on your dcc allow list:");
			for(lp = client->user->dccallow; lp; lp = lp->next)
			{
				if (lp->flags == DCC_LINK_REMOTE)
					continue;
				sendnumericfmt(client, RPL_DCCLIST, ":%s (%s@%s)", lp->value.client->name,
					lp->value.client->user->username,
					GetHost(lp->value.client));
			}
			sendnumeric(client, RPL_ENDOFDCCLIST, s);
		} else
		if (!didhelp && !strncasecmp(s, "help", 4))
		{
			didanything = didhelp = 1;
			for(ptr = dcc_help; *ptr; ptr++)
				sendnumeric(client, RPL_DCCINFO, *ptr);
			sendnumeric(client, RPL_ENDOFDCCLIST, s);
		}
	}
	if (!didanything)
	{
		sendnotice(client, "Invalid syntax for DCCALLOW. Type '/DCCALLOW HELP' for more information.");
		return;
	}
}

/* The dccallow functions below are all taken from bahamut (1.8.1).
 * Well, with some small modifications of course. -- Syzop
 */

/** Adds 'optr' to the DCCALLOW list of 'client' */
int add_dccallow(Client *client, Client *optr)
{
	Link *lp;
	int cnt = 0;

	for (lp = client->user->dccallow; lp; lp = lp->next)
	{
		if (lp->flags != DCC_LINK_ME)
			continue;
		cnt++;
		if (lp->value.client == optr)
			return 0;
	}

	if (cnt >= MAXDCCALLOW)
	{
		sendnumeric(client, ERR_TOOMANYDCC,
			optr->name, MAXDCCALLOW);
		return 0;
	}

	lp = make_link();
	lp->value.client = optr;
	lp->flags = DCC_LINK_ME;
	lp->next = client->user->dccallow;
	client->user->dccallow = lp;

	lp = make_link();
	lp->value.client = client;
	lp->flags = DCC_LINK_REMOTE;
	lp->next = optr->user->dccallow;
	optr->user->dccallow = lp;

	sendnumeric(client, RPL_DCCSTATUS, optr->name, "added to");
	return 0;
}

/** Removes 'optr' from the DCCALLOW list of 'client' */
int del_dccallow(Client *client, Client *optr)
{
	Link **lpp, *lp;
	int found = 0;

	for (lpp = &(client->user->dccallow); *lpp; lpp=&((*lpp)->next))
	{
		if ((*lpp)->flags != DCC_LINK_ME)
			continue;
		if ((*lpp)->value.client == optr)
		{
			lp = *lpp;
			*lpp = lp->next;
			free_link(lp);
			found++;
			break;
		}
	}
	if (!found)
	{
		sendnumericfmt(client, RPL_DCCINFO, ":%s is not in your DCC allow list", optr->name);
		return 0;
	}

	for (found = 0, lpp = &(optr->user->dccallow); *lpp; lpp=&((*lpp)->next))
	{
		if ((*lpp)->flags != DCC_LINK_REMOTE)
			continue;
		if ((*lpp)->value.client == client)
		{
			lp = *lpp;
			*lpp = lp->next;
			free_link(lp);
			found++;
			break;
		}
	}
	if (!found)
	{
		unreal_log(ULOG_WARNING, "dccallow", "BUG_DCCALLOW", client,
		           "[BUG] DCCALLOW list for $client did not contain $target",
		           log_data_client("target", optr));
	}

	sendnumeric(client, RPL_DCCSTATUS, optr->name, "removed from");

	return 0;
}
