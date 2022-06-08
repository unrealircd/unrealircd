/*
 * RPC module - for remote management of UnrealIRCd
 * (C)Copyright 2022 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 */
   
#include "unrealircd.h"
#include "dns.h"

ModuleHeader MOD_HEADER
  = {
	"rpc/rpc",
	"1.0.0",
	"RPC module for remote management",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
int rpc_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int rpc_config_run_ex(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr);
//int rpc_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length);
void rpc_mdata_free(ModData *m);
int rpc_client_accept(Client *client);
void rpc_client_handshake(Client *client);
int rpc_handle_webrequest(Client *client, WebRequest *web);
int rpc_handle_webrequest_data(Client *client, WebRequest *web, const char *buf, int len);
int rpc_packet_in(Client *client, const char *readbuf, int *length);
void rpc_call_text(Client *client, const char *buf, int len);
void rpc_call(Client *client, json_t *request);
void _rpc_response(Client *client, json_t *request, json_t *result);
void _rpc_error(Client *client, json_t *request, int error_code, const char *error_message);
void _rpc_error_fmt(Client *client, json_t *request, int error_code, FORMAT_STRING(const char *fmt), ...) __attribute__((format(printf,4,5)));

/* Structs */
typedef struct RPCUser RPCUser;
struct RPCUser {
	int something;
};

/* Macros */
#define RPC(client)       ((RPCUser *)moddata_client(client, rpc_md).ptr)
#define RPC_PORT(client)  ((client->local && client->local->listener) ? client->local->listener->rpc_options : 0)

/* Global variables */
ModDataInfo *rpc_md;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, rpc_config_test);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_RESPONSE, _rpc_response);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_ERROR, _rpc_error);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_ERROR_FMT, TO_VOIDFUNC(_rpc_error_fmt));
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN_EX, 0, rpc_config_run_ex);
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, -5000, rpc_client_accept);
	//HookAdd(modinfo->handle, HOOKTYPE_PACKET, INT_MAX, rpc_packet_out);
	HookAdd(modinfo->handle, HOOKTYPE_RAWPACKET_IN, INT_MIN, rpc_packet_in);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "rpc";
	mreq.serialize = NULL;
	mreq.unserialize = NULL;
	mreq.free = rpc_mdata_free;
	mreq.sync = 0;
	mreq.type = MODDATATYPE_CLIENT;
	rpc_md = ModDataAdd(modinfo->handle, mreq);

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

int rpc_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	// FIXME: CONFIG_SET is not handled here as this is xxx_EX and not xxx
	if (type == CONFIG_SET)
	{
		/* We are only interested in set::rpc.. */
		if (!ce || !ce->name || strcmp(ce->name, "rpc"))
			return 0;

		/* No options atm */

		*errs = errors;
		return errors ? -1 : 1;
	} else
	if (type == CONFIG_LISTEN_OPTIONS)
	{
		/* We are only interested in listen::options::rpc.. */
		if (!ce || !ce->name || strcmp(ce->name, "rpc"))
			return 0;

		/* No options atm */

		*errs = errors;
		return errors ? -1 : 1;
	}
	return 0;
}

int rpc_config_run_ex(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_listen *l;
	static char warned_once_channel = 0;

	// FIXME: CONFIG_SET is not handled here as this is xxx_EX and not xxx
	if (type == CONFIG_SET)
	{
		/* We are only interested in set::rpc.. */
		if (!ce || !ce->name || strcmp(ce->name, "rpc"))
			return 0;

		/* Do something useful here */

		return 1;
	} else
	if (type == CONFIG_LISTEN_OPTIONS)
	{
		/* We are only interrested in listen::options::rpc.. */
		if (!ce || !ce->name || strcmp(ce->name, "rpc"))
			return 0;

		l = (ConfigItem_listen *)ptr;
		l->options |= LISTENER_NO_CHECK_CONNECT_FLOOD;
		l->start_handshake = rpc_client_handshake;
		l->webserver = safe_alloc(sizeof(WebServer));
		l->webserver->handle_request = rpc_handle_webrequest;
		l->webserver->handle_data = rpc_handle_webrequest_data;
		l->rpc_options = 1;

		return 1;
	}
	return 0;
}

