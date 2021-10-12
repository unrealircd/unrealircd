/*
 * Robust channel forwarding system
 * (C) Copyright 2019 Syzop, Gottem and the UnrealIRCd team
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

#define MAX_EB_LEN 128 // Max extban length

ModuleHeader MOD_HEADER = {
	"chanmodes/link",
	"5.0",
	"Channel Mode +L",
	"UnrealIRCd Team",
	"unrealircd-5",
};

Cmode_t EXTMODE_LINK = 0L;

typedef struct {
	char linked[CHANNELLEN + 1];
} aModeLEntry;

typedef enum {
	LINKTYPE_BAN = 1, // +b
	LINKTYPE_INVITE = 2, // +i
	LINKTYPE_OPER = 3, // +O
	LINKTYPE_SSL = 4, // +z
	LINKTYPE_REG = 5, // +R
	LINKTYPE_LIMIT = 6, // +l
	LINKTYPE_BADKEY = 7, // +k
} linkType;

int cmodeL_is_ok(Client *client, Channel *channel, char mode, char *para, int type, int what);
void *cmodeL_put_param(void *r_in, char *param);
char *cmodeL_get_param(void *r_in);
char *cmodeL_conv_param(char *param_in, Client *client, Channel *channel);
void cmodeL_free_param(void *r);
void *cmodeL_dup_struct(void *r_in);
int cmodeL_sjoin_check(Channel *channel, void *ourx, void *theirx);

int extban_link_syntax(Client *client, int checkt, char *reason);
int extban_link_is_ok(Client *client, Channel *channel, char *param, int checkt, int what, int what2);
char *extban_link_conv_param(char *param);
int extban_link_is_banned(Client *client, Channel *channel, char *ban, int type, char **msg, char **errmsg);
int link_doforward(Client *client, Channel *channel, char *linked, linkType linktype);
int link_pre_localjoin_cb(Client *client, Channel *channel, char *parv[]);

MOD_INIT()
{
	CmodeInfo req;
	ExtbanInfo req_extban;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 1;
	req.is_ok = cmodeL_is_ok;
	req.flag = 'L';
	req.unset_with_param = 1; /* Oh yeah, we are special! */
	req.put_param = cmodeL_put_param;
	req.get_param = cmodeL_get_param;
	req.conv_param = cmodeL_conv_param;
	req.free_param = cmodeL_free_param;
	req.dup_struct = cmodeL_dup_struct;
	req.sjoin_check = cmodeL_sjoin_check;
	CmodeAdd(modinfo->handle, req, &EXTMODE_LINK);

	memset(&req_extban, 0, sizeof(ExtbanInfo));
	req_extban.flag = 'f';
	req_extban.is_ok = extban_link_is_ok;
	req_extban.conv_param = extban_link_conv_param;
	req_extban.is_banned = extban_link_is_banned;
	req_extban.options = EXTBOPT_ACTMODIFIER;
	if (!ExtbanAdd(modinfo->handle, req_extban))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_JOIN, -99, link_pre_localjoin_cb);
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

int cmodeL_is_ok(Client *client, Channel *channel, char mode, char *para, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsUser(client) && is_chan_op(client, channel))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendnumeric(client, ERR_NOTFORHALFOPS, 'H');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		/* Check parameter.. syntax is +L #channel */
		if (strchr(para, ','))
			return EX_DENY; /* multiple channels not permitted */
		if (!valid_channelname(para))
		{
			if (MyUser(client))
				sendnumeric(client, ERR_NOSUCHCHANNEL, para);
			return EX_DENY;
		}

		if (find_channel(para, NULL) == channel)
		{
			if (MyUser(client))
				sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'L',
					   "a channel cannot be linked to itself");
			return EX_DENY;
		}
		return EX_ALLOW;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

void *cmodeL_put_param(void *r_in, char *param)
{
	aModeLEntry *r = (aModeLEntry *)r_in;

	if (!r)
	{
		/* Need to create one */
		r = safe_alloc(sizeof(aModeLEntry));
	}
	strlcpy(r->linked, param, sizeof(r->linked));
	return (void *)r;
}

char *cmodeL_get_param(void *r_in)
{
	aModeLEntry *r = (aModeLEntry *)r_in;
	static char retbuf[CHANNELLEN+1];

	if (!r)
		return NULL;

	strlcpy(retbuf, r->linked, sizeof(retbuf));
	return retbuf;
}

/** Convert parameter to something proper.
 * NOTE: client may be NULL
 */
char *cmodeL_conv_param(char *param, Client *client, Channel *channel)
{
	char *p;

	if (!valid_channelname(param))
		return NULL;

	return param;
}

