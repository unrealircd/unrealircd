/************************************************************************
 *   IRC - Internet Relay Chat, src/api-channelmode.c
 *   (C) 2003-2007 Bram Matthys (Syzop) and the UnrealIRCd Team
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

/** @file
 * @brief The channel mode API used by modules.
 */

#include "unrealircd.h"

/** This is the extended channel mode API,
 * see also https://www.unrealircd.org/docs/Dev:Channel_Mode_API
 * for more information.
 * @defgroup ChannelModeAPI Channel mode API
 * @{
 */

/** List of all channel modes, their handlers, etc */
Cmode *channelmodes = NULL;

/** @} */

/** Channel parameter to slot# mapping - used by GETPARAMSLOT() macro */
MODVAR unsigned char param_to_slot_mapping[256];
/** Extended channel modes in use - used by ISUPPORT/005 numeric only */
char extchmstr[4][64];

/* Private functions (forward declaration) and variables */
static void make_cmodestr(void);
static char previous_chanmodes[256];
static char previous_prefix[256];
static Cmode *ParamTable[MAXPARAMMODES+1];
static void unload_extcmode_commit(Cmode *cmode);

/** Create the strings that are used for CHANMODES=a,b,c,d in numeric 005 */
void make_extcmodestr()
{
	char *p;
	Cmode *cm;
	int i;
	
	extchmstr[0][0] = extchmstr[1][0] = extchmstr[2][0] = extchmstr[3][0] = '\0';
	
	/* type 1: lists (like b/e) */
	/* [NOT IMPLEMENTED IN EXTCMODES] */

	/* type 2: 1 par to set/unset (has .unset_with_param) */
	p = extchmstr[1];
	for (cm=channelmodes; cm; cm = cm->next)
		if (cm->paracount && cm->letter && cm->unset_with_param && (cm->type != CMODE_MEMBER))
			*p++ = cm->letter;
	*p = '\0';

	/* type 3: 1 param to set, 0 params to unset (does not have .unset_with_param) */
	p = extchmstr[2];
	for (cm=channelmodes; cm; cm = cm->next)
		if (cm->paracount && cm->letter && !cm->unset_with_param)
			*p++ = cm->letter;
	*p = '\0';
	
	/* type 4: paramless modes */
	p = extchmstr[3];
	for (cm=channelmodes; cm; cm = cm->next)
		if (!cm->paracount && cm->letter)
			*p++ = cm->letter;
	*p = '\0';
}

/** Create the string that is used in numeric 004 */
static void make_cmodestr(void)
{
	Cmode *cm;
	char *p = &cmodestring[0];
	CoreChannelModeTable *tab = &corechannelmodetable[0];
	int i;
	while (tab->mode != 0x0)
	{
		*p = tab->flag;
		p++;
		tab++;
	}
	for (cm=channelmodes; cm; cm = cm->next)
		if (cm->letter)
			*p++ = cm->letter;
	*p = '\0';
}

/** Check for changes - if any are detected, we broadcast the change */
void extcmodes_check_for_changed_channel_modes(void)
{
	char chanmodes[256];
	ISupport *isup;

	//sort_cmodes();
	make_cmodestr();
	make_extcmodestr();

	snprintf(chanmodes, sizeof(chanmodes), "%s%s", CHPAR1, EXPAR1);
	safe_strdup(me.server->features.chanmodes[0], chanmodes);
	snprintf(chanmodes, sizeof(chanmodes), "%s", EXPAR2);
	safe_strdup(me.server->features.chanmodes[1], chanmodes);
	snprintf(chanmodes, sizeof(chanmodes), "%s", EXPAR3);
	safe_strdup(me.server->features.chanmodes[2], chanmodes);
	snprintf(chanmodes, sizeof(chanmodes), "%s", EXPAR4);
	safe_strdup(me.server->features.chanmodes[3], chanmodes);

	ircsnprintf(chanmodes, sizeof(chanmodes), "%s,%s,%s,%s",
	            me.server->features.chanmodes[0],
	            me.server->features.chanmodes[1],
	            me.server->features.chanmodes[2],
	            me.server->features.chanmodes[3]);

	isup = ISupportFind("CHANMODES");
	if (!isup)
	{
		strlcpy(previous_chanmodes, chanmodes, sizeof(previous_chanmodes));
		return; /* not booted yet. then we are done here. */
	}
	
	ISupportSetValue(isup, chanmodes);
	
	if (*previous_chanmodes && strcmp(chanmodes, previous_chanmodes))
	{
		unreal_log(ULOG_INFO, "mode", "CHANNEL_MODES_CHANGED", NULL,
		           "Channel modes changed at runtime: $old_channel_modes -> $new_channel_modes",
		           log_data_string("old_channel_modes", previous_chanmodes),
		           log_data_string("new_channel_modes", chanmodes));
		/* Broadcast change to all (locally connected) servers */
		sendto_server(NULL, 0, 0, NULL, "PROTOCTL CHANMODES=%s", chanmodes);
	}

	strlcpy(previous_chanmodes, chanmodes, sizeof(previous_chanmodes));
}

