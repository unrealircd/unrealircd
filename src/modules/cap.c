/*
 *   IRC - Internet Relay Chat, src/modules/m_cap.c
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
	"unrealircd-5",
	};

/* Forward declarations */
int cap_is_handshake_finished(Client *acptr);
int cap_never_visible(Client *acptr);

/* Variables */
long CAP_IN_PROGRESS = 0L;
long CAP_ACCOUNT_NOTIFY = 0L;
long CAP_AWAY_NOTIFY = 0L;
long CAP_MULTI_PREFIX = 0L;
long CAP_USERHOST_IN_NAMES = 0L;
long CAP_NOTIFY = 0L;
long CAP_CHGHOST = 0L;
long CAP_EXTENDED_JOIN = 0L;

MOD_INIT()
{
	ClientCapabilityInfo c;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_CAP, cmd_cap, MAXPARA, M_UNREGISTERED|M_USER|M_NOLAG);

	/* This first cap is special, in the sense that it is hidden
	 * and indicates a cap exchange is in progress.
	 */
	memset(&c, 0, sizeof(c));
	c.name = "cap";
	c.visible = cap_never_visible;
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_IN_PROGRESS);

	memset(&c, 0, sizeof(c));
	c.name = "account-notify";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_ACCOUNT_NOTIFY);
	
	memset(&c, 0, sizeof(c));
	c.name = "away-notify";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_AWAY_NOTIFY);

	memset(&c, 0, sizeof(c));
	c.name = "multi-prefix";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_MULTI_PREFIX);

	memset(&c, 0, sizeof(c));
	c.name = "userhost-in-names";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_USERHOST_IN_NAMES);

	memset(&c, 0, sizeof(c));
	c.name = "cap-notify";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_NOTIFY);

	memset(&c, 0, sizeof(c));
	c.name = "chghost";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_CHGHOST);

	memset(&c, 0, sizeof(c));
	c.name = "extended-join";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_EXTENDED_JOIN);

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

static ClientCapability *clicap_find(Client *sptr, const char *data, int *negate, int *finished, int *errors)
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

	if(*p == '-')
	{
		*negate = 1;
		p++;

		/* someone sent a '-' without a parameter.. */
		if(*p == '\0')
			return NULL;
	}

	if((s = strchr(p, ' ')))
		*s++ = '\0';

	cap = ClientCapabilityFind(p, sptr);
	if (!s)
		*finished = 1;
	
	p = s; /* point to next token for next iteration */

	if (cap && (cap->flags & CLICAP_FLAGS_ADVERTISE_ONLY))
		cap = NULL;

	if (!cap)
		*errors = 1;

	return cap;
}