void cmodeL_free_param(void *r)
{
	safe_free(r);
}

void *cmodeL_dup_struct(void *r_in)
{
	aModeLEntry *r = (aModeLEntry *)r_in;
	aModeLEntry *w = safe_alloc(sizeof(aModeLEntry));

	memcpy(w, r, sizeof(aModeLEntry));
	return (void *)w;
}

int cmodeL_sjoin_check(Channel *channel, void *ourx, void *theirx)
{
	aModeLEntry *our = (aModeLEntry *)ourx;
	aModeLEntry *their = (aModeLEntry *)theirx;

	if (!strcmp(our->linked, their->linked))
		return EXSJ_SAME;
	if (strcmp(our->linked, their->linked) > 0)
		return EXSJ_WEWON;
	return EXSJ_THEYWON;
}

int extban_link_syntax(Client *client, int checkt, char *reason)
{
	if (MyUser(client) && (checkt == EXBCHK_PARAM))
	{
		sendnotice(client, "Error when setting ban: %s", reason);
		sendnotice(client, "  Syntax: +b ~f:#channel:mask");
		sendnotice(client, "Examples:");
		sendnotice(client, "  +b ~f:#public:*!*@badisp.org");
		sendnotice(client, "  +b ~f:#public:~c:#badchannel");
		sendnotice(client, "Multiple channels are not supported");
		sendnotice(client, "Valid masks are nick!user@host or another extban type such as ~a, ~c, ~S, etc");
	}
	return 0; // Reject ban
}

int extban_link_is_ok(Client *client, Channel *channel, char *param, int checkt, int what, int what2)
{
	char paramtmp[MAX_EB_LEN + 1];
	char tmpmask[MAX_EB_LEN + 1];
	char *matchby; // Matching method, such as 'n!u@h'
	char *chan;

	// Always permit deletion
	if (what == MODE_DEL)
		return 1;

	if (what2 != EXBTYPE_BAN)
	{
		if (checkt == EXBCHK_PARAM)
			sendnotice(client, "Ban type ~f only works with bans (+b) and not with exceptions or invex (+e/+I)");
		return 0; // Reject
	}

	strlcpy(paramtmp, param + 3, sizeof(paramtmp)); // Work on a size-truncated copy
	chan = paramtmp;
	matchby = strchr(paramtmp, ':');
	if (!matchby || !matchby[1])
		return extban_link_syntax(client, checkt, "Invalid syntax");
	*matchby++ = '\0';

	if (*chan != '#' || strchr(param, ','))
		return extban_link_syntax(client, checkt, "Invalid channel");

	// Possibly stack multiple extbans, this is a little convoluted due to extban API limitations
	snprintf(tmpmask, sizeof(tmpmask), "~?:%s", matchby);
	if (extban_is_ok_nuh_extban(client, channel, tmpmask, checkt, what, what2) == 0)
		return extban_link_syntax(client, checkt, "Invalid matcher");

	return 1; // Is ok
}

char *extban_link_conv_param(char *param)
{
	static char retbuf[MAX_EB_LEN + 1];
	char paramtmp[MAX_EB_LEN + 1];
	char tmpmask[MAX_EB_LEN + 1];
	char *matchby; // Matching method, such as 'n!u@h'
	char *newmask; // Cleaned matching method, such as 'n!u@h'
	char *chan;

	strlcpy(paramtmp, param + 3, sizeof(paramtmp)); // Work on a size-truncated copy
	chan = paramtmp;
	matchby = strchr(paramtmp, ':');
	if (!matchby || !matchby[1])
		return NULL;
	*matchby++ = '\0';

	if (!valid_channelname(chan))
		return NULL;

	// Possibly stack multiple extbans, this is a little convoluted due to extban API limitations
	snprintf(tmpmask, sizeof(tmpmask), "~?:%s", matchby);
	newmask = extban_conv_param_nuh_or_extban(tmpmask);
	if (!newmask || (strlen(newmask) <= 3))
		return NULL;

	snprintf(retbuf, sizeof(retbuf), "~f:%s:%s", chan, newmask + 3);
	return retbuf;
}

int extban_link_is_banned(Client *client, Channel *channel, char *ban, int type, char **msg, char **errmsg)
{
	// We don't actually ban here because we have to extract the channel name in PRE_LOCAL_JOIN anyways
	return 0;
}