void make_prefix(char **isupport_prefix, char **isupport_statusmsg)
{
	static char prefix[256];
	static char prefix_prefix[256];
	char prefix_modes[256];
	int rank[256];
	Cmode *cm;
	int n;

	*prefix = *prefix_prefix = *prefix_modes = '\0';

	for (n=0, cm=channelmodes; cm && n < ARRAY_SIZEOF(rank)-1; cm = cm->next)
	{
		if ((cm->type == CMODE_MEMBER) && cm->letter)
		{
			strlcat_letter(prefix_prefix, cm->prefix, sizeof(prefix_prefix));
			strlcat_letter(prefix_modes, cm->letter, sizeof(prefix_modes));
			rank[n] = cm->rank;
			n++;
		}
	}

	if (*prefix_prefix)
	{
		int i, j;
		/* Now sort the damn thing */
		for (i=0; i < n; i++)
		{
			for (j=i+1; j < n; j++)
			{
				if (rank[i] < rank[j])
				{
					/* swap */
					char save;
					int save_rank;
					save = prefix_prefix[i];
					prefix_prefix[i] = prefix_prefix[j];
					prefix_prefix[j] = save;
					save = prefix_modes[i];
					prefix_modes[i] = prefix_modes[j];
					prefix_modes[j] = save;
					save_rank = rank[i];
					rank[i] = rank[j];
					rank[j] = save_rank;
				}
			}
		}
		snprintf(prefix, sizeof(prefix), "(%s)%s", prefix_modes, prefix_prefix);
	}

	*isupport_prefix = prefix;
	*isupport_statusmsg = prefix_prefix;
}

void extcmodes_check_for_changed_prefixes(void)
{
	ISupport *isup;
	char *prefix, *statusmsg;

	make_prefix(&prefix, &statusmsg);
	ISupportSet(NULL, "PREFIX", prefix);
	ISupportSet(NULL, "STATUSMSG", statusmsg);

	if (*previous_prefix && strcmp(prefix, previous_prefix))
	{
		unreal_log(ULOG_INFO, "mode", "PREFIX_CHANGED", NULL,
		           "Prefix changed at runtime: $old_prefix -> $new_prefix",
		           log_data_string("old_prefix", previous_prefix),
		           log_data_string("new_prefix", prefix));
		/* Broadcast change to all (locally connected) servers */
		sendto_server(NULL, 0, 0, NULL, "PROTOCTL PREFIX=%s", prefix);
	}

	strlcpy(previous_prefix, prefix, sizeof(previous_prefix));
}

/** Check for changes - if any are detected, we broadcast the change */
void extcmodes_check_for_changes(void)
{
	extcmodes_check_for_changed_channel_modes();
	extcmodes_check_for_changed_prefixes();
}

/** Initialize the extended channel modes system */
void extcmode_init(void)
{
	memset(&extchmstr, 0, sizeof(extchmstr));
	memset(&param_to_slot_mapping, 0, sizeof(param_to_slot_mapping));
	*previous_chanmodes = '\0';
	*previous_prefix = '\0';
}

/** Update letter->slot mapping and slot->handler mapping */
void extcmode_para_addslot(Cmode *cm, int slot)
{
	if ((slot < 0) || (slot > MAXPARAMMODES))
		abort();
	cm->param_slot = slot;
	ParamTable[slot] = cm;
	param_to_slot_mapping[cm->letter] = slot;
}

