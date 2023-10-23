/*
 *   IRC - Internet Relay Chat, src/modules/cap.c
 *   (C) 2012 The UnrealIRCd Team
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

typedef int (*bqcmp)(const void *, const void *);

CMD_FUNC(cmd_cap);

#define MSG_CAP 	"CAP"

ModuleHeader MOD_HEADER
  = {
	"cap",	/* Name of module */
	"5.0", /* Version */
	"command /cap", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Forward declarations */
int cap_is_handshake_finished(Client *client);
int cap_never_visible(Client *client);

/* Variables */
long CAP_IN_PROGRESS = 0L;
long CAP_NOTIFY = 0L;

MOD_INIT()
{
	ClientCapabilityInfo c;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_CAP, cmd_cap, MAXPARA, CMD_UNREGISTERED|CMD_USER|CMD_NOLAG);

	/* This first cap is special, in the sense that it is hidden
	 * and indicates a cap exchange is in progress.
	 */
	memset(&c, 0, sizeof(c));
	c.name = "cap";
	c.visible = cap_never_visible;
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_IN_PROGRESS);

	memset(&c, 0, sizeof(c));
	c.name = "cap-notify";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_NOTIFY);

	HookAdd(modinfo->handle, HOOKTYPE_IS_HANDSHAKE_FINISHED, 0, cap_is_handshake_finished);

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

static ClientCapability *clicap_find(Client *client, const char *data, int *negate, int *finished, int *errors)
{
	static char buf[BUFSIZE];
	static char *p;
	ClientCapability *cap;
	char *s;

	*negate = 0;

	if (data)
	{
		strlcpy(buf, data, sizeof(buf));
		p = buf;
		*finished = 0;
		*errors = 0;
	}

	if (*finished)
		return NULL;

	/* skip any whitespace */
	while(*p && isspace(*p))
		p++;

	if (BadPtr(p))
	{
		*finished = 1;
		return NULL;
	}

	if (*p == '-')
	{
		*negate = 1;
		p++;

		/* someone sent a '-' without a parameter.. */
		if (*p == '\0')
			return NULL;
	}

	if ((s = strchr(p, ' ')))
		*s++ = '\0';

	cap = ClientCapabilityFind(p, client);
	if (!s)
		*finished = 1;
	
	p = s; /* point to next token for next iteration */

	if (cap && (cap->flags & CLICAP_FLAGS_ADVERTISE_ONLY))
		cap = NULL;

	if (!cap)
		*errors = 1;

	return cap;
}

static void clicap_generate(Client *client, const char *subcmd, int flags)
{
	ClientCapability *cap;
	char buf[BUFSIZE];
	char capbuf[BUFSIZE];
	char *p;
	int buflen = 0;
	int curlen, mlen;

	mlen = snprintf(buf, BUFSIZE, ":%s CAP %s %s", me.name,	BadPtr(client->name) ? "*" : client->name, subcmd);

	p = capbuf;
	buflen = mlen;

	if (flags == -1)
	{
		sendto_one(client, NULL, "%s :", buf);
		return;
	}

	for (cap = clicaps; cap; cap = cap->next)
	{
		char name[256];
		const char *param;

		if (cap->visible && !cap->visible(client))
			continue; /* hidden */

		if (flags)
		{
			if (!cap->cap || !(client->local->caps & cap->cap))
				continue;
		}

		if ((client->local->cap_protocol >= 302) && cap->parameter && (param = cap->parameter(client)))
			snprintf(name, sizeof(name), "%s=%s", cap->name, param);
		else
			strlcpy(name, cap->name, sizeof(name));

		/* \r\n\0, possible "-~=", space, " *" */
		if (buflen + strlen(name) >= BUFSIZE - 10)
		{
			if (buflen != mlen)
				*(p - 1) = '\0';
			else
				*p = '\0';

			sendto_one(client, NULL, "%s * :%s", buf, capbuf);
			p = capbuf;
			buflen = mlen;
		}

		curlen = snprintf(p, (capbuf + BUFSIZE) - p, "%s ", name);
		p += curlen;
		buflen += curlen;
	}

	if (buflen != mlen)
		*(p - 1) = '\0';
	else
		*p = '\0';

	sendto_one(client, NULL, "%s :%s", buf, capbuf);
}

static void cap_end(Client *client, const char *arg)
{
	if (IsUser(client))
		return;

	ClearCapabilityFast(client, CAP_IN_PROGRESS);

	if (is_handshake_finished(client))
		register_user(client);
}

static void cap_list(Client *client, const char *arg)
{
	clicap_generate(client, "LIST", client->local->caps ? client->local->caps : -1);
}

