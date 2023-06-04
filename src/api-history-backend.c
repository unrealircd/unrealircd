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
	ModuleObject *mobj;

	if (!mreq->history_add || !mreq->history_request || !mreq->history_delete ||
	    !mreq->history_destroy || !mreq->history_set_limit)
	{
		module->errorcode = MODERR_INVALID;
		unreal_log(ULOG_ERROR, "module", "HISTORYBACKENDADD_API_ERROR", NULL,
			   "HistoryBackendAdd(): missing a handler for add/del/request/destroy/set_limit. Module: $module_name",
			   log_data_string("module_name", module->header->name));
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
	m->history_request = mreq->history_request;
	m->history_delete = mreq->history_delete;
	m->history_destroy = mreq->history_destroy;
	m->history_set_limit = mreq->history_set_limit;

	if (!exists)
		AddListItem(m, historybackends);

	mobj = safe_alloc(sizeof(ModuleObject));
	mobj->type = MOBJ_HISTORY_BACKEND;
	mobj->object.history_backend = m;
	AddListItem(mobj, module->objects);
	module->errorcode = MODERR_NOERROR;

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

	if (loop.rehashing)
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

int history_add(const char *object, MessageTag *mtags, const char *line)
{
	HistoryBackend *hb;

	for (hb = historybackends; hb; hb=hb->next)
		hb->history_add(object, mtags, line);

	return 1;
}

HistoryResult *history_request(const char *object, HistoryFilter *filter)
{
	HistoryBackend *hb = historybackends;
	HistoryResult *r;
	HistoryLogLine *l;

	if (!hb)
		return 0; /* no history backend loaded */

	/* Right now we return whenever the first backend has a result. */
	for (hb = historybackends; hb; hb = hb->next)
		if ((r = hb->history_request(object, filter)))
			return r;

	return NULL;
}

int history_delete(const char *object, HistoryFilter *filter, int *rejected_deletes) {
	HistoryBackend *hb = historybackends;
	HistoryResult *r;
	HistoryLogLine *l;

	int deleted, max_deleted = 0, max_rejected_deletes = 0;
	if (rejected_deletes)
	    *rejected_deletes = 0;

	if (!hb)
		return 0; /* no history backend loaded */

	/* Right now we assume each backend stores either a superset or a subset of
	 * other backends; so the actual number of deleted lines and rejected deletes
	 * can simply be computed as the maximum */
	for (hb = historybackends; hb; hb = hb->next) {
		deleted = hb->history_delete(object, filter, rejected_deletes);
		if (deleted > max_deleted)
			max_deleted = deleted;
		if (!rejected_deletes && *rejected_deletes > max_rejected_deletes)
			max_rejected_deletes = *rejected_deletes;
	}

	if (rejected_deletes)
		*rejected_deletes = max_rejected_deletes;

	return max_deleted;
}

int history_destroy(const char *object)
{
	HistoryBackend *hb;

	for (hb = historybackends; hb; hb=hb->next)
		hb->history_destroy(object);

	return 1;
}

int history_set_limit(const char *object, int max_lines, long max_t)
{
	HistoryBackend *hb;

	for (hb = historybackends; hb; hb=hb->next)
		hb->history_set_limit(object, max_lines, max_t);

	return 1;
}

/** Free a HistoryResult object that was returned from request_result() earlier */
void free_history_result(HistoryResult *r)
{
	HistoryLogLine *l, *l_next;
	for (l = r->log; l; l = l_next)
	{
		l_next = l->next;
		free_message_tags(l->mtags);
		safe_free(l);
	}
	safe_free(r->object);
	safe_free(r);
}

/** Returns 1 if the client can receive channel history, 0 if not.
 * @param client	The client to check.
 * @note It is recommend to call this function BEFORE trying to
 *       retrieve channel history via history_request(),
 *       as to not waste useless resources.
 */
int can_receive_history(Client *client)
{
	if (HasCapability(client, "server-time"))
		return 1;
	return 0;
}

static void history_send_result_line(Client *client, HistoryLogLine *l, const char *batchid)
{
	if (BadPtr(batchid))
	{
		sendto_one(client, l->mtags, "%s", l->line);
	} else {
		MessageTag *m = safe_alloc(sizeof(MessageTag));
		m->name = "batch";
		m->value = strdup(batchid);
		AddListItem(m, l->mtags);
		sendto_one(client, l->mtags, "%s", l->line);
		safe_free(m->value);
		DelListItem(m, l->mtags);
		safe_free(m);
	}
}

/** Send the result of a history_request() to the client.
 * @param client	The client to send to.
 * @param r		The history result retrieved via history_request().
 */
void history_send_result(Client *client, HistoryResult *r)
{
	char batch[BATCHLEN+1];
	HistoryLogLine *l;

	if (!can_receive_history(client))
		return;

	batch[0] = '\0';
	if (HasCapability(client, "batch"))
	{
		/* Start a new batch */
		generate_batch_id(batch);
		sendto_one(client, NULL, ":%s BATCH +%s chathistory %s", me.name, batch, r->object);
	}

	for (l = r->log; l; l = l->next)
		history_send_result_line(client, l, batch);

	/* End of batch */
	if (*batch)
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch);
}

void free_history_filter(HistoryFilter *f)
{
	safe_free(f->timestamp_a);
	safe_free(f->msgid_a);
	safe_free(f->timestamp_b);
	safe_free(f->msgid_b);
	safe_free(f);
}
