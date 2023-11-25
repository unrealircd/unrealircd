/************************************************************************
 * UnrealIRCd - Unreal Internet Relay Chat Daemon - src/api-apicallback.c
 * (c) 2022- Bram Matthys and The UnrealIRCd Team
 * License: GPLv2 or later
 */

/** @file
 * @brief APICallback API
 */
#include "unrealircd.h"

/** This is for API callbacks like RegisterWebCallback.
 */

/** List of API Callbacks */
MODVAR APICallback *apicallbacks = NULL;

/* Forward declarations */
static void unload_apicallback_commit(APICallback *m);
#ifdef DEBUGMODE
static void print_apicallbacks(void);
#endif

/** Adds a new API Callback.
 * @param module The module which owns this API callback.
 * @param mreq   The details of the request such as the name and callback
 * @return Returns the handle to the API callback if successful, otherwise NULL.
 *         The module's error code contains specific information about the
 *         error.
 */
APICallback *APICallbackAdd(Module *module, APICallback *mreq)
{
	APICallback *m;
	ModuleObject *mobj;

	/* Some consistency checks to avoid a headache for module devs later on: */
	if (!mreq->callback_type)
	{
		unreal_log(ULOG_ERROR, "module", "API_CALLBACK_ADD_API_ERROR", NULL,
			   "APICallbackAdd() from module $module_name: "
			   "Missing required fields.",
			   log_data_string("module_name", module->header->name));
		abort();
	}

	m = APICallbackFind(mreq->name, mreq->callback_type);
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
		/* New API callback */
		m = safe_alloc(sizeof(APICallback));
		safe_strdup(m->name, mreq->name);
		AddListItem(m, apicallbacks);
	}
	/* Add or update the following fields: */
	m->owner = module;
	m->callback_type = mreq->callback_type;
	memcpy(&m->callback, &mreq->callback, sizeof(m->callback));

	/* Add module object */
	mobj = safe_alloc(sizeof(ModuleObject));
	mobj->type = MOBJ_API_CALLBACK;
	mobj->object.apicallback = m;
	AddListItem(mobj, module->objects);
	module->errorcode = MODERR_NOERROR;

#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "module", "API_CALLBACK_DEBUG", NULL, "APICallbackAdd()");
	print_apicallbacks();
#endif
	return m;
}

/** Returns the API callback for the given name and callback type.
 * @param name The method to search for.
 * @param callback_type To which callback_type this belongs (scope)
 * @return Returns the handle to the API callback,
 *         or NULL if not found.
 */
APICallback *APICallbackFind(const char *name, APICallbackType callback_type)
{
	APICallback *m;

#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "module", "API_CALLBACK_DEBUG", NULL, "APICallbackFind()");
	print_apicallbacks();
#endif
	for (m = apicallbacks; m; m = m->next)
	{
		if ((m->callback_type == callback_type) &&
		    !strcasecmp(name, m->name))
		{
			return m;
		}
	}
	return NULL;
}

/** Remove the specified API callback - modules should not call this.
 * This is done automatically for modules on unload, so is only called internally.
 * @param m The API Callback to remove.
 */
void APICallbackDel(APICallback *m)
{
	if (m->owner)
	{
		ModuleObject *mobj;
		for (mobj = m->owner->objects; mobj; mobj = mobj->next)
		{
			if (mobj->type == MOBJ_API_CALLBACK && mobj->object.apicallback == m)
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
		unload_apicallback_commit(m);
}

/** @} */

static void unload_apicallback_commit(APICallback *m)
{
	/* This is an unusual operation, I think we should log it. */
	unreal_log(ULOG_INFO, "module", "UNLOAD_API_CALLBACK", NULL,
	           "Unloading API callback for '$object_name'",
	           log_data_string("object_name", m->name));

	/* Destroy the object */
	DelListItem(m, apicallbacks);
	safe_free(m->name);
	safe_free(m);
}

void unload_all_unused_apicallbacks(void)
{
	APICallback *m, *m_next;

#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "module", "API_CALLBACK_DEBUG", NULL, "unload_all_unused_apicallbacks() BEFORE");
	print_apicallbacks();
#endif
	for (m = apicallbacks; m; m = m_next)
	{
		m_next = m->next;
		if (m->unloaded)
			unload_apicallback_commit(m);
	}
#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "module", "API_CALLBACK_DEBUG", NULL, "unload_all_unused_apicallbacks() AFTER");
	print_apicallbacks();
#endif
}

#ifdef DEBUGMODE
void print_apicallbacks(void)
{
	APICallback *m, *m_next;

	unreal_log(ULOG_DEBUG, "module", "API_CALLBACK_LIST", NULL, "----");
	for (m = apicallbacks; m; m = m_next)
	{
		m_next = m->next;
		unreal_log(ULOG_DEBUG, "module", "API_CALLBACK_LIST", NULL,
			"$name ($deleted)",
			log_data_string("name", m->name),
			log_data_string("deleted", m->unloaded ? "deleted" : ""));
	}
	unreal_log(ULOG_DEBUG, "module", "API_CALLBACK_LIST", NULL, "----");
}
#endif