/** UnrealIRCd internals: free WebRequest object. */
void rpc_mdata_free(ModData *m)
{
	RPCUser *r = (RPCUser *)m->ptr;
	if (r)
	{
		//safe_free(r->something);
		safe_free(m->ptr);
	}
}

/** Outgoing packet hook. */
int rpc_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length)
{
	static char utf8buf[510];

	if (MyConnect(to) && RPC(to))
	{
		// TODO: Inhibit all?
		// Websocket can override though?
		return 0;
	}
	return 0;
}

int rpc_handle_webrequest(Client *client, WebRequest *web)
{
	if (!strcmp(web->uri, "/api"))
	{
		if (web->method != HTTP_METHOD_POST)
		{
			webserver_send_response(client, 200, "To use the UnrealIRCd RPC API you need to make a POST request. See https://www.unrealircd.org/docs/RPC\n");
			return 0;
		}
		webserver_send_response(client, 200, NULL); /* continue.. */
		return 1; /* accept */
	}
	webserver_send_response(client, 404, "Page not found.\n");
	return 0;
}

int rpc_handle_webrequest_data(Client *client, WebRequest *web, const char *buf, int len)
{
	/* We only handle POST to /api -- reject all the rest */
	if (strcmp(web->uri, "/api") || (web->method != HTTP_METHOD_POST))
	{
		webserver_send_response(client, 404, "Page not found\n");
		return 0;
	}

	// NB: content_length
	// NB: chunked transfers?
	if (!webserver_handle_request_body(client, web, buf, len))
	{
		webserver_send_response(client, 400, "Error handling POST body data\n");
		return 0;
	}


	if (web->request_body_complete)
	{
		if (!web->request_buffer)
		{
			webserver_send_response(client, 500, "Error while processing POST body data\n");
			return 0;
		}
		//config_status("GOT: '%s'", buf);
		rpc_call_text(client, web->request_buffer, web->request_buffer_size);
		send_queued(client);
		webserver_close_client(client);
	}

	return 0;
}

// TODO: exempt the web port from throttling for defined trusted IP's, or at a different rate.
// perhaps hook on create/accept and then immediately reject if not trusted IP,
// that would be safest.

int rpc_packet_in(Client *client, const char *readbuf, int *length)
{
	/* Don't care about EOF and errors - are we even called for those? */
	if (*length <= 0)
		return 1;

	if (!RPC_PORT(client) || !(client->local->listener->socket_type == SOCKET_TYPE_UNIX))
	{
		/* Not for us */
		return 1;
	}

	// FIXME: this assumes a single request in 'readbuf' while in fact:
	// - it could only contain partial JSON, eg no ending } yet
	// - there could be multiple requests
	rpc_call_text(client, readbuf, *length);

	return 0;
}

void rpc_close(Client *client)
{
	// FIXME: need to stop input here.
	
	send_queued(client);

	/* May not be a web request actually, but this works: */
	webserver_close_client(client);
}

/** Handle the RPC request: input is a buffer with a certain length.
 * This calls rpc_call()
 */
void rpc_call_text(Client *client, const char *readbuf, int len)
{
	char buf[2048];
	json_t *request = NULL;
	json_error_t jerr;

	*buf = '\0';
	strlncpy(buf, readbuf, sizeof(buf), len);

	request = json_loads(buf, JSON_REJECT_DUPLICATES, &jerr);
	if (!request)
	{
		unreal_log(ULOG_INFO, "log", "RPC_INVALID_JSON", client,
		           "Received unparsable JSON request from $client",
		           log_data_string("json_incoming", buf));
		rpc_error(client, NULL, JSON_RPC_ERROR_PARSE_ERROR, "Unparsable JSON data");
		/* This is a fatal error */
		rpc_close(client);
		return;
	}
	rpc_call(client, request);
	json_decref(request);
}