/** Update letter->slot mapping and slot->handler mapping */
void extcmode_para_delslot(Cmode *cm, int slot)
{
	if ((slot < 0) || (slot > MAXPARAMMODES))
		abort();
	ParamTable[slot] = NULL;
	param_to_slot_mapping[cm->letter] = 0;
}

void channelmode_add_sorted(Cmode *n)
{
	Cmode *m;

	if (channelmodes == NULL)
	{
		channelmodes = n;
		return;
	}

	for (m = channelmodes; m; m = m->next)
	{
		if (m->letter == '\0')
			abort();
		if (sort_character_lowercase_before_uppercase(n->letter, m->letter))
		{
			/* Insert us before */
			if (m->prev)
				m->prev->next = n;
			else
				channelmodes = n; /* new head */
			n->prev = m->prev;

			n->next = m;
			m->prev = n;
			return;
		}
		if (!m->next)
		{
			/* Append us at end */
			m->next = n;
			n->prev = m;
			return;
		}
	}
}

/** @defgroup ChannelModeAPI Channel mode API
 * @{
 */

/** Register a new channel mode (Channel mode API).
 * @param module	The module requesting this channel mode (usually: modinfo->handle)
 * @param req		Details of the channel mode request
 * @param mode		Store the mode value (bit) here on success
 * @returns the newly created channel mode, or NULL in case of error.
 */
