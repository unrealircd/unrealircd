/*
 * Extended ban to ban based on security groups such as "unknown-users"
 * (C) Copyright 2020 Bram Matthys (Syzop) and the UnrealIRCd team
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

ModuleHeader MOD_HEADER
= {
	"extbans/securitygroup",
	"4.2",
	"ExtBan ~G - Ban based on security-group",
	"UnrealIRCd Team",
	"unrealircd-5",
};

/* Forward declarations */
char *extban_securitygroup_conv_param(char *para);
int extban_securitygroup_is_ok(Client *client, Channel *channel, char *para, int checkt, int what, int what2);
int extban_securitygroup_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg);

/** Called upon module init */
MOD_INIT()
{
	ExtbanInfo req;
	
	req.flag = 'G';
	req.conv_param = extban_securitygroup_conv_param;
	req.is_ok = extban_securitygroup_is_ok;
	req.is_banned = extban_securitygroup_is_banned;
	req.options = EXTBOPT_INVEX|EXTBOPT_TKL;
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type ~G");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/* Helper function for extban_securitygroup_is_ok() and extban_securitygroup_conv_param()
 * to do ban validation.
 */
int extban_securitygroup_generic(char *para, int strict)
{
	char *mask;

	mask = para+3;

	/* ! at the start means negative match */
	if (*mask == '!')
		mask++;

	/* Check if the rest of the security group name is valid */
	if (strict)
	{
		if (!security_group_exists(mask))
			return 0; /* security group does not exist */
	} else {
		if (!security_group_valid_name(mask))
			return 0; /* invalid characters or too long */
	}

	if (!*mask)
		return 0; /* don't allow "~G:" nor "~G:!" */

	if (strlen(mask) > SECURITYGROUPLEN + 3)
		mask[SECURITYGROUPLEN + 3] = '\0';

	return 1;
}

int extban_securitygroup_is_ok(Client *client, Channel *channel, char *para, int checkt, int what, int what2)
{
	if (MyUser(client) && (what == MODE_ADD) && (checkt == EXBCHK_PARAM))
	{
		char banbuf[SECURITYGROUPLEN+8];
		strlcpy(banbuf, para, sizeof(banbuf));
		if (!extban_securitygroup_generic(banbuf, 1))
		{
			SecurityGroup *s;
			sendnotice(client, "ERROR: Unknown security-group '%s'. Syntax: +b ~G:securitygroup or +b ~G:!securitygroup", para+3);
			sendnotice(client, "Available security groups:");
			for (s = securitygroups; s; s = s->next)
				sendnotice(client, "%s", s->name);
			sendnotice(client, "unknown-users");
			sendnotice(client, "End of security group list.");
			return 0;
		}
	}
	return 1;
}

/** Security group extban - conv_param */
char *extban_securitygroup_conv_param(char *para)
{
	static char retbuf[SECURITYGROUPLEN + 8];

	strlcpy(retbuf, para, sizeof(retbuf));
	if (!extban_securitygroup_generic(retbuf, 0))
		return NULL;

	return retbuf;
}

/** Is the user banned by ~G:something ? */
int extban_securitygroup_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg)
{
	char *ban = banin+3;

	if (*ban == '!')
		return !user_allowed_by_security_group_name(client, ban+1);
	return user_allowed_by_security_group_name(client, ban);
}