int link_doforward(Client *client, Channel *channel, char *linked, linkType type)
{
	char desc[64];
	char *parv[3];

	switch (type)
	{
		case LINKTYPE_BAN:
			strncpy(desc, "you are banned", sizeof(desc));
			break;

		case LINKTYPE_INVITE:
			strncpy(desc, "channel is invite only", sizeof(desc));
			break;

		case LINKTYPE_OPER:
			strncpy(desc, "channel is oper only", sizeof(desc));
			break;

		case LINKTYPE_SSL:
			strncpy(desc, "channel requires SSL", sizeof(desc));
			break;

		case LINKTYPE_REG:
			strncpy(desc, "channel requires registration", sizeof(desc));
			break;

		case LINKTYPE_LIMIT:
			strncpy(desc, "channel has become full", sizeof(desc));
			break;

		case LINKTYPE_BADKEY:
			strncpy(desc, "invalid channel key", sizeof(desc));
			break;

		default:
			strncpy(desc, "no reason specified", sizeof(desc));
			break;
	}

	sendto_one(client, NULL,
	           ":%s %d %s %s %s :[Link] Cannot join channel %s (%s) -- transferring you to %s",
	           me.name, ERR_LINKCHANNEL, client->name, channel->chname, linked,
	           channel->chname, desc, linked);
	parv[0] = client->name;
	parv[1] = linked;
	parv[2] = NULL;
	do_join(client, 2, parv);
	return HOOK_DENY; // Original channel join = ignored
}

int link_pre_localjoin_cb(Client *client, Channel *channel, char *parv[])
{
	char *linked;
	int canjoin;
	Ban *ban;
	char bantmp[MAX_EB_LEN + 1];
	char *banchan;
	char *banmask;

	// User might already be on this channel, let's also exclude any possible services bots early
	if (IsULine(client) || find_membership_link(client->user->channel, channel))
		return HOOK_CONTINUE;

	// Extbans take precedence over +L #channel and other restrictions,
	// only /INVITE from chanop bypasses:
	if (!is_invited(client, channel))
	{
		for(ban = channel->banlist; ban; ban = ban->next)
		{
			if (!strncmp(ban->banstr, "~f:", 3))
			{
				strlcpy(bantmp, ban->banstr + 3, sizeof(bantmp));
			} else
			if (!strncmp(ban->banstr, "~t:", 3))
			{
				/* A timed ban, but is it for us? Need to parse a little:
				 * ~t:dddd:~f:...
				 */
				char *p = strchr(ban->banstr + 3, ':');
				if (p && !strncmp(p, ":~f:", 4))
				{
					strlcpy(bantmp, p + 4, sizeof(bantmp));
				} else {
					/* Not for us - some other ~t ban */
					continue;
				}
			} else
			{
				/* Not for us */
				continue;
			}
			banchan = bantmp;
			banmask = strchr(bantmp, ':');
			if (!banmask || !banmask[1])
				continue;
			*banmask++ = '\0';

			if (ban_check_mask(client, channel, banmask, BANCHK_JOIN, NULL, NULL, 0))
				return link_doforward(client, channel, banchan, LINKTYPE_BAN);
		}
	}

	// Either +L is not set, or it is set but the parameter isn't stored somehow
	if (!(channel->mode.extmode & EXTMODE_LINK) || !(linked = cm_getparameter(channel, 'L')))
		return HOOK_CONTINUE;

	// can_join() actually returns 0 if we *can* join a channel, so we don't need to bother checking any further conditions
	if (!(canjoin = can_join(client, channel, parv[2], parv)))
		return HOOK_CONTINUE;

	// Oper only channel
	if (has_channel_mode(channel, 'O') && !IsOper(client))
		return link_doforward(client, channel, linked, LINKTYPE_OPER);

	// SSL/TLS connected users only
	if (has_channel_mode(channel, 'z') && !IsSecureConnect(client))
		return link_doforward(client, channel, linked, LINKTYPE_SSL);

	// Registered/identified users only
	if (has_channel_mode(channel, 'R') && !IsRegNick(client))
		return link_doforward(client, channel, linked, LINKTYPE_REG);

	// For a couple of conditions we can use the return value from can_join() =]
	switch(canjoin) {
		// Any ban other than our own ~f: extban
		case ERR_BANNEDFROMCHAN:
			return link_doforward(client, channel, linked, LINKTYPE_BAN);

		// Invite only
		case ERR_INVITEONLYCHAN:
			return link_doforward(client, channel, linked, LINKTYPE_INVITE);

		// User limit
		case ERR_CHANNELISFULL:
			return link_doforward(client, channel, linked, LINKTYPE_LIMIT);

		// Specified channel key doesn't match
		case ERR_BADCHANNELKEY:
			return link_doforward(client, channel, linked, LINKTYPE_BADKEY);

		default:
			break;
	}

	// Let any other errors be handled by their respective modules
	return HOOK_CONTINUE;
}