void _rpc_error(Client *client, json_t *request, int error_code, const char *error_message)
{
	/* Careful, we are in the "error" routine, so everything can be NULL */
	const char *method = NULL;
	json_t *id = NULL;
	char *json_serialized;
	json_t *error;

	/* Start a new object for the error response */
	json_t *j = json_object();

	if (request)
	{
		method = json_object_get_string(request, "method");
		id = json_object_get(request, "id");
	}

	json_object_set_new(j, "jsonrpc", json_string_unreal("2.0"));
	if (method)
		json_object_set_new(j, "method", json_string_unreal(method));
	if (id)
		json_object_set_new(j, "id", id);

	error = json_object();
	json_object_set_new(j, "error", error);
	json_object_set_new(error, "code", json_integer(error_code));
	json_object_set_new(error, "message", json_string_unreal(error_message));

	json_serialized = json_dumps(j, 0);
	if (!json_serialized)
	{
		unreal_log(ULOG_WARNING, "rpc", "BUG_RPC_ERROR_SERIALIZE_FAILED", NULL,
		           "[BUG] rpc_error() failed to serialize response "
		           "for request from $client ($method)",
		           log_data_string("method", method));
		json_decref(j);
		return;
	}
	dbuf_put(&client->local->sendQ, json_serialized, strlen(json_serialized));
	dbuf_put(&client->local->sendQ, "\n", 1);
	json_decref(j);
	safe_free(json_serialized);
}

void _rpc_error_fmt(Client *client, json_t *request, int error_code, const char *fmt, ...)
{
	char buf[512];

	va_list vl;
	va_start(vl, fmt);
	vsnprintf(buf, sizeof(buf), fmt, vl);
	va_end(vl);
	rpc_error(client, request, error_code, buf);
}

void _rpc_response(Client *client, json_t *request, json_t *result)
{
	const char *method = json_object_get_string(request, "method");
	json_t *id = json_object_get(request, "id");
	char *json_serialized;
	json_t *j = json_object();

	json_object_set_new(j, "jsonrpc", json_string_unreal("2.0"));
	json_object_set_new(j, "method", json_string_unreal(method));
	if (id)
		json_object_set_new(j, "id", id); /* 'id' is optional */
	json_object_set(j, "response", result);

	json_serialized = json_dumps(j, 0);
	if (!json_serialized)
	{
		unreal_log(ULOG_WARNING, "rpc", "BUG_RPC_RESPONSE_SERIALIZE_FAILED", NULL,
		           "[BUG] rpc_response() failed to serialize response "
		           "for request from $client ($method)",
		           log_data_string("method", method));
		json_decref(j);
		return;
	}
	dbuf_put(&client->local->sendQ, json_serialized, strlen(json_serialized));
	dbuf_put(&client->local->sendQ, "\n", 1);
	json_decref(j);
	safe_free(json_serialized);
}


/** Handle the RPC request: request is in JSON */
void rpc_call(Client *client, json_t *request)
{
	json_t *t;
	const char *jsonrpc;
	const char *method;
	json_t *params;
	RPCHandler *handler;

	jsonrpc = json_object_get_string(request, "jsonrpc");
	if (!jsonrpc || strcasecmp(jsonrpc, "2.0"))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_REQUEST, "Only JSON-RPC version 2.0 is supported");
		return;
	}
	method = json_object_get_string(request, "method");
	if (!method)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_REQUEST, "Missing 'method' to call");
		return;
	}
	params = json_object_get(request, "params");
	if (!params)
	{
		/* Params is optional, so create an empty params object instead
		 * to make life easier of the RPC handlers (no need to check NULL).
		 */
		params = json_object();
	}

	handler = RPCHandlerFind(method);
	if (!handler)
	{
		rpc_error(client, request, JSON_RPC_ERROR_METHOD_NOT_FOUND, "Unsupported method");
		return;
	}
	handler->call(client, request, params);
}

/** Called very early on accept() of the socket, before TLS is ready */
int rpc_client_accept(Client *client)
{
	if (RPC_PORT(client))
		SetRPC(client);
	return 0;
}

void rpc_client_handshake(Client *client)
{
	SetRPC(client); /* explicit set due to TLS */
	fd_setselect(client->local->fd, FD_SELECT_READ, read_packet, client);

	/* FIXME: IP Access checks here (if possible) */
}
