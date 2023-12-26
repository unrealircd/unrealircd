/* IRC - Internet Relay Chat, src/modules/rmtkl.c
 * Easily remove *-Lines in bulk
 * (C) Copyright 2019 Gottem and the UnrealIRCd team
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

ModuleHeader MOD_HEADER = {
	"rmtkl",
	"1.4",
	"Adds /rmtkl command to easily remove *-Lines in bulk",
	"Gottem and the UnrealIRCd Team",
	"unrealircd-6",
};

#define IsParam(x) (parc > (x) && !BadPtr(parv[(x)]))
#define IsNotParam(x) (parc <= (x) || BadPtr(parv[(x)]))

typedef struct {
	int type;
	char flag;
	char *txt;
	char *operpriv;
} TKLType;

static void dump_str(Client *client, const char **buf);
static TKLType *find_TKLType_by_flag(char flag);
void rmtkl_check_options(const char *param, int *skipperm, int *silent);
int rmtkl_tryremove(Client *client, TKLType *tkltype, TKL *tkl, const char *uhmask, const char *commentmask, int skipperm, int silent);
CMD_FUNC(rmtkl);

TKLType tkl_types[] = {
	{ TKL_KILL, 'k', "K-Line", "server-ban:kline:remove" },
	{ TKL_ZAP, 'z',	"Z-Line", "server-ban:zline:local:remove" },
	{ TKL_KILL | TKL_GLOBAL, 'G', "G-Line", "server-ban:gline:remove" },
	{ TKL_ZAP | TKL_GLOBAL, 'Z', "Global Z-Line", "server-ban:zline:global:remove" },
	{ TKL_SHUN | TKL_GLOBAL, 's', "Shun", "server-ban:shun:remove" },
//	{ TKL_SPAMF | TKL_GLOBAL, 'F', "Global Spamfilter", "server-ban:spamfilter:remove" }, TODO: re-add spamfilter support
	{ 0, 0, "Unknown *-Line", 0 },
};

static const char *rmtkl_help[] = {
	"*** \002Help on /rmtkl\002 *** ",
	"Removes all TKLs matching the given conditions from the local server, or the entire",
	"network if it's a global-type ban.",
	"Syntax:",
	"    \002/rmtkl\002 \037user@host\037 \037type\037 [\037comment\037] [\037-skipperm\037] [\037-silent\037]",
	"The \037user@host\037 field is a wildcard mask to match the target of a ban.",
	"The \037type\037 field may contain any number of the following characters:",
	"    k, z, G, Z, s, F and *",
	"    These correspond to (local) K-Line, (local) Z-Line, G-Line, Global Z-Line, (global) Shun and (global) Spamfilter",
	"    (asterisk includes every type besides F)",
	"The \037comment\037 field is also a wildcard mask to match the reason text of a ban. If specified, it must always",
	"come \037before\037 the options starting with \002-\002.",
	"Examples:",
	"    - \002/rmtkl * *\002",
	"        [remove \037all\037 supported TKLs except spamfilters]",
	"    - \002/rmtkl *@*.mx GZ\002 * -skipperm",
	"        [remove all Mexican G/Z-Lines while skipping over permanent ones]",
/*	"    - \002/rmtkl * * *Zombie*\002",
	"        [remove all non-spamfilter bans having \037Zombie\037 in the reason field]", TODO: re-add spamfilter support  */
	"*** \002End of help\002 ***",
	NULL
};

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (CommandExists("RMTKL"))
	{
		config_error("Command RMTKL already exists");
		return MOD_FAILED;
	}
	CommandAdd(modinfo->handle, "RMTKL", rmtkl, 5, CMD_USER);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

static void dump_str(Client *client, const char **buf)
{
	if (!MyUser(client))
		return;

	// Using sendto_one() instead of sendnumericfmt() because the latter strips indentation and stuff ;]
	for (; *buf != NULL; buf++)
		sendto_one(client, NULL, ":%s %03d %s :%s", me.name, RPL_TEXT, client->name, *buf);

	// Let user take 8 seconds to read it
	add_fake_lag(client, 8000);
}

static TKLType *find_TKLType_by_flag(char flag)
{
	TKLType *t;
	for (t = tkl_types; t->type; t++)
		if (t->flag == flag)
			break;
	return t;
}

void rmtkl_check_options(const char *param, int *skipperm, int *silent) {
	if (!strcasecmp("-skipperm", param))
		*skipperm = 1;
	if (!strcasecmp("-silent", param))
		*silent = 1;
}