Cmode *CmodeAdd(Module *module, CmodeInfo req, Cmode_t *mode)
{
	int paraslot = -1;
	int existing = 0;
	Cmode *cm;

	for (cm=channelmodes; cm; cm = cm->next)
	{
		if (cm->letter == req.letter)
		{
			if (cm->unloaded)
			{
				cm->unloaded = 0;
				existing = 1;
				break;
			} else {
				if (module)
					module->errorcode = MODERR_EXISTS;
				return NULL;
			}
		}
	}

	if (!cm)
	{
		long l, found = 0;

		if (req.type == CMODE_NORMAL)
		{
			for (l = 1; l < LONG_MAX/2; l *= 2)
			{
				found = 0;
				for (cm=channelmodes; cm; cm = cm->next)
				{
					if (cm->mode == l)
					{
						found = 1;
						break;
					}
				}
				if (!found)
					break;
			}
			/* If 'found' is still true, then we are out of space */
			if (found)
			{
				unreal_log(ULOG_ERROR, "module", "CHANNEL_MODE_OUT_OF_SPACE", NULL,
					   "CmodeAdd: out of space!!!");
				if (module)
					module->errorcode = MODERR_NOSPACE;
				return NULL;
			}
			cm = safe_alloc(sizeof(Cmode));
			cm->letter = req.letter;
			cm->mode = l;
			*mode = cm->mode;
		} else if (req.type == CMODE_MEMBER)
		{
			if (!req.prefix || !req.sjoin_prefix || !req.paracount ||
			    !req.unset_with_param || !req.rank)
			{
				unreal_log(ULOG_ERROR, "module", "CMODEADD_API_ERROR", NULL,
					   "CmodeAdd(): module is missing required information. "
					   "Module: $module_name",
					   log_data_string("module_name", module->header->name));
				module->errorcode = MODERR_INVALID;
				return NULL;
			}
			cm = safe_alloc(sizeof(Cmode));
			cm->letter = req.letter;
		} else {
			abort();
		}
		channelmode_add_sorted(cm);
	}

	if ((req.paracount == 1) && (req.type == CMODE_NORMAL))
	{
		if (existing)
		{
			/* Re-use parameter slot of the module with the same modechar that is unloading */
			paraslot = cm->param_slot;
		}
		else
		{
			/* Allocate a new one */
			for (paraslot = 0; ParamTable[paraslot]; paraslot++)
			{
				if (paraslot == MAXPARAMMODES - 1)
				{
					unreal_log(ULOG_ERROR, "module", "CHANNEL_MODE_OUT_OF_SPACE", NULL,
						   "CmodeAdd: out of space!!! Place 2.");
					if (module)
						module->errorcode = MODERR_NOSPACE;
					return NULL;
				}
			}
		}
	}

	cm->letter = req.letter;
	cm->type = req.type;
	cm->prefix = req.prefix;
	cm->sjoin_prefix = req.sjoin_prefix;
	cm->rank = req.rank;
	cm->paracount = req.paracount;
	cm->is_ok = req.is_ok;
	cm->put_param = req.put_param;
	cm->get_param = req.get_param;
	cm->conv_param = req.conv_param;
	cm->free_param = req.free_param;
	cm->dup_struct = req.dup_struct;
	cm->sjoin_check = req.sjoin_check;
	cm->local = req.local;
	cm->unset_with_param = req.unset_with_param;
	cm->owner = module;
	cm->unloaded = 0;

	if (cm->type == CMODE_NORMAL)
	{
		*mode = cm->mode;
		if (cm->paracount == 1)
			extcmode_para_addslot(cm, paraslot);
	}

	if (module)
	{
		ModuleObject *cmodeobj = safe_alloc(sizeof(ModuleObject));
		cmodeobj->object.cmode = cm;
		cmodeobj->type = MOBJ_CMODE;
		AddListItem(cmodeobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return cm;
}

/** Delete a previously registered channel mode - not called by modules.
 * For modules this is done automatically on unload, no need to call this explicitly.
 */
void CmodeDel(Cmode *cmode)
{
	if (cmode->owner)
	{
		ModuleObject *cmodeobj;
		for (cmodeobj = cmode->owner->objects; cmodeobj; cmodeobj = cmodeobj->next) {
			if (cmodeobj->type == MOBJ_CMODE && cmodeobj->object.cmode == cmode) {
				DelListItem(cmodeobj, cmode->owner->objects);
				safe_free(cmodeobj);
				break;
			}
		}
		cmode->owner = NULL;
	}
	if (loop.rehashing)
		cmode->unloaded = 1;
	else
		unload_extcmode_commit(cmode);

}

/** @} */

/** After a channel mode is deregistered for sure, unload it completely.
 * This is done after a REHASH when no new module has registered the mode.
 * Then we can unload it for good. This also sends MODE -.. out etc.
 */
static void unload_extcmode_commit(Cmode *cmode)
{
	Channel *channel;

	if (!cmode)
		return;

	if (cmode->type == CMODE_NORMAL)
	{
		/* Unset channel mode and send MODE to everyone */
		if (cmode->paracount == 0)
		{
			/* Paramless mode, easy */
			for (channel = channels; channel; channel = channel->nextch)
			{
				if (channel->mode.mode & cmode->mode)
				{
					MessageTag *mtags = NULL;

					new_message(&me, NULL, &mtags);
					sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
						       ":%s MODE %s -%c",
						       me.name, channel->name, cmode->letter);
					sendto_server(NULL, 0, 0, mtags,
						":%s MODE %s -%c 0",
						me.id, channel->name, cmode->letter);
					free_message_tags(mtags);

					channel->mode.mode &= ~cmode->mode;
				}
			}
		} else
		{
			/* Parameter mode, more complicated */
			for (channel = channels; channel; channel = channel->nextch)
			{
				if (channel->mode.mode & cmode->mode)
				{
					MessageTag *mtags = NULL;

					new_message(&me, NULL, &mtags);
					if (cmode->unset_with_param)
					{
						const char *param = cmode->get_param(GETPARASTRUCT(channel, cmode->letter));
						sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
							       ":%s MODE %s -%c %s",
							       me.name, channel->name, cmode->letter, param);
						sendto_server(NULL, 0, 0, mtags,
							":%s MODE %s -%c %s 0",
							me.id, channel->name, cmode->letter, param);
					} else {
						sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
							       ":%s MODE %s -%c",
							       me.name, channel->name, cmode->letter);
						sendto_server(NULL, 0, 0, mtags,
							":%s MODE %s -%c 0",
							me.id, channel->name, cmode->letter);
					}
					free_message_tags(mtags);

					cmode->free_param(GETPARASTRUCT(channel, cmode->letter), 0);
					channel->mode.mode &= ~cmode->mode;
				}
			}
			extcmode_para_delslot(cmode, cmode->param_slot);
		}
	} else
	if (cmode->type == CMODE_MEMBER)
	{
		for (channel = channels; channel; channel = channel->nextch)
		{
			Member *m;
			for (m = channel->members; m; m = m->next)
			{
				if (strchr(m->member_modes, cmode->letter))
				{
					MessageTag *mtags = NULL;

					new_message(&me, NULL, &mtags);
					sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
						       ":%s MODE %s -%c %s",
						       me.name, channel->name, cmode->letter, m->client->name);
					sendto_server(NULL, 0, 0, mtags,
						":%s MODE %s -%c %s 0",
						me.id, channel->name, cmode->letter, m->client->id);
					free_message_tags(mtags);
					del_member_mode(m->client, channel, cmode->letter);
				}
			}
		}
	}

	DelListItem(cmode, channelmodes);
	safe_free(cmode);
}

