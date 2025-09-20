/*
 * UTF8ONLY ISUPPORT: Only allow UTF8 incoming and outgoing on IRC
 * (C) Copyright 2025-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"utf8only",
	"1.0.0",
	"only allow UTF8 traffic on IRC (UTF8ONLY)",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
int utf8only_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int utf8only_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int utf8only_packet(Client *from, Client *to, Client *intended_to, char **msg, int *length);

/* Variables */
int utf8_only = 0;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, utf8only_config_test);

	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, utf8only_config_run);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	/* You may wonder what this is... two variables for the same setting?
	 * The thing is that iConf.utf8_only is external because it is also
	 * used in src/send. But... if we only set iConf.utf8_only if
	 * set::utf8-only is present then if it is first set to 1, and then
	 * later after a config change is deleted, it won't be set to 0.
	 * So we use this 'trick'.
	 * The alternative would be to do an iConf.utf8_only = 0 in MOD_INIT
	 * but then it is always zero for a short while, during rehashes,
	 * which may or may not be an issue, depending on if traffic is
	 * sent during a rehash (eg rehash output).
	 */
	iConf.utf8_only = utf8_only;

	if (iConf.utf8_only)
	{
		ISupportAdd(modinfo->handle, "UTF8ONLY", NULL);
		HookAdd(modinfo->handle, HOOKTYPE_PACKET, 0, utf8only_packet);
	}

	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int utf8only_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;

	if (ce && !strcmp(ce->name, "utf8-only"))
	{
		if (!ce->value)
		{
			config_error("%s:%d: set::utf8-only: no value", ce->file->filename, ce->line_number);
			errors++;
		}
		*errs = errors;
		return errors ? -1 : 1;
	}
	return 0;
}

int utf8only_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	if (type != CONFIG_SET)
		return 0;

	if (ce && !strcmp(ce->name, "utf8-only"))
	{
		iConf.utf8_only = utf8_only = config_checkval(ce->value, CFG_YESNO);
		return 1;
	}
	return 0;
}

/* Get the command from a line from an IRC client.
 * Returns "*" if not found (eg empty line, or malformed)
 */
const char *parse_get_command(const char *msg)
{
	const char *p = msg;
	static char cmd[64];

	/* Skip whitespace at beginning of the line */
	for (; *p == ' '; p++);
	/* Skip message tags (if any) */
	if (*p == '@')
	{
		for (p = p + 1; *p != ' '; p++);
		for (; *p == ' '; p++);
	}

	if (*p)
	{
		char *o = cmd;
		int sz = sizeof(cmd) - 1;
		for (; sz && *p && (*p != ' '); p++)
		{
			*o++ = *p;
			sz--;
		}
		*o++ = '\0';
	} else {
		strlcpy(cmd, "*", sizeof(cmd));
	}

	return cmd;
}

int utf8only_packet(Client *from, Client *to, Client *intended_to, char **msg, int *length)
{
	static char buf[16384];

	if (IsMe(from))
		return 0;

	if (IsServer(from) || IsUnknown(from))
	{
		/* For servers we convert the contents just in case,
		 * since it may be 'poisoned' with non-UTF8 stuff
		 * (eg if remote server does not enable UTF8ONLY).
		 * For unknown connections we do this too, because
		 * otherwise things like 'USER' with invalid UTF8
		 * may not be allowed through, which is a bit
		 * confusing to the user.
		 */
		char *ret = unrl_utf8_make_valid(*msg, buf, sizeof(buf), 0);
		if (ret != *msg)
		{
			*msg = ret;
			*length = strlen(*msg); /* Needs to be recalculated */
		}
		return 0;
	} else if (IsUser(from)) {
		/* Connected user: be strict */
		if (!unrl_utf8_validate(*msg, NULL))
		{
			const char *cmd = parse_get_command(*msg);
			if (HasCapability(from, "standard-replies"))
				sendto_one(from, NULL, ":%s FAIL %s INVALID_UTF8 :Message rejected, your IRC client MUST use UTF-8 encoding on this network",
				           me.name, cmd);
			else
				sendnumeric(from, ERR_CANNOTDOCOMMAND, cmd, "Message rejected, your IRC client MUST use UTF-8 encoding on this network");
			*msg = NULL;
			*length = 0;
			return 0;
		}
	}
	return 0;
}
