/************************************************************************
 * UnrealIRCd - Unreal Internet Relay Chat Daemon - src/api-rpc.c
 * (c) 2022- Bram Matthys and The UnrealIRCd Team
 * License: GPLv2 or later
 */

/** @file
 * @brief RPC API
 */
#include "unrealircd.h"

/** This is the RPC API used for web requests.
 * For an overview of available RPC's (not the API)
 * see https://www.unrealircd.org/docs/RPC
 * @defgroup RPCAPI RPC API
 * @{
 */

/** List of RPC handlers */
MODVAR RPCHandler *rpchandlers = NULL;

/* Forward declarations */
static void unload_rpc_handler_commit(RPCHandler *m);

/** Adds a new RPC handler.
 * @param module The module which owns this RPC handler.
 * @param mreq   The details of the request such as the method name, callback, etc.
 * @return Returns the handle to the RPC handler if successful, otherwise NULL.
 *         The module's error code contains specific information about the
 *         error.
 */
RPCHandler *RPCHandlerAdd(Module *module, RPCHandlerInfo *mreq)
{
	RPCHandler *m;
	ModuleObject *mobj;

	/* Some consistency checks to avoid a headache for module devs later on: */
	if (!mreq->method || !mreq->call)
	{
		unreal_log(ULOG_ERROR, "module", "RPCHANDLERADD_API_ERROR", NULL,
			   "RPCHandlerAdd() from module $module_name: "
			   "Missing required fields.",
			   log_data_string("module_name", module->header->name));
		abort();
	}

	m = RPCHandlerFind(mreq->method);
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
		/* New RPC handler */
		m = safe_alloc(sizeof(RPCHandler));
		safe_strdup(m->method, mreq->method);
		AddListItem(m, rpchandlers);
	}
	/* Add or update the following fields: */
	m->owner = module;
	m->flags = mreq->flags;
	m->loglevel = mreq->loglevel;
	if (!valid_loglevel(m->loglevel))
		m->loglevel = ULOG_INFO;
	m->call = mreq->call;

	/* Add module object */
	mobj = safe_alloc(sizeof(ModuleObject));
	mobj->type = MOBJ_RPC;
	mobj->object.rpc = m;
	AddListItem(mobj, module->objects);
	module->errorcode = MODERR_NOERROR;

	return m;
}

/** Returns the RPC handler for the given method name.
 * @param method The method to search for.
 * @return Returns the handle to the RPC handler,
 *         or NULL if not found.
 */
RPCHandler *RPCHandlerFind(const char *method)
{
	RPCHandler *m;

	for (m = rpchandlers; m; m = m->next)
	{
		if (!strcasecmp(method, m->method))
			return m;
	}
	return NULL;
}

/** Remove the specified RPC handler - modules should not call this.
 * This is done automatically for modules on unload, so is only called internally.
 * @param m The PRC handler to remove.
 */
void RPCHandlerDel(RPCHandler *m)
{
	if (m->owner)
	{
		ModuleObject *mobj;
		for (mobj = m->owner->objects; mobj; mobj = mobj->next) {
			if (mobj->type == MOBJ_RPC && mobj->object.rpc == m)
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
		unload_rpc_handler_commit(m);
}

/** @} */

static void unload_rpc_handler_commit(RPCHandler *m)
{
	/* This is an unusual operation, I think we should log it. */
	unreal_log(ULOG_INFO, "module", "UNLOAD_RPC_HANDLER", NULL,
	           "Unloading RPC handler for '$method'",
	           log_data_string("method", m->method));

	/* Destroy the object */
	DelListItem(m, rpchandlers);
	safe_free(m->method);
	safe_free(m);
}

void unload_all_unused_rpc_handlers(void)
{
	RPCHandler *m, *m_next;

	for (m = rpchandlers; m; m = m_next)
	{
		m_next = m->next;
		if (m->unloaded)
			unload_rpc_handler_commit(m);
	}
}