static void cap_ls(Client *client, const char *arg)
{
	if (!IsUser(client))
		SetCapabilityFast(client, CAP_IN_PROGRESS);

	if (arg)
		client->local->cap_protocol = atoi(arg);

	/* Since the client did a "CAP LS" it apparently supports CAP
	 * and thus at least protocol version 300.
	 */
	if (client->local->cap_protocol < 300)
		client->local->cap_protocol = 300;

	if (client->local->cap_protocol >= 302)
		SetCapabilityFast(client, CAP_NOTIFY); /* Implicit support (JIT) */

	clicap_generate(client, "LS", 0);
}

static void cap_req(Client *client, const char *arg)
{
	char buf[BUFSIZE];
	char pbuf[2][BUFSIZE];
	ClientCapability *cap;
	int buflen, plen;
	int i = 0;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;
	int errors = 0;

	if (!IsUser(client))
		SetCapabilityFast(client, CAP_IN_PROGRESS);

	if (BadPtr(arg))
		return;

	buflen = snprintf(buf, sizeof(buf), ":%s CAP %s ACK",
			  me.name, BadPtr(client->name) ? "*" : client->name);

	pbuf[0][0] = '\0';
	plen = 0;

	for(cap = clicap_find(client, arg, &negate, &finished, &errors); cap;
	    cap = clicap_find(client, NULL, &negate, &finished, &errors))
	{
		/* filled the first array, but cant send it in case the
		 * request fails.  one REQ should never fill more than two
		 * buffers --fl
		 */
		if (buflen + plen + strlen(cap->name) + 6 >= BUFSIZE)
		{
			pbuf[1][0] = '\0';
			plen = 0;
			i = 1;
		}

		if (negate)
		{
			strcat(pbuf[i], "-");
			plen++;

			capdel |= cap->cap;
		}
		else
		{
			capadd |= cap->cap;
		}

		strcat(pbuf[i], cap->name);
		strcat(pbuf[i], " ");
		plen += (strlen(cap->name) + 1);
	}

	/* This one is special */
	if ((client->local->cap_protocol >= 302) && (capdel & CAP_NOTIFY))
		errors++; /* Reject "CAP REQ -cap-notify" */

	if (errors)
	{
		sendto_one(client, NULL, ":%s CAP %s NAK :%s", me.name, BadPtr(client->name) ? "*" : client->name, arg);
		return;
	}

	if (i)
	{
		sendto_one(client, NULL, "%s * :%s", buf, pbuf[0]);
		sendto_one(client, NULL, "%s :%s", buf, pbuf[1]);
	}
	else
		sendto_one(client, NULL, "%s :%s", buf, pbuf[0]);

	client->local->caps |= capadd;
	client->local->caps &= ~capdel;
}

struct clicap_cmd {
	const char *cmd;
	void (*func)(Client *source_p, const char *arg);
};

static struct clicap_cmd clicap_cmdtable[] = {
	{ "END",	cap_end		},
	{ "LIST",	cap_list	},
	{ "LS",		cap_ls		},
	{ "REQ",	cap_req		},
};

static int clicap_cmd_search(const char *command, struct clicap_cmd *entry)
{
	return strcasecmp(command, entry->cmd);
}

int cap_never_visible(Client *client)
{
	return 0;
}

/** Is our handshake done? */
int cap_is_handshake_finished(Client *client)
{
	if (HasCapabilityFast(client, CAP_IN_PROGRESS))
		return 0; /* We are in CAP LS stage, waiting for a CAP END */

	return 1;
}

CMD_FUNC(cmd_cap)
{
	struct clicap_cmd *cmd;

	if (!MyConnect(client))
		return;

	/* CAP is marked as "no fake lag" because we use custom fake lag rules:
	 * Only add a 1 second fake lag penalty if this is the XXth command.
	 * This will speed up connections considerably.
	 */
	if (client->local->traffic.messages_received > 15)
		add_fake_lag(client, 1000);

	if (DISABLE_CAP)
	{
		/* I know nothing! */
		if (IsUser(client))
			sendnumeric(client, ERR_UNKNOWNCOMMAND, "CAP");
		else
			sendnumeric(client, ERR_NOTREGISTERED);
		return;
	}

	if (parc < 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "CAP");

		return;
	}

	if (!(cmd = bsearch(parv[1], clicap_cmdtable,
			   sizeof(clicap_cmdtable) / sizeof(struct clicap_cmd),
			   sizeof(struct clicap_cmd), (bqcmp) clicap_cmd_search)))
	{
		sendnumeric(client, ERR_INVALIDCAPCMD, parv[1]);

		return;
	}

	cmd->func(client, parv[2]);
}
