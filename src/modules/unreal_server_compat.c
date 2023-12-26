/*
 * IRC - Internet Relay Chat, src/modules/unreal_server_compat.c
 * unreal_server_compat - Compatibility with pre-U6 servers
 * (C) Copyright 2016-2021 Bram Matthys (Syzop)
 * License: GPLv2 or later
 *
 * Currently the only purpose of this module is to rewrite MODE
 * and SJOIN lines to older servers so any bans/exempts/invex
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
 * For SJOIN we do something similar, though in that case
 * it would have been quite doable to handle it in there.
 * Just figured I would stuff it in here as well, since
 * it is basically the same case.
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
int usc_reparse_mode(char **msg, char *p, int *length);
int usc_reparse_sjoin(char **msg, char *p, int *length);
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

	/* We are only interested in outgoing servers
	 * that do not support PROTOCTL NEXTBANS
	 */
	if (IsMe(to) || !IsServer(to) || SupportNEXTBANS(to) || !buf || !length || !*length)
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

	if (!strncmp(p, "MODE ", 5)) /* MODE #channel */
	{
		if (!eat_parameter(&p))
			return 0;
		/* p now points to #channel */

		/* Now it gets interesting... we have to re-parse and re-write the entire MODE line. */
		return usc_reparse_mode(msg, p, length);
	}

	if (!strncmp(p, "SJOIN ", 6)) /* SJOIN timestamp #channel */
	{
		if (!eat_parameter(&p) || !eat_parameter(&p))
			return 0;
		/* p now points to #channel */

		/* Now it gets interesting... we have to re-parse and re-write the entire SJOIN line. */
		return usc_reparse_sjoin(msg, p, length);
	}

	return 0;
}

int usc_reparse_mode(char **msg, char *p, int *length)
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
	strlncpy(modebuf, mode_buf_p, sizeof(modebuf), p - mode_buf_p);

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
	strlncpy(obuf, *msg, sizeof(obuf), p - *msg);
	para_buf_p = p;

	/* Now parse the modes */
	for (n = parse_chanmode(&pm, modebuf, para_buf_p); n; n = parse_chanmode(&pm, NULL, NULL))
	{
		/* We only rewrite the parameters, so don't care about paramless modes.. */
		if (!pm.param)
			continue;

		if ((pm.modechar == 'b') || (pm.modechar == 'e') || (pm.modechar == 'I'))
		{
			const char *result = clean_ban_mask(pm.param, pm.what, &me, 1);
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

int usc_reparse_sjoin(char **msg, char *p, int *length)
{
	static char obuf[8192];
	char parabuf[512];
	char *save = NULL;
	char *s;

	/* Skip right to the last parameter, the only one we care about */
	p = strstr(p, " :");
	if (!p)
		return 0;
	p += 2;

	/* Save everything before p, put it in obuf... */

	/* Fill 'obuf' with that 'header' */
	strlncpy(obuf, *msg, sizeof(obuf), p - *msg);

	/* Put parameters in parabuf so we can trash it :D */
	strlcpy(parabuf, p, sizeof(parabuf));

	/* Now parse the SJOIN */
	for (s = strtoken(&save, parabuf, " "); s; s = strtoken(&save, NULL, " "))
	{
		if (*s == '<')
		{
			/* SJSBY */
			char *next = strchr(s, '>');
			const char *result;
			if (!next)
			{
				unreal_log(ULOG_WARNING, "unreal_server_compat", "USC_REPARSE_SJOIN_FAILURE", NULL,
				           "[unreal_server_compat] usc_reparse_sjoin(): sjoin data '$ban' seemed like a SJSBY but was not??",
				           log_data_string("ban", s));
				continue;
			}
			if (!strchr("&\"\\", next[1]))
				goto fallback_usc_reparse_sjoin;
			*next++ = '\0';
			result = clean_ban_mask(next+1, MODE_ADD, &me, 1);
			if (!result)
			{
				unreal_log(ULOG_WARNING, "unreal_server_compat", "USC_REPARSE_SJOIN_FAILURE", NULL,
				           "[unreal_server_compat] usc_reparse_sjoin(): ban '$ban' could not be converted",
				           log_data_string("ban", s+1));
				continue;
			}
			strlcat(obuf, s, sizeof(obuf)); /* "<123,nick" */
			strlcat(obuf, ">", sizeof(obuf)); /* > */
			strlncat(obuf, next, sizeof(obuf), 1); /* & or \" or \\ */
			strlcat(obuf, result, sizeof(obuf)); /* the converted result */
			strlcat(obuf, " ", sizeof(obuf));
		} else
		if (strchr("&\"\\", *s))
		{
			/* +b / +e / +I */
			const char *result = clean_ban_mask(s+1, MODE_ADD, &me, 1);
			if (!result)
			{
				unreal_log(ULOG_WARNING, "unreal_server_compat", "USC_REPARSE_SJOIN_FAILURE", NULL,
				           "[unreal_server_compat] usc_reparse_sjoin(): ban '$ban' could not be converted",
				           log_data_string("ban", s+1));
				continue;
			}
			strlncat(obuf, s, sizeof(obuf), 1);
			strlcat(obuf, result, sizeof(obuf));
			strlcat(obuf, " ", sizeof(obuf));
		} else {
fallback_usc_reparse_sjoin:
			strlcat(obuf, s, sizeof(obuf));
			strlcat(obuf, " ", sizeof(obuf));
		}
	}

	/* Strip final whitespace */
	if (obuf[strlen(obuf)-1] == ' ')
		obuf[strlen(obuf)-1] = '\0';

	/* Add CRLF */
	if (obuf[strlen(obuf)-1] != '\n')
		strlcat(obuf, "\r\n", sizeof(obuf));

	/* And use it! */
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
