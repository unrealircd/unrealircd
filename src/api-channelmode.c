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

/** Table with details on each channel mode handler */
Cmode *Channelmode_Table = NULL;
/** Highest index in Channelmode_Table */
unsigned short Channelmode_highest = 0;

/** @} */

/** Channel parameter to slot# mapping - used by GETPARAMSLOT() macro */
MODVAR unsigned char param_to_slot_mapping[256];
/** Extended channel modes in use - used by ISUPPORT/005 numeric only */
char extchmstr[4][64];

/* Private functions (forward declaration) and variables */
static void make_cmodestr(void);
static char previous_chanmodes[256];
static Cmode *ParamTable[MAXPARAMMODES+1];
static void unload_extcmode_commit(Cmode *cmode);

/** Create the strings that are used for CHANMODES=a,b,c,d in numeric 005 */
void make_extcmodestr()
{
	char *p;
	int i;
	
	extchmstr[0][0] = extchmstr[1][0] = extchmstr[2][0] = extchmstr[3][0] = '\0';
	
	/* type 1: lists (like b/e) */
	/* [NOT IMPLEMENTED IN EXTCMODES] */

	/* type 2: 1 par to set/unset (has .unset_with_param) */
	p = extchmstr[1];
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].paracount && Channelmode_Table[i].flag &&
		    Channelmode_Table[i].unset_with_param)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';

	/* type 3: 1 param to set, 0 params to unset (does not have .unset_with_param) */
	p = extchmstr[2];
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].paracount && Channelmode_Table[i].flag &&
		    !Channelmode_Table[i].unset_with_param)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';
	
	/* type 4: paramless modes */
	p = extchmstr[3];
	for (i=0; i <= Channelmode_highest; i++)
		if (!Channelmode_Table[i].paracount && Channelmode_Table[i].flag)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';
}

/** Create the string that is used in numeric 004 */
static void make_cmodestr(void)
{
	char *p = &cmodestring[0];
	CoreChannelModeTable *tab = &corechannelmodetable[0];
	int i;
	while (tab->mode != 0x0)
	{
		*p = tab->flag;
		p++;
		tab++;
	}
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].flag)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';
}

/** Check for changes - if any are detected, we broadcast the change */
void extcmodes_check_for_changes(void)
{
	char chanmodes[256];
	ISupport *isup;

	make_cmodestr();
	make_extcmodestr();

	snprintf(chanmodes, sizeof(chanmodes), "%s%s", CHPAR1, EXPAR1);
	safe_strdup(me.serv->features.chanmodes[0], chanmodes);
	snprintf(chanmodes, sizeof(chanmodes), "%s%s", CHPAR2, EXPAR2);
	safe_strdup(me.serv->features.chanmodes[1], chanmodes);
	snprintf(chanmodes, sizeof(chanmodes), "%s%s", CHPAR3, EXPAR3);
	safe_strdup(me.serv->features.chanmodes[2], chanmodes);
	snprintf(chanmodes, sizeof(chanmodes), "%s%s", CHPAR4, EXPAR4);
	safe_strdup(me.serv->features.chanmodes[3], chanmodes);

	ircsnprintf(chanmodes, sizeof(chanmodes), "%s,%s,%s,%s",
	            me.serv->features.chanmodes[0],
	            me.serv->features.chanmodes[1],
	            me.serv->features.chanmodes[2],
	            me.serv->features.chanmodes[3]);

	isup = ISupportFind("CHANMODES");
	if (!isup)
	{
		strlcpy(previous_chanmodes, chanmodes, sizeof(previous_chanmodes));
		return; /* not booted yet. then we are done here. */
	}
	
	ISupportSetValue(isup, chanmodes);
	
	if (*previous_chanmodes && strcmp(chanmodes, previous_chanmodes))
	{
		ircd_log(LOG_ERROR, "Channel modes changed at runtime: %s -> %s",
			previous_chanmodes, chanmodes);
		sendto_realops("Channel modes changed at runtime: %s -> %s",
			previous_chanmodes, chanmodes);
		/* Broadcast change to all (locally connected) servers */
		sendto_server(NULL, 0, 0, NULL, "PROTOCTL CHANMODES=%s", chanmodes);
	}

	strlcpy(previous_chanmodes, chanmodes, sizeof(previous_chanmodes));
}

/** Initialize the extended channel modes system */
void extcmode_init(void)
{
	Cmode_t val = 1;
	int	i;
	Channelmode_Table = safe_alloc(sizeof(Cmode) * EXTCMODETABLESZ);
	for (i = 0; i < EXTCMODETABLESZ; i++)
	{
		Channelmode_Table[i].mode = val;
		val *= 2;
	}
	Channelmode_highest = 0;
	memset(&extchmstr, 0, sizeof(extchmstr));
	memset(&param_to_slot_mapping, 0, sizeof(param_to_slot_mapping));
	*previous_chanmodes = '\0';
}