/** Unload all unused channel modes after a REHASH */
void unload_all_unused_extcmodes(void)
{
	Cmode *cm, *cm_next;

	for (cm=channelmodes; cm; cm = cm_next)
	{
		cm_next = cm->next;
		if (cm->letter && cm->unloaded)
		{
			unload_extcmode_commit(cm);
		}
	}

}

/** @defgroup ChannelModeAPI Channel mode API
 * @{
 */

/** Get parameter for a channel mode.
 * @param channel	The channel
 * @param mode		The mode character (eg: 'f')
 */
const char *cm_getparameter(Channel *channel, char mode)
{
	return GETPARAMHANDLERBYLETTER(mode)->get_param(GETPARASTRUCT(channel, mode));
}

/** Get parameter for a channel mode - special version for SJOIN.
 * This version doesn't take a channel, but a mode.mode_params.
 * It is only used by SJOIN and should not be used in 3rd party modules.
 * @param p	The list, eg oldmode.mode_params
 * @param mode	The mode letter
 */
const char *cm_getparameter_ex(void **p, char mode)
{
	return GETPARAMHANDLERBYLETTER(mode)->get_param(GETPARASTRUCTEX(p, mode));
}

/** Set parameter for a channel mode.
 * @param channel	The channel
 * @param mode		The mode character (eg: 'f')
 * @param str		The parameter string
 * @note Module users should not use this function directly, it is only used by MODE and SJOIN.
 */
void cm_putparameter(Channel *channel, char mode, const char *str)
{
	GETPARASTRUCT(channel, mode) = GETPARAMHANDLERBYLETTER(mode)->put_param(GETPARASTRUCT(channel, mode), str);
}

/** Free a channel mode parameter.
 * @param channel	The channel
 * @param mode		The mode character (eg: 'f')
 */
void cm_freeparameter(Channel *channel, char mode)
{
	int n = GETPARAMHANDLERBYLETTER(mode)->free_param(GETPARASTRUCT(channel, mode), 1);
	if (n == 0)
		GETPARASTRUCT(channel, mode) = NULL;
}


/** Set parameter for a channel mode - special version for SJOIN.
 * This version doesn't take a channel, but a mode.mode_params.
 * It is only used by SJOIN and should not be used in 3rd party modules.
 * @param p	The list, eg oldmode.mode_params
 * @param mode	The mode letter
 * @param str	The mode parameter string to set
 */
void cm_putparameter_ex(void **p, char mode, const char *str)
{
	GETPARASTRUCTEX(p, mode) = GETPARAMHANDLERBYLETTER(mode)->put_param(GETPARASTRUCTEX(p, mode), str);
}

/** Default handler for - require channel operator or higher (+o/+a/+q)
 * @param client	The client issueing the MODE
 * @param channel	The channel
 * @param mode		The mode letter (eg: 'f')
 * @param para		The parameter, if any (can be NULL)
 * @param checkt	The check type, one of .....
 * @param what		MODE_ADD / MODE_DEL (???)
 * @returns EX_ALLOW or EX_DENY
 */
