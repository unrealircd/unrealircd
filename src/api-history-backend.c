/************************************************************************
 * UnrealIRCd - Unreal Internet Relay Chat Daemon - src/api-history-backend.c
 * (c) 2019- Bram Matthys and The UnrealIRCd team
 *
 * See file AUTHORS in IRC package for additional names of
 * the programmers. 
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

MODVAR HistoryBackend *historybackends = NULL; /**< List of registered history backends */

void history_backend_init(void)
{
}

/**
 * Returns a history backend based on the given token name.
 *
 * @param name The name of the history backend.
 * @return Returns the handle to the history backend,
 *         or NULL if not found.
 */
HistoryBackend *HistoryBackendFind(const char *name)
{
	HistoryBackend *m;

	for (m = historybackends; m; m = m->next)
	{
		if (!strcasecmp(name, m->name))
			return m;
	}
	return NULL;
}

/**
 * Adds a new history backend.
 *
 * @param module The module which provides this history backend.
 * @param mreq   The details of the request such as token name, access check handler, etc.
 * @return Returns the handle to the new token if successful, otherwise NULL.
 *         The module's error code contains specific information about the
 *         error.
 */
HistoryBackend *HistoryBackendAdd(Module *module, HistoryBackendInfo *mreq)
{
	HistoryBackend *m;
	int exists = 0;

	if (!mreq->history_add || !mreq->history_del || !mreq->history_request || !mreq->history_destroy)
	{
		if (module)
			module->errorcode = MODERR_INVALID;
		ircd_log(LOG_ERROR, "HistoryBackendAdd(): missing a handler for add/del/request/destroy");
		return NULL;
	}
	m = HistoryBackendFind(mreq->name);
	if (m)
	{
		exists = 1;
		if (m->unloaded)
		{
			m->unloaded = 0;
		} else {
			if (module)
				module->errorcode = MODERR_EXISTS;
			return NULL;
		}
	} else {
		/* New history backend */
		m = safe_alloc(sizeof(HistoryBackend));
		safe_strdup(m->name, mreq->name);
	}

	/* Add or update the following fields: */
	m->owner = module;
	m->history_add = mreq->history_add;
	m->history_del = mreq->history_del;
	m->history_request = mreq->history_request;
	m->history_destroy = mreq->history_destroy;

	if (!exists)
		AddListItem(m, historybackends);

	if (module)
	{
		ModuleObject *mobj = safe_alloc(sizeof(ModuleObject));
		mobj->type = MOBJ_HISTORY_BACKEND;
		mobj->object.history_backend = m;
		AddListItem(mobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}

	return m;
}

void unload_history_backend_commit(HistoryBackend *m)
{
	/* Destroy the object */
	DelListItem(m, historybackends);
	safe_free(m->name);
	safe_free(m);
}

/**
 * Removes the specified history backend.
 *
 * @param m The history backend to remove.
 */
void HistoryBackendDel(HistoryBackend *m)
{
	if (m->owner)
	{
		ModuleObject *mobj;
		for (mobj = m->owner->objects; mobj; mobj = mobj->next) {
			if (mobj->type == MOBJ_HISTORY_BACKEND && mobj->object.history_backend == m)
			{
				DelListItem(mobj, m->owner->objects);
				safe_free(mobj);
				break;
			}
		}
		m->owner = NULL;
	}

	if (loop.ircd_rehashing)
		m->unloaded = 1;
	else
		unload_history_backend_commit(m);
}

void unload_all_unused_history_backends(void)
{
	HistoryBackend *m, *m_next;

	for (m = historybackends; m; m = m_next)
	{
		m_next = m->next;
		if (m->unloaded)
			unload_history_backend_commit(m);
	}
}

int history_add(char *object, MessageTag *mtags, char *line)
{
	HistoryBackend *hb;

	for (hb = historybackends; hb; hb=hb->next)
		hb->history_add(object, mtags, line);

	return 1;
}

int history_del(char *object, int max_lines, long max_time)
{
	HistoryBackend *hb;

	for (hb = historybackends; hb; hb=hb->next)
		hb->history_del(object, max_lines, max_time);

	return 1;
}

int history_request(Client *client, char *object, HistoryFilter *filter)
{
	HistoryBackend *hb;

	for (hb = historybackends; hb; hb=hb->next)
		hb->history_request(client, object, filter);

	return 1;
}

int history_destroy(char *object)
{
	HistoryBackend *hb;

	for (hb = historybackends; hb; hb=hb->next)
		hb->history_destroy(object);

	return 1;
}