/** Update letter->slot mapping and slot->handler mapping */
void extcmode_para_addslot(Cmode *c, int slot)
{
	if ((slot < 0) || (slot > MAXPARAMMODES))
		abort();
	c->slot = slot;
	ParamTable[slot] = c;
	param_to_slot_mapping[c->flag] = slot;
}

/** Update letter->slot mapping and slot->handler mapping */
void extcmode_para_delslot(Cmode *c, int slot)
{
	if ((slot < 0) || (slot > MAXPARAMMODES))
		abort();
	ParamTable[slot] = NULL;
	param_to_slot_mapping[c->flag] = 0;
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
	short i = 0, j = 0;
	int paraslot = -1;
	int existing = 0;

	while (i < EXTCMODETABLESZ)
	{
		if (!Channelmode_Table[i].flag)
			break;
		else if (Channelmode_Table[i].flag == req.flag)
		{
			if (Channelmode_Table[i].unloaded)
			{
				Channelmode_Table[i].unloaded = 0;
				existing = 1;
				break;
			} else {
				if (module)
					module->errorcode = MODERR_EXISTS;
				return NULL;
			}
		}
		i++;
	}
	if (i == EXTCMODETABLESZ)
	{
		Debug((DEBUG_DEBUG, "CmodeAdd failed, no space"));
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}

	if (req.paracount == 1)
	{
		if (existing)
		{
			/* Re-use parameter slot of the module with the same modechar that is unloading */
			paraslot = Channelmode_Table[i].slot;
		}
		else
		{
			/* Allocate a new one */
			for (paraslot = 0; ParamTable[paraslot]; paraslot++)
			{
				if (paraslot == MAXPARAMMODES - 1)
				{
					Debug((DEBUG_DEBUG, "CmodeAdd failed, no space for parameter"));
					if (module)
						module->errorcode = MODERR_NOSPACE;
					return NULL;
				}
			}
		}
	}

	*mode = Channelmode_Table[i].mode;
	/* Update extended channel mode table highest */
	Channelmode_Table[i].flag = req.flag;
	Channelmode_Table[i].paracount = req.paracount;
	Channelmode_Table[i].is_ok = req.is_ok;
	Channelmode_Table[i].put_param = req.put_param;
	Channelmode_Table[i].get_param = req.get_param;
	Channelmode_Table[i].conv_param = req.conv_param;
	Channelmode_Table[i].free_param = req.free_param;
	Channelmode_Table[i].dup_struct = req.dup_struct;
	Channelmode_Table[i].sjoin_check = req.sjoin_check;
	Channelmode_Table[i].local = req.local;
	Channelmode_Table[i].unset_with_param = req.unset_with_param;
	Channelmode_Table[i].owner = module;
	Channelmode_Table[i].unloaded = 0;
	
	for (j = 0; j < EXTCMODETABLESZ; j++)
		if (Channelmode_Table[j].flag)
			if (j > Channelmode_highest)
				Channelmode_highest = j;

        if (Channelmode_Table[i].paracount == 1)
                extcmode_para_addslot(&Channelmode_Table[i], paraslot);
                
	if (module)
	{
		ModuleObject *cmodeobj = safe_alloc(sizeof(ModuleObject));
		cmodeobj->object.cmode = &Channelmode_Table[i];
		cmodeobj->type = MOBJ_CMODE;
		AddListItem(cmodeobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return &(Channelmode_Table[i]);
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
	if (loop.ircd_rehashing)
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

	/* Unset channel mode and send MODE to everyone */

	if (cmode->paracount == 0)
	{
		/* Paramless mode, easy */
		for (channel = channels; channel; channel = channel->nextch)
		{
			if (channel->mode.extmode & cmode->mode)
			{
				MessageTag *mtags = NULL;

				new_message(&me, NULL, &mtags);
				sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
					       ":%s MODE %s -%c",
					       me.name, channel->chname, cmode->flag);
				sendto_server(NULL, 0, 0, mtags,
					":%s MODE %s -%c 0",
					me.id, channel->chname, cmode->flag);
				free_message_tags(mtags);

				channel->mode.extmode &= ~cmode->mode;
			}
		}
	} else
	{
		/* Parameter mode, more complicated */
		for (channel = channels; channel; channel = channel->nextch)
		{
			if (channel->mode.extmode & cmode->mode)
			{
				MessageTag *mtags = NULL;

				new_message(&me, NULL, &mtags);
				if (cmode->unset_with_param)
				{
					char *param = cmode->get_param(GETPARASTRUCT(channel, cmode->flag));
					sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
						       ":%s MODE %s -%c %s",
						       me.name, channel->chname, cmode->flag, param);
					sendto_server(NULL, 0, 0, mtags,
						":%s MODE %s -%c %s 0",
						me.id, channel->chname, cmode->flag, param);
				} else {
					sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
						       ":%s MODE %s -%c",
						       me.name, channel->chname, cmode->flag);
					sendto_server(NULL, 0, 0, mtags,
						":%s MODE %s -%c 0",
						me.id, channel->chname, cmode->flag);
				}
				free_message_tags(mtags);

				cmode->free_param(GETPARASTRUCT(channel, cmode->flag));
				channel->mode.extmode &= ~cmode->mode;
			}
		}
		extcmode_para_delslot(cmode, cmode->slot);
	}

	cmode->flag = '\0';
}