int extcmode_default_requirechop(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	if (IsUser(client) && check_channel_access(client, channel, "oaq"))
		return EX_ALLOW;
	if (checkt == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
		sendnumeric(client, ERR_NOTFORHALFOPS, mode);
	return EX_DENY;
}

/** Default handler for - require halfop or higher (+h/+o/+a/+q)
 * @param client	The client issueing the MODE
 * @param channel	The channel
 * @param mode		The mode letter (eg: 'f')
 * @param para		The parameter, if any (can be NULL)
 * @param checkt	The check type, one of .....
 * @param what		MODE_ADD / MODE_DEL (???)
 * @returns EX_ALLOW or EX_DENY
 */
int extcmode_default_requirehalfop(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	if (IsUser(client) && (check_channel_access(client, channel, "oaq") || check_channel_access(client, channel, "h")))
		return EX_ALLOW;
	return EX_DENY;
}

/** Duplicate all channel mode parameters - only used by SJOIN.
 * @param xi	Input list
 * @param xi	Output list
 */
void extcmode_duplicate_paramlist(void **xi, void **xo)
{
	int i;
	Cmode *handler;
	void *inx;

	for (i = 0; i < MAXPARAMMODES; i++)
	{
		handler = CMP_GETHANDLERBYSLOT(i);
		if (!handler)
			continue; /* nothing there.. */
		inx = xi[handler->param_slot]; /* paramter data of input is here */
		if (!inx)
			continue; /* not set */
		xo[handler->param_slot] = handler->dup_struct(inx); /* call dup_struct with that input and set the output param to that */
	}
}

/** Free all channel mode parameters - only used by SJOIN.
 * @param ar	The list
 */
void extcmode_free_paramlist(void **ar)
{
	int i;
	Cmode *handler;

	for (i = 0; i < MAXPARAMMODES; i++)
	{
		handler = GETPARAMHANDLERBYSLOT(i);
		if (!handler)
			continue; /* nothing here... */
		handler->free_param(ar[handler->param_slot], 0);
		ar[handler->param_slot] = NULL;
	}
}

/** @} */

/** Internal function: returns 1 if the specified module has 1 or more extended channel modes registered */
int module_has_extcmode_param_mode(Module *mod)
{
	Cmode *cm;

	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->letter) && (cm->owner == mod) && (cm->paracount))
			return 1;

	return 0;
}

/** Channel member privileges - getting, setting, checking vhoaq status, etc.
 * These functions get or set the access rights of channel members, such as +vhoaq.
 * They can also convert between modes (vhoaq), prefixes and sjoin prefixes.
 * @defgroup ChannelMember Channel members access privileges
 * @{
 */

/** Retrieve channel access for a user on a channel, returns modes eg "qao".
 * @param client	The client
 * @param channel	The channel
 * @returns The modes, sorted by high ranking to lower ranking, eg "qao".
 * An empty string ("") is returned when not in the channel or no modes.
 */
const char *get_channel_access(Client *client, Channel *channel)
{
	Membership *mb;

	mb = find_membership_link(client->user->channel, channel);
	if (!mb)
		return "";
	return mb->member_modes;
}

/** Check channel access for user.
 * @param client	The client to check
 * @param channel	The channel to check
 * @param modes		Which mode(s) to check for
 * @returns If the client in channel has any of the modes set, 1 is returned.
 * Otherwise 0 is returned, which is also the case if the user is
 * not a user or is not in the channel at all.
 */
int check_channel_access(Client *client, Channel *channel, const char *modes)
{
	Membership *mb;
	const char *p;

	if (!IsUser(client))
		return 0; /* eg server */

	mb = find_membership_link(client->user->channel, channel);
	if (!mb)
		return 0; /* not a member */

	for (p = mb->member_modes; *p; p++)
		if (strchr(modes, *p))
			return 1; /* match new style */

	return 0; /* nomatch */
}

/** Check channel access for user.
 * @param client	The client to check
 * @param channel	The channel to check
 * @param modes		Which mode(s) to check for
 * @returns If the client in channel has any of the modes set, 1 is returned.
 * Otherwise 0 is returned, which is also the case if the user is
 * not a user or is not in the channel at all.
 */
int check_channel_access_membership(Membership *mb, const char *modes)
{
	const char *p;

	if (!mb)
		return 0;

	for (p = mb->member_modes; *p; p++)
		if (strchr(modes, *p))
			return 1; /* match new style */

	return 0; /* nomatch */
}

/** Check channel access for user.
 * @param client	The client to check
 * @param channel	The channel to check
 * @param modes		Which mode(s) to check for
 * @returns If the client in channel has any of the modes set, 1 is returned.
 * Otherwise 0 is returned, which is also the case if the user is
 * not a user or is not in the channel at all.
 */
int check_channel_access_member(Member *mb, const char *modes)
{
	const char *p;

	if (!mb)
		return 0;

	for (p = mb->member_modes; *p; p++)
		if (strchr(modes, *p))
			return 1; /* match new style */

	return 0; /* nomatch */
}

