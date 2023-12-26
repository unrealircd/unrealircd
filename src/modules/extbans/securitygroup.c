/* 
 * IRC - Internet Relay Chat, src/modules/extbans/securitygroup.c
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
	"unrealircd-6",
};

/* Forward declarations */
const char *extban_securitygroup_conv_param(BanContext *b, Extban *extban);
int extban_securitygroup_is_ok(BanContext *b);
int extban_securitygroup_is_banned(BanContext *b);

Extban *register_securitygroup_extban(ModuleInfo *modinfo)
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(req));
	req.letter = 'G';
	req.name = "security-group";
	req.conv_param = extban_securitygroup_conv_param;
	req.is_ok = extban_securitygroup_is_ok;
	req.is_banned = extban_securitygroup_is_banned;
	req.is_banned_events = BANCHK_ALL|BANCHK_TKL;
	req.options = EXTBOPT_INVEX|EXTBOPT_TKL;
	return ExtbanAdd(modinfo->handle, req);
}

/** Called upon module test */
MOD_TEST()
{
	if (!register_securitygroup_extban(modinfo))
	{
		config_error("could not register extended ban type ~G");
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

/** Called upon module init */
MOD_INIT()
{
	if (!register_securitygroup_extban(modinfo))
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
int extban_securitygroup_generic(char *mask, int strict)
{
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

	return 1;
}

int extban_securitygroup_is_ok(BanContext *b)
{
	if (b->client && MyUser(b->client) && (b->what == MODE_ADD) && (b->is_ok_check == EXBCHK_PARAM))
	{
		char banbuf[SECURITYGROUPLEN+8];
		strlcpy(banbuf, b->banstr, sizeof(banbuf));
		if (!extban_securitygroup_generic(banbuf, 1))
		{
			SecurityGroup *s;
			sendnotice(b->client, "ERROR: Unknown security-group '%s'. Syntax: +b ~security-group:securitygroup or +b ~security-group:!securitygroup", b->banstr);
			sendnotice(b->client, "Available security groups:");
			for (s = securitygroups; s; s = s->next)
				sendnotice(b->client, "%s", s->name);
			sendnotice(b->client, "unknown-users");
			sendnotice(b->client, "End of security group list.");
			return 0;
		}
	}
	return 1;
}

/** Security group extban - conv_param */
const char *extban_securitygroup_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[SECURITYGROUPLEN + 8];

	strlcpy(retbuf, b->banstr, sizeof(retbuf));
	if (!extban_securitygroup_generic(retbuf, 0))
		return NULL;

	return retbuf;
}

/** Is the user banned by ~G:something ? */
int extban_securitygroup_is_banned(BanContext *b)
{
	if (*b->banstr == '!')
		return !user_allowed_by_security_group_name(b->client, b->banstr+1);
	return user_allowed_by_security_group_name(b->client, b->banstr);
}