/** Unload all unused channel modes after a REHASH */
void unload_all_unused_extcmodes(void)
{
	int i;

	for (i = 0; i < EXTCMODETABLESZ; i++)
		if (Channelmode_Table[i].flag && Channelmode_Table[i].unloaded)
		{
			unload_extcmode_commit(&Channelmode_Table[i]);
		}

}

/** @defgroup ChannelModeAPI Channel mode API
 * @{
 */

/** Get parameter for a channel mode.
 * @param channel	The channel
 * @param mode		The mode character (eg: 'f')
 */
char *cm_getparameter(Channel *channel, char mode)
{
	return GETPARAMHANDLERBYLETTER(mode)->get_param(GETPARASTRUCT(channel, mode));
}

/** Set parameter for a channel mode.
 * @param channel	The channel
 * @param mode		The mode character (eg: 'f')
 * @param str		The parameter string
 * @note Module users should not use this function directly, it is only used by MODE and SJOIN.
 */
void cm_putparameter(Channel *channel, char mode, char *str)
{
	GETPARASTRUCT(channel, mode) = GETPARAMHANDLERBYLETTER(mode)->put_param(GETPARASTRUCT(channel, mode), str);
}

/** Free a channel mode parameter.
 * @param channel	The channel
 * @param mode		The mode character (eg: 'f')
 */
void cm_freeparameter(Channel *channel, char mode)
{
	GETPARAMHANDLERBYLETTER(mode)->free_param(GETPARASTRUCT(channel, mode));
	GETPARASTRUCT(channel, mode) = NULL;
}

/** Get parameter for a channel mode - special version for SJOIN.
 * This version doesn't take a channel, but a mode.extmodeparams.
 * It is only used by SJOIN and should not be used in 3rd party modules.
 * @param p	The list, eg oldmode.extmodeparams
 * @param mode	The mode letter
 */
char *cm_getparameter_ex(void **p, char mode)
{
	return GETPARAMHANDLERBYLETTER(mode)->get_param(GETPARASTRUCTEX(p, mode));
}

/** Set parameter for a channel mode - special version for SJOIN.
 * This version doesn't take a channel, but a mode.extmodeparams.
 * It is only used by SJOIN and should not be used in 3rd party modules.
 * @param p	The list, eg oldmode.extmodeparams
 * @param mode	The mode letter
 * @param str	The mode parameter string to set
 */
void cm_putparameter_ex(void **p, char mode, char *str)
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
int extcmode_default_requirechop(Client *client, Channel *channel, char mode, char *para, int checkt, int what)
{
	if (IsUser(client) && is_chan_op(client, channel))
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
int extcmode_default_requirehalfop(Client *client, Channel *channel, char mode, char *para, int checkt, int what)
{
	if (IsUser(client) && (is_chan_op(client, channel) || is_half_op(client, channel)))
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
		inx = xi[handler->slot]; /* paramter data of input is here */
		if (!inx)
			continue; /* not set */
		xo[handler->slot] = handler->dup_struct(inx); /* call dup_struct with that input and set the output param to that */
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
		handler->free_param(ar[handler->slot]);
		ar[handler->slot] = NULL;
	}
}

/** @} */

/** Internal function: returns 1 if the specified module has 1 or more extended channel modes registered */
int module_has_extcmode_param_mode(Module *mod)
{
	int i = 0;

	while (i < EXTCMODETABLESZ)
	{
		if ((Channelmode_Table[i].flag) &&
		    (Channelmode_Table[i].owner == mod) &&
		    (Channelmode_Table[i].paracount))
		{
			return 1;
		}
		i++;
	}
	return 0;
}
