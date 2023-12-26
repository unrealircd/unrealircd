/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/api-messagetag.c
 *   (c) 2019- Bram Matthys and The UnrealIRCd team
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
 * @brief Message tag API
 */
#include "unrealircd.h"

/** This is the message tags API (message-tags).
 * For an overview of message tags in general (not the API)
 * see https://www.unrealircd.org/docs/Message_tags
 * @defgroup MessagetagAPI Message tag API
 * @{
 */

/** List of message tag handlers */
MODVAR MessageTagHandler *mtaghandlers = NULL;

/* Forward declarations */
static void unload_mtag_handler_commit(MessageTagHandler *m);

/** Adds a new message tag handler.
 * @param module The module which owns this message-tag handler.
 * @param mreq   The details of the request such as which message tag, the handler, etc.
 * @return Returns the handle to the new token if successful, otherwise NULL.
 *         The module's error code contains specific information about the
 *         error.
 */
MessageTagHandler *MessageTagHandlerAdd(Module *module, MessageTagHandlerInfo *mreq)
{
	MessageTagHandler *m;

	/* Some consistency checks to avoid a headache for module devs later on: */
	if ((mreq->flags & MTAG_HANDLER_FLAGS_NO_CAP_NEEDED) && mreq->clicap_handler)
	{
		unreal_log(ULOG_ERROR, "module", "MESSAGETAGHANDLERADD_API_ERROR", NULL,
			   "MessageTagHandlerAdd() from module $module_name: "
			   ".flags is set to MTAG_HANDLER_FLAGS_NO_CAP_NEEDED "
			   "but a .clicap_handler is passed as well. These options are mutually "
			   "exclusive, choose one or the other.",
			   log_data_string("module_name", module->header->name));
		abort();
	} else if (!(mreq->flags & MTAG_HANDLER_FLAGS_NO_CAP_NEEDED) && !mreq->clicap_handler)
	{
		unreal_log(ULOG_ERROR, "module", "MESSAGETAGHANDLERADD_API_ERROR", NULL,
			   "MessageTagHandlerAdd() from module $module_name: "
			   "no .clicap_handler is passed. If the "
		           "message tag really does not require a cap then you must "
		           "set .flags to MTAG_HANDLER_FLAGS_NO_CAP_NEEDED",
		           log_data_string("module_name", module->header->name));
		abort();
	}

	m = MessageTagHandlerFind(mreq->name);
	if (m)
	{
		if (m->unloaded)
		{
			m->unloaded = 0;
		} else {
			if (module)
				module->errorcode = MODERR_EXISTS;
			return NULL;
		}
	} else {
		/* New message tag handler */
		m = safe_alloc(sizeof(MessageTagHandler));
		safe_strdup(m->name, mreq->name);
		AddListItem(m, mtaghandlers);
	}
	/* Add or update the following fields: */
	m->owner = module;
	m->flags = mreq->flags;
	m->is_ok = mreq->is_ok;
	m->should_send_to_client = mreq->should_send_to_client;
	m->clicap_handler = mreq->clicap_handler;

	/* Update reverse dependency (if any) */
	if (m->clicap_handler)
		m->clicap_handler->mtag_handler = m;

	if (module)
	{
		ModuleObject *mobj = safe_alloc(sizeof(ModuleObject));
		mobj->type = MOBJ_MTAG;
		mobj->object.mtag = m;
		AddListItem(mobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}

	return m;
}

/** Returns the message tag handler for the given name.
 * @param name The message-tag name to search for.
 * @return Returns the handle to the message tag handler,
 *         or NULL if not found.
 */
MessageTagHandler *MessageTagHandlerFind(const char *name)
{
	MessageTagHandler *m;

	for (m = mtaghandlers; m; m = m->next)
	{
		if (!strcasecmp(name, m->name))
			return m;
	}
	return NULL;
}

/** Remove the specified message tag handler - modules should not call this.
 * This is done automatically for modules on unload, so is only called internally.
 * @param m The message tag handler to remove.
 */
void MessageTagHandlerDel(MessageTagHandler *m)
{
	if (m->owner)
	{
		ModuleObject *mobj;
		for (mobj = m->owner->objects; mobj; mobj = mobj->next) {
			if (mobj->type == MOBJ_MTAG && mobj->object.mtag == m)
			{
				DelListItem(mobj, m->owner->objects);
				safe_free(mobj);
				break;
			}
		}
		m->owner = NULL;
	}

	if (loop.rehashing)
		m->unloaded = 1;
	else
		unload_mtag_handler_commit(m);
}

/** @} */

static void unload_mtag_handler_commit(MessageTagHandler *m)
{
	/* This is an unusual operation, I think we should log it. */
	unreal_log(ULOG_INFO, "module", "UNLOAD_MESSAGE_TAG", NULL,
	           "Unloading message-tag handler for '$token'",
	           log_data_string("token", m->name));

	/* Remove reverse dependency, if any */
	if (m->clicap_handler)
		m->clicap_handler->mtag_handler = NULL;

	/* Destroy the object */
	DelListItem(m, mtaghandlers);
	safe_free(m->name);
	safe_free(m);
}

void unload_all_unused_mtag_handlers(void)
{
	MessageTagHandler *m, *m_next;

	for (m = mtaghandlers; m; m = m_next)
	{
		m_next = m->next;
		if (m->unloaded)
			unload_mtag_handler_commit(m);
	}
}