static void clicap_generate(Client *sptr, const char *subcmd, int flags)
{
	ClientCapability *cap;
	char buf[BUFSIZE];
	char capbuf[BUFSIZE];
	char *p;
	int buflen = 0;
	int curlen, mlen;

	mlen = snprintf(buf, BUFSIZE, ":%s CAP %s %s", me.name,	BadPtr(sptr->name) ? "*" : sptr->name, subcmd);

	p = capbuf;
	buflen = mlen;

	if (flags == -1)
	{
		sendto_one(sptr, NULL, "%s :", buf);
		return;
	}

	for (cap = clicaps; cap; cap = cap->next)
	{
		char name[256];
		char *param;

		if (cap->visible && !cap->visible(sptr))
			continue; /* hidden */

		if ((sptr->local->cap_protocol >= 302) && cap->parameter && (param = cap->parameter(sptr)))
			snprintf(name, sizeof(name), "%s=%s", cap->name, param);
		else
			strlcpy(name, cap->name, sizeof(name));

		if (flags)
		{
			if (!cap->cap || !CHECKPROTO(sptr, cap->cap))
				continue;
		}

		/* \r\n\0, possible "-~=", space, " *" */
		if (buflen + strlen(name) >= BUFSIZE - 10)
		{
			if (buflen != mlen)
				*(p - 1) = '\0';
			else
				*p = '\0';

			sendto_one(sptr, NULL, "%s * :%s", buf, capbuf);
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

	sendto_one(sptr, NULL, "%s :%s", buf, capbuf);
}

static int cap_end(Client *sptr, const char *arg)
{
	if (IsUser(sptr))
		return 0;

	ClearCapabilityFast(sptr, CAP_IN_PROGRESS);

	if (*sptr->name && sptr->user && *sptr->user->username && IsNotSpoof(sptr))
		return register_user(sptr, sptr, sptr->name, sptr->user->username, NULL, NULL, NULL);

	return 0;
}

static int cap_list(Client *sptr, const char *arg)
{
	clicap_generate(sptr, "LIST", sptr->local->caps ? sptr->local->caps : -1);
	return 0;
}

static int cap_ls(Client *sptr, const char *arg)
{
	if (!IsUser(sptr))
		SetCapabilityFast(sptr, CAP_IN_PROGRESS);

	if (arg)
		sptr->local->cap_protocol = atoi(arg);

	/* Since the client did a "CAP LS" it apparently supports CAP
	 * and thus at least protocol version 300.
	 */
	if (sptr->local->cap_protocol < 300)
		sptr->local->cap_protocol = 300;

	if (sptr->local->cap_protocol >= 302)
		SetCapabilityFast(sptr, CAP_NOTIFY); /* Implicit support (JIT) */

	clicap_generate(sptr, "LS", 0);
	return 0;
}

static int cap_req(Client *sptr, const char *arg)
{
	char buf[BUFSIZE];
	char pbuf[2][BUFSIZE];
	ClientCapability *cap;
	int buflen, plen;
	int i = 0;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;
	int errors = 0;

	if (!IsUser(sptr))
		SetCapabilityFast(sptr, CAP_IN_PROGRESS);

	if (BadPtr(arg))
		return 0;

	buflen = snprintf(buf, sizeof(buf), ":%s CAP %s ACK",
			  me.name, BadPtr(sptr->name) ? "*" : sptr->name);

	pbuf[0][0] = '\0';
	plen = 0;

	for(cap = clicap_find(sptr, arg, &negate, &finished, &errors); cap;
	    cap = clicap_find(sptr, NULL, &negate, &finished, &errors))
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
	if ((sptr->local->cap_protocol >= 302) && (capdel & CAP_NOTIFY))
		errors++; /* Reject "CAP REQ -cap-notify" */

	if (errors)
	{
		sendto_one(sptr, NULL, ":%s CAP %s NAK :%s", me.name, BadPtr(sptr->name) ? "*" : sptr->name, arg);
		return 0;
	}

	if (i)
	{
		sendto_one(sptr, NULL, "%s * :%s", buf, pbuf[0]);
		sendto_one(sptr, NULL, "%s :%s", buf, pbuf[1]);
	}
	else
		sendto_one(sptr, NULL, "%s :%s", buf, pbuf[0]);

	sptr->local->caps |= capadd;
	sptr->local->caps &= ~capdel;
	return 0;
}

struct clicap_cmd {
	const char *cmd;
	int (*func)(struct Client *source_p, const char *arg);
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

int cap_never_visible(Client *acptr)
{
	return 0;
}

/** Is our handshake done? */
int cap_is_handshake_finished(Client *acptr)
{
	if (HasCapabilityFast(acptr, CAP_IN_PROGRESS))
		return 0; /* We are in CAP LS stage, waiting for a CAP END */

	return 1;
}

CMD_FUNC(cmd_cap)
{
	struct clicap_cmd *cmd;

	if (!MyConnect(sptr))
		return 0;

	/* CAP is marked as "no fake lag" because we use custom fake lag rules:
	 * Only add a 1 second fake lag penalty if this is the XXth command.
	 * This will speed up connections considerably.
	 */
	if (sptr->local->receiveM > 15)
		cptr->local->since++;

	if (DISABLE_CAP)
	{
		/* I know nothing! */
		if (IsUser(sptr))
			sendnumeric(sptr, ERR_UNKNOWNCOMMAND, "CAP");
		else
			sendnumeric(sptr, ERR_NOTREGISTERED);
		return 0;
	}

	if (parc < 2)
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "CAP");

		return 0;
	}

	if(!(cmd = bsearch(parv[1], clicap_cmdtable,
			   sizeof(clicap_cmdtable) / sizeof(struct clicap_cmd),
			   sizeof(struct clicap_cmd), (bqcmp) clicap_cmd_search)))
	{
		sendnumeric(sptr, ERR_INVALIDCAPCMD, parv[1]);

		return 0;
	}

	return (cmd->func)(sptr, parv[2]);
}