int rmtkl_tryremove(Client *client, TKLType *tkltype, TKL *tkl, const char *uhmask, const char *commentmask, int skipperm, int silent)
{
	if (tkl->type != tkltype->type)
		return 0;

	// Let's not touch Q-Lines
	if (tkl->type & TKL_NAME)
		return 0;

	/* Don't touch TKL's that were added through config */
	if (tkl->flags & TKL_FLAG_CONFIG)
		return 0;

	if (TKLIsSpamfilter(tkl))
	{
#if 0
//FIXME: re-add spamfilter support
		// Is a spamfilter added through IRC, we can remove this if the "user" mask matches the reason
		if (!match_simple(uhmask, tkl->reason))
			return 0;
#endif
	} else
	if (TKLIsServerBan(tkl))
	{
		if (!match_simple(uhmask, make_user_host(tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask)))
			return 0;

		if (commentmask && !match_simple(commentmask, tkl->ptr.serverban->reason))
			return 0;
	} else
		return 0;

	if (skipperm && tkl->expire_at == 0)
		return 0;

	if (!silent)
		sendnotice_tkl_del(client->name, tkl);

	RunHook(HOOKTYPE_TKL_DEL, client, tkl);

	if (tkl->type & TKL_SHUN)
		tkl_check_local_remove_shun(tkl);
	tkl_del_line(tkl);
	return 1;
}

CMD_FUNC(rmtkl)
{
	TKL *tkl, *next;
	TKLType *tkltype;
	const char *types, *uhmask, *commentmask, *p;
	char tklchar;
	int tklindex, tklindex2, skipperm, silent;
	unsigned int count;
	char broadcast[BUFSIZE];

	if (!IsULine(client) && !IsOper(client))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (IsNotParam(1))
	{
		dump_str(client, rmtkl_help);
		return;
	}

	if (IsNotParam(2))
	{
		sendnotice(client, "Not enough parameters. Type /RMTKL for help.");
		return;
	}

	uhmask = parv[1];
	types = parv[2];
	commentmask = NULL;
	skipperm = 0;
	silent = 0;
	count = 0;
	snprintf(broadcast, sizeof(broadcast), ":%s RMTKL %s %s", client->name, types, uhmask);

	// Check for optionals
	if (IsParam(3))
	{
		// Comment mask, if specified, always goes third
		if (*parv[3] != '-')
			commentmask = parv[3];
		else
			rmtkl_check_options(parv[3], &skipperm, &silent);
		ircsnprintf(broadcast, sizeof(broadcast), "%s %s", broadcast, parv[3]);
	}
	if (IsParam(4))
	{
		rmtkl_check_options(parv[4], &skipperm, &silent);
		ircsnprintf(broadcast, sizeof(broadcast), "%s %s", broadcast, parv[4]);
	}
	if (IsParam(5))
	{
		rmtkl_check_options(parv[5], &skipperm, &silent);
		ircsnprintf(broadcast, sizeof(broadcast), "%s %s", broadcast, parv[5]);
	}

	// Wildcard resolves to everything but 'F', since spamfilters are a bit special
	if (strchr(types, '*'))
		types = "kzGZs";

	// Make sure the oper actually has the privileges to remove the *-Lines he wants
	if (!IsULine(client))
	{
		for (p = types; *p; p++)
		{
			tkltype = find_TKLType_by_flag(*p);
			if (!tkltype->type)
				continue;

			if (!ValidatePermissionsForPath(tkltype->operpriv, client, NULL, NULL, NULL))
			{
				sendnumeric(client, ERR_NOPRIVILEGES);
				return;
			}
		}
	}

	// Broadcast the command to other servers *before* we proceed with removal
	sendto_server(NULL, 0, 0, NULL, "%s", broadcast);

	// Loop over all supported types
	for (tkltype = tkl_types; tkltype->type; tkltype++) {
		if (!strchr(types, tkltype->flag))
			continue;

		// Loop over all TKL entries, first try the ones in the hash table
		tklchar = tkl_typetochar(tkltype->type);
		tklindex = tkl_ip_hash_type(tklchar);
		if (tklindex >= 0)
		{
			for (tklindex2 = 0; tklindex2 < TKLIPHASHLEN2; tklindex2++)
			{
				for (tkl = tklines_ip_hash[tklindex][tklindex2]; tkl; tkl = next)
				{
					next = tkl->next;
					count += rmtkl_tryremove(client, tkltype, tkl, uhmask, commentmask, skipperm, silent);
				}
			}
		}

		// Then the regular *-Lines (not an else because certain TKLs might have a hash as well as a plain linked list)
		tklindex = tkl_hash(tklchar);
		for (tkl = tklines[tklindex]; tkl; tkl = next)
		{
			next = tkl->next;
			count += rmtkl_tryremove(client, tkltype, tkl, uhmask, commentmask, skipperm, silent);
		}
	}

	unreal_log(ULOG_INFO, "tkl", "RMTKL_COMMAND", client,
	           "[rmtkl] $client removed $tkl_removed_count TKLine(s) using /RMTKL",
	           log_data_integer("tkl_removed_count", count));
}
