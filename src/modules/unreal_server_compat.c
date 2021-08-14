/*
 * unreal_server_compat - Compatibility with pre-U6 servers
 * (C) Copyright 2016-2021 Bram Matthys (Syzop)
 * License: GPLv2
 *
 * Currently the only purpose of this module is to rewrite
 * MODE lines to older servers so any bans/exempts/invex
 * will show up with their single letter syntax,
 * eg "MODE #test +b ~account:someacc" will be rewritten
 * as "MODE #test +b ~a:someacc".
 * It uses rather complex mode reparsing techniques to
 * achieve this, but this was deemed to be the only way
 * that we could achieve this in a doable way.
 * The alternative was complicating the mode.c code with
 * creating multiple strings for multiple clients, and
 * doing the same in any other MODE change routine.
 * That would have caused rather intrussive compatibility
 * code, so I don't want that.
 * With this we can just rip out the module at some point
 * that we no longer want to support pre-U6 protocol.
 * -- Syzop
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"unreal_server_compat",
	"1.0.0",
	"Provides compatibility with non-U6 servers",
	"Bram Matthys (Syzop)",
	"unrealircd-6"
    };

/* Forward declarations */
int usc_packet(Client *from, Client *to, Client *intended_to, char **msg, int *length);
int usc_reparsemode(char **msg, char *p, int *length);
void skip_spaces(char **p);
void read_until_space(char **p);
int eat_parameter(char **p);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_PACKET, 0, usc_packet);
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

int usc_packet(Client *from, Client *to, Client *intended_to, char **msg, int *length)
{
	char *p, *buf = *msg;

	/* We are only interested in outgoing data. Also ircops get to see everything as-is */
	if (IsMe(to) || !IsServer(to) || !buf || !length || !*length)
		return 0;

	buf[*length] = '\0'; /* safety */

	p = *msg;

	skip_spaces(&p);
	/* Skip over message tags */
	if (*p == '@')
	{
		read_until_space(&p);
		if (*p == '\0')
			return 0; /* unexpected ending */
		p++;
	}

	skip_spaces(&p);
	if (*p == '\0')
		return 0;

	/* Skip origin */
	if (*p == ':')
	{
		read_until_space(&p);
		if (*p == '\0')
			return 0; /* unexpected ending */
	}

	skip_spaces(&p);
	if (*p == '\0')
		return 0;

	if (!strncmp(p, "MODE ", 5))
	{
		read_until_space(&p);
		skip_spaces(&p);
		if (*p == '\0')
			return 0; /* unexpected */
		/* p now points to #channel */

		/* Now it gets interesting... we have to re-parse and re-write the entire MODE line. */
		return usc_reparsemode(msg, p, length);
	}

	return 0;
}

int usc_reparsemode(char **msg, char *p, int *length)
{
	static char obuf[8192];
	char modebuf[512], *mode_buf_p, *para_buf_p;
	char *channel_name;
	int i;
	int n;
	ParseMode pm;
	int modes_processed = 0;

	channel_name = p;
	if (!eat_parameter(&p))
		return 0;

	mode_buf_p = p;
	if (!eat_parameter(&p))
		return 0;
	*modebuf = '\0';
	strlncat(modebuf, mode_buf_p, sizeof(modebuf), p - mode_buf_p); // FIXME: verify that length calculation (last arg) is correct and doesnt need +1 or -1 etc.

	/* If we get here then it is (for example) a
	 * MODE #channel +b nick!user@host
	 * So, has at least one parameter (nick!user@host in the example).
	 * p now points exactly to the 'n' from nick!user@host.
	 *
	 * Now, what we will do:
	 * everything BEFORE p is the 'header' that we will
	 * send exactly as-is.
	 * The only thing we may (potentially) change is
	 * everything AFTER p!
	 */

	/* Fill 'obuf' with that 'header' */
	*obuf = '\0'; // we should really get strlncpy ;D
	strlncat(obuf, *msg, sizeof(obuf), p - *msg); // FIXME: verify that p-msg is correct and should not be -1 or +1 or anything :D
	para_buf_p = p;

	/* Now parse the modes */
	for (n = parse_chanmode(&pm, modebuf, para_buf_p); n; n = parse_chanmode(&pm, NULL, NULL))
	{
		/* We only rewrite the parameters, so don't care about paramless modes.. */
		if (!pm.param)
			continue;

		if ((pm.modechar == 'b') || (pm.modechar == 'e') || (pm.modechar == 'I'))
		{
			char *result = clean_ban_mask(pm.param, pm.what, NULL);
			strlcat(obuf, result?result:"<invalid>", sizeof(obuf));
			strlcat(obuf, " ", sizeof(obuf));
		} else
		{
			/* as-is */
			strlcat(obuf, pm.param, sizeof(obuf));
			strlcat(obuf, " ", sizeof(obuf));
		}
		modes_processed++;
	}

	/* Send line as-is */
	if (modes_processed == 0)
		return 0;

	/* Strip final whitespace */
	if (obuf[strlen(obuf)-1] == ' ')
		obuf[strlen(obuf)-1] = '\0';

	if (pm.parabuf && *pm.parabuf)
	{
		strlcat(obuf, " ", sizeof(obuf));
		strlcat(obuf, pm.parabuf, sizeof(obuf));
	}

	/* Add CRLF */
	if (obuf[strlen(obuf)-1] != '\n')
		strlcat(obuf, "\r\n", sizeof(obuf));

	/* Line modified, use it! */
	*msg = obuf;
	*length = strlen(obuf);

	return 0;
}

/** Skip space(s), if any. */
void skip_spaces(char **p)
{
	for (; **p == ' '; *p = *p + 1);
}

/** Keep reading until we hit space. */
void read_until_space(char **p)
{
	for (; **p && (**p != ' '); *p = *p + 1);
}

int eat_parameter(char **p)
{
	read_until_space(p);
	if (**p == '\0')
		return 0; /* was just a "MODE #channel" query - wait.. that's weird we are a server sending this :D */
	skip_spaces(p);
	if (**p == '\0')
		return 0; // impossible
	return 1;
}
