/*
 * hideserver.c - Hide certain or all servers from /MAP & /LINKS
 *
 * Note that this module simple hides servers. It does not truly
 * increase security. Use as you wish.
 *
 * (C) Copyright 2003-2004 AngryWolf <angrywolf@flashmail.com>
 * (C) Copyright 2016 Bram Matthys <syzop@vulnscan.org>
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

CMD_OVERRIDE_FUNC(override_map);
CMD_OVERRIDE_FUNC(override_links);
static int cb_test(ConfigFile *, ConfigEntry *, int, int *);
static int cb_conf(ConfigFile *, ConfigEntry *, int);

ConfigItem_ulines *HiddenServers;

static struct
{
	unsigned	disable_map : 1;
	unsigned	disable_links : 1;
	char		*map_deny_message;
	char		*links_deny_message;
} Settings;

static ModuleInfo	*MyModInfo;
#define MyMod		MyModInfo->handle
#define SAVE_MODINFO	MyModInfo = modinfo;

static int lmax = 0;
static int umax = 0;

static int dcount(int n)
{
   int cnt = 0;

   while (n != 0)
   {
	   n = n/10;
	   cnt++;
   }

   return cnt;
}

ModuleHeader MOD_HEADER
  = {
	"hideserver",
	"5.0",
	"Hide servers from /MAP & /LINKS",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

static void InitConf()
{
	memset(&Settings, 0, sizeof Settings);
}

static void FreeConf()
{
	ConfigItem_ulines	*h, *next;

	safe_free(Settings.map_deny_message);
	safe_free(Settings.links_deny_message);

	for (h = HiddenServers; h; h = next)
	{
		next = h->next;
		DelListItem(h, HiddenServers);
		safe_free(h->servername);
		safe_free(h);
	}
}

MOD_TEST()
{
	SAVE_MODINFO
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, cb_test);

	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	SAVE_MODINFO
	HiddenServers = NULL;
	InitConf();

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, cb_conf);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!CommandOverrideAdd(MyMod, "MAP", 0, override_map))
		return MOD_FAILED;

	if (!CommandOverrideAdd(MyMod, "LINKS", 0, override_links))
		return MOD_FAILED;

	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	FreeConf();

	return MOD_SUCCESS;
}

static int cb_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;

	if (type == CONFIG_MAIN)
	{
		if (!strcmp(ce->name, "hideserver"))
		{
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "hide"))
				{
					/* No checking needed */
				}
				else if (!cep->value)
				{
					config_error("%s:%i: %s::%s without value",
						cep->file->filename,
						cep->line_number,
						ce->name, cep->name);
					errors++;
					continue;
				}
				else if (!strcmp(cep->name, "disable-map"))
					;
				else if (!strcmp(cep->name, "disable-links"))
					;
				else if (!strcmp(cep->name, "map-deny-message"))
					;
				else if (!strcmp(cep->name, "links-deny-message"))
					;
				else
				{
					config_error("%s:%i: unknown directive hideserver::%s",
						cep->file->filename, cep->line_number, cep->name);
					errors++;
				}
			}
			*errs = errors;
			return errors ? -1 : 1;
		}
	}

	return 0;
}

static int cb_conf(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry		*cep, *cepp;
	ConfigItem_ulines	*ca;

	if (type == CONFIG_MAIN)
	{
		if (!strcmp(ce->name, "hideserver"))
		{
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "disable-map"))
					Settings.disable_map = config_checkval(cep->value, CFG_YESNO);
				else if (!strcmp(cep->name, "disable-links"))
					Settings.disable_links = config_checkval(cep->value, CFG_YESNO);
				else if (!strcmp(cep->name, "map-deny-message"))
				{
					safe_strdup(Settings.map_deny_message, cep->value);
				}
				else if (!strcmp(cep->name, "links-deny-message"))
				{
					safe_strdup(Settings.links_deny_message, cep->value);
				}
				else if (!strcmp(cep->name, "hide"))
				{
					for (cepp = cep->items; cepp; cepp = cepp->next)
					{
						if (!strcasecmp(cepp->name, me.name))
							continue;

						ca = safe_alloc(sizeof(ConfigItem_ulines));
						safe_strdup(ca->servername, cepp->name);
						AddListItem(ca, HiddenServers);
					}
				}
			}

			return 1;
		}
	}

	return 0;
}

ConfigItem_ulines *FindHiddenServer(char *servername)
{
	ConfigItem_ulines *h;

	for (h = HiddenServers; h; h = h->next)
		if (!strcasecmp(servername, h->servername))
			break;

	return h;
}

/*
 * New /MAP format -Potvin
 * dump_map function.
 */