/** Check channel access for user.
 * @param current	Flags currently set on the client (eg mb->member_modes)
 * @param modes		Which mode(s) to check for
 * @returns If the client in channel has any of the modes set, 1 is returned.
 * Otherwise 0 is returned.
 */
int check_channel_access_string(const char *current_modes, const char *modes)
{
	const char *p;

	for (p = current_modes; *p; p++)
		if (strchr(modes, *p))
			return 1;

	return 0; /* nomatch */
}

/** Check channel access for user.
 * @param current	Flags currently set on the client (eg mb->member_modes)
 * @param letter	Which mode letter to check for
 * @returns If the client in channel has any of the modes set, 1 is returned.
 * Otherwise 0 is returned.
 */
int check_channel_access_letter(const char *current_modes, const char letter)
{
	return strchr(current_modes, letter) ? 1 : 0;
}

Cmode *find_channel_mode_handler(char letter)
{
	Cmode *cm;

	for (cm=channelmodes; cm; cm = cm->next)
		if (cm->letter == letter)
			return cm;
	return NULL;
}

/** Is 'letter' a valid mode used for access/levels/ranks? (vhoaq and such)
 * @param letter	The channel mode letter to check, eg 'v'
 * @returns 1 if valid, 0 if the channel mode does not exist or is not a level mode.
 */
int valid_channel_access_mode_letter(char letter)
{
	Cmode *cm;

	if ((cm = find_channel_mode_handler(letter)) && (cm->type == CMODE_MEMBER))
		return 1;

	return 0;
}

void addlettertomstring(char *str, char letter)
{
	Cmode *cm;
	int n;
	int my_rank;
	char *p;

	if (!(cm = find_channel_mode_handler(letter)) || (cm->type != CMODE_MEMBER))
		return; // should we BUG on this? if something makes it this far, it can never be good right?

	my_rank = cm->rank;

	n = strlen(str);
	if (n >= MEMBERMODESLEN-1)
		return; // panic!

	for (p = str; *p; p++)
	{
		cm = find_channel_mode_handler(*p);
		if (!cm)
			continue; /* wtf */
		if (cm->rank < my_rank)
		{
			/* We need to insert us here */
			n = strlen(p);
			memmove(p+1, p, n+1); // +1 for NUL byte
			*p = letter;
			return;
		}
	}
	/* We should be at the end */
	str[n] = letter;
	str[n+1] = '\0';
}

void add_member_mode_fast(Member *mb, Membership *mbs, char letter)
{
	addlettertomstring(mb->member_modes, letter);
	addlettertomstring(mbs->member_modes, letter);
}

void del_member_mode_fast(Member *mb, Membership *mbs, char letter)
{
	delletterfromstring(mb->member_modes, letter);
	delletterfromstring(mbs->member_modes, letter);
}

int find_mbs(Client *client, Channel *channel, Member **mb, Membership **mbs)
{
	*mbs = NULL;

	if (!(*mb = find_member_link(channel->members, client)))
		return 0;

	if (!(*mbs = find_membership_link(client->user->channel, channel)))
		return 0;
	
	return 1;
}

void add_member_mode(Client *client, Channel *channel, char letter)
{
	Member *mb;
	Membership *mbs;

	if (!find_mbs(client, channel, &mb, &mbs))
		return;

	add_member_mode_fast(mb, mbs, letter);
}

void del_member_mode(Client *client, Channel *channel, char letter)
{
	Member *mb;
	Membership *mbs;

	if (!find_mbs(client, channel, &mb, &mbs))
		return;

	del_member_mode_fast(mb, mbs, letter);
}

char sjoin_prefix_to_mode(char s)
{
	Cmode *cm;

	/* Filter this out early to avoid spurious results */
	if (s == '\0')
		return '\0';

	/* First the hardcoded list modes: */
	if (s == '&')
		return 'b';
	if (s == '"')
		return 'e';
	if (s == '\'')
		return 'I';

	/* Now the dynamic ones (+vhoaq): */
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->sjoin_prefix == s) && (cm->type == CMODE_MEMBER))
			return cm->letter;

	/* Not found */
	return '\0';
}

char mode_to_sjoin_prefix(char s)
{
	Cmode *cm;

	/* Filter this out early to avoid spurious results */
	if (s == '\0')
		return '\0';

	/* First the hardcoded list modes: */
	if (s == 'b')
		return '&';
	if (s == 'e')
		return '"';
	if (s == 'I')
		return '\'';

	/* Now the dynamic ones (+vhoaq): */
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->letter == s) && (cm->type == CMODE_MEMBER))
			return cm->sjoin_prefix;

	/* Not found */
	return '\0';
}

const char *modes_to_sjoin_prefix(const char *modes)
{
	static char buf[MEMBERMODESLEN];
	const char *m;
	char f;

	*buf = '\0';
	for (m = modes; *m; m++)
	{
		f = mode_to_sjoin_prefix(*m);
		if (f)
			strlcat_letter(buf, f, sizeof(buf));
	}

	return buf;
}

char mode_to_prefix(char s)
{
	Cmode *cm;

	/* Filter this out early to avoid spurious results */
	if (s == '\0')
		return '\0';

	/* Now the dynamic ones (+vhoaq): */
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->letter == s) && (cm->type == CMODE_MEMBER))
			return cm->prefix;

	/* Not found */
	return '\0';
}

const char *modes_to_prefix(const char *modes)
{
	static char buf[MEMBERMODESLEN];
	const char *m;
	char f;

	*buf = '\0';
	for (m = modes; *m; m++)
	{
		f = mode_to_prefix(*m);
		if (f)
			strlcat_letter(buf, f, sizeof(buf));
	}

	return buf;
}

char prefix_to_mode(char s)
{
	Cmode *cm;

	/* Filter this out early to avoid spurious results */
	if (s == '\0')
		return '\0';

	/* Now the dynamic ones (+vhoaq): */
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->prefix == s) && (cm->type == CMODE_MEMBER))
			return cm->letter;

	/* Not found */
	return '\0';
}

char rank_to_mode(int rank)
{
	Cmode *cm;
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->type == CMODE_MEMBER) && (cm->rank == rank))
			return cm->letter;
	return '\0';
}

int mode_to_rank(char mode)
{
	Cmode *cm;
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->type == CMODE_MEMBER) && (cm->letter == mode))
			return cm->rank;
	return '\0';
}

int prefix_to_rank(char prefix)
{
	Cmode *cm;
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->type == CMODE_MEMBER) && (cm->prefix == prefix))
			return cm->rank;
	return '\0';
}

char rank_to_prefix(int rank)
{
	Cmode *cm;
	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->type == CMODE_MEMBER) && (cm->rank == rank))
			return cm->prefix;
	return '\0';
}

char lowest_ranking_prefix(const char *prefix)
{
	const char *p;
	int winning_rank = INT_MAX;

	for (p = prefix; *p; p++)
	{
		int rank = prefix_to_rank(*p);
		if (rank < winning_rank)
			winning_rank = rank;
	}
	if (winning_rank == INT_MAX)
		return '\0'; /* No result */
	return rank_to_prefix(winning_rank);
}

char lowest_ranking_mode(const char *mode)
{
	const char *p;
	int winning_rank = INT_MAX;

	for (p = mode; *p; p++)
	{
		int rank = mode_to_rank(*p);
		if (rank < winning_rank)
			winning_rank = rank;
	}
	if (winning_rank == INT_MAX)
		return '\0'; /* No result */
	return rank_to_mode(winning_rank);
}

/** Generate all member modes that are equal or greater than 'modes'.
 * Eg calling this with "o" would generate "oaq" with the default loaded modules.
 * This is used in sendto_channel() to make multiple check_channel_access_member()
 * calls more easy / faster.
 */
void channel_member_modes_generate_equal_or_greater(const char *modes, char *buf, size_t buflen)
{
	const char *p;
	int rank;
	Cmode *cm;

	*buf = '\0';

	/* First we must grab the lowest ranking mode, eg 'vhoaq' results in rank for 'v' */
	rank = lowest_ranking_mode(modes);
	if (!rank)
		return; /* zero matches */

	for (cm=channelmodes; cm; cm = cm->next)
	if ((cm->type == CMODE_MEMBER) && (cm->rank >= rank))
		strlcat_letter(buf, cm->letter, buflen);
}

/** @} */