static void dump_map(Client *client, Client *server, char *mask, int prompt_length, int length)
{
	static char prompt[64];
	char *p = &prompt[prompt_length];
	int  cnt = 0;
	Client *acptr;

	*p = '\0';

	if (prompt_length > 60)
		sendnumeric(client, RPL_MAPMORE, prompt, length, server->name);
	else
	{
		char tbuf[256];
		char sid[10];
		int len = length - strlen(server->name) + 1;

		if (len < 0)
			len = 0;
		if (len > 255)
			len = 255;

		tbuf[len--] = '\0';
		while (len >= 0)
			tbuf[len--] = '-';
		if (IsOper(client))
			snprintf(sid, sizeof(sid), " [%s]", server->id);
		sendnumeric(client, RPL_MAP, prompt, server->name, tbuf, umax,
			server->server->users, (double)(lmax < 10) ? 4 : (lmax == 100) ? 6 : 5,
			(server->server->users * 100.0 / irccounts.clients),
			IsOper(client) ? sid : "");
		cnt = 0;
	}

	if (prompt_length > 0)
	{
		p[-1] = ' ';
		if (p[-2] == '`')
			p[-2] = ' ';
	}
	if (prompt_length > 60)
		return;

	strcpy(p, "|-");

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (acptr->uplink != server ||
 		    (IsULine(acptr) && HIDE_ULINES && !ValidatePermissionsForPath("server:info:map:ulines",client,NULL,NULL,NULL)))
			continue;
		if (FindHiddenServer(acptr->name))
			break;
		SetMap(acptr);
		cnt++;
	}

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (IsULine(acptr) && HIDE_ULINES && !ValidatePermissionsForPath("server:info:map:ulines",client,NULL,NULL,NULL))
			continue;
		if (FindHiddenServer(acptr->name))
			break;
		if (acptr->uplink != server)
			continue;
		if (!IsMap(acptr))
			continue;
		if (--cnt == 0)
			*p = '`';
		dump_map(client, acptr, mask, prompt_length + 2, length - 2);
	}

	if (prompt_length > 0)
		p[-1] = '-';
}

void dump_flat_map(Client *client, Client *server, int length)
{
	char buf[4];
	char tbuf[256];
	Client *acptr;
	int cnt = 0, hide_ulines;

	hide_ulines = (HIDE_ULINES && !ValidatePermissionsForPath("server:info:map:ulines",client,NULL,NULL,NULL)) ? 1 : 0;

	sendnumeric(client, RPL_MAP, "", server->name, "-", umax, server->server->users,
		(lmax < 10) ? 4 : (lmax == 100) ? 6 : 5,
		(server->server->users * 100.0 / irccounts.clients), "");

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if ((IsULine(acptr) && hide_ulines) || (acptr == server))
			continue;
		if (FindHiddenServer(acptr->name))
			break;
		cnt++;
	}

	strcpy(buf, "|-");
	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		int len = 0;
		if ((IsULine(acptr) && hide_ulines) || (acptr == server))
			continue;
		if (FindHiddenServer(acptr->name))
			break;
		if (--cnt == 0)
			*buf = '`';

		len = length - strlen(server->name) + 1;
		if (len < 0)
			len = 0;
		if (len > 255)
			len = 255;

		tbuf[len--] = '\0';
		while (len >= 0)
			tbuf[len--] = '-';

		sendnumeric(client, RPL_MAP, buf, server->name, tbuf, umax, server->server->users,
			(lmax < 10) ? 4 : (lmax == 100) ? 6 : 5,
			(server->server->users * 100.0 / irccounts.clients), "");
	}
}

/*
** New /MAP format. -Potvin
** cmd_map (NEW)
**
**      parv[1] = server mask
**/
CMD_OVERRIDE_FUNC(override_map)
{
	Client *acptr;
	int longest = strlen(me.name);
	float avg_users = 0.0;

	umax = 0;
	lmax = 0;

	if (parc < 2)
		parv[1] = "*";
	
	if (IsOper(client))
	{
		CallCommandOverride(ovr, client, recv_mtags, parc, parv);
		return;
	}

	if (Settings.disable_map)
	{
		if (Settings.map_deny_message)
			sendnotice(client, "%s", Settings.map_deny_message);
		else
			sendnumeric(client, RPL_MAPEND);
		return;
	}

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		int perc = 0;
		if (FindHiddenServer(acptr->name))
			break;
		perc = (acptr->server->users * 100 / irccounts.clients);
		if ((strlen(acptr->name) + acptr->hopcount * 2) > longest)
			longest = strlen(acptr->name) + acptr->hopcount * 2;
		if (lmax < perc)
			lmax = perc;
		if (umax < dcount(acptr->server->users))
			umax = dcount(acptr->server->users);
	}

	if (longest > 60)
		longest = 60;
	longest += 2;

	if (FLAT_MAP && !ValidatePermissionsForPath("server:info:map:real-map",client,NULL,NULL,NULL))
		dump_flat_map(client, &me, longest);
	else
		dump_map(client, &me, "*", 0, longest);

	avg_users = irccounts.clients * 1.0 / irccounts.servers;
	sendnumeric(client, RPL_MAPUSERS, irccounts.servers, (irccounts.servers > 1 ? "s" : ""), irccounts.clients,
		(irccounts.clients > 1 ? "s" : ""), avg_users);
	sendnumeric(client, RPL_MAPEND);
}

CMD_OVERRIDE_FUNC(override_links)
{
	Client *acptr;
	int flat = (FLAT_MAP && !IsOper(client)) ? 1 : 0;

	if (IsOper(client))
	{
		CallCommandOverride(ovr, client, recv_mtags, parc, parv);
		return;
	}

	if (Settings.disable_links)
	{
		if (Settings.links_deny_message)
			sendnotice(client, "%s", Settings.links_deny_message);
		else
			sendnumeric(client, RPL_ENDOFLINKS, "*");
		return;
	}

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		/* Some checks */
		if (HIDE_ULINES && IsULine(acptr) && !ValidatePermissionsForPath("server:info:map:ulines",client,NULL,NULL,NULL))
			continue;
		if (FindHiddenServer(acptr->name))
			continue;
		if (flat)
			sendnumeric(client, RPL_LINKS, acptr->name, me.name,
			    1, (acptr->info[0] ? acptr->info : "(Unknown Location)"));
		else
			sendnumeric(client, RPL_LINKS, acptr->name, acptr->uplink->name,
			    acptr->hopcount, (acptr->info[0] ? acptr->info : "(Unknown Location)"));
	}

	sendnumeric(client, RPL_ENDOFLINKS, "*");
}
