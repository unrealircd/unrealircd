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
int rpc_config_test_listen(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int rpc_config_run_ex_listen(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr);
int rpc_config_test_rpc_user(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int rpc_config_run_rpc_user(ConfigFile *cf, ConfigEntry *ce, int type);
int rpc_client_accept(Client *client);
int rpc_pre_local_handshake_timeout(Client *client, const char **comment);
void rpc_client_handshake_unix_socket(Client *client);
void rpc_client_handshake_web(Client *client);
int rpc_handle_webrequest(Client *client, WebRequest *web);
int rpc_handle_webrequest_websocket(Client *client, WebRequest *web);
int rpc_websocket_handshake_send_response(Client *client);
int rpc_handle_webrequest_data(Client *client, WebRequest *web, const char *buf, int len);
int rpc_handle_body_websocket(Client *client, WebRequest *web, const char *readbuf2, int length2);
int rpc_packet_in_websocket(Client *client, char *readbuf, int length);
int rpc_packet_in_unix_socket(Client *client, const char *readbuf, int *length);
void rpc_call_text(Client *client, const char *buf, int len);
void rpc_call(Client *client, json_t *request);
void _rpc_response(Client *client, json_t *request, json_t *result);
void _rpc_error(Client *client, json_t *request, JsonRpcError error_code, const char *error_message);
void _rpc_error_fmt(Client *client, json_t *request, JsonRpcError error_code, FORMAT_STRING(const char *fmt), ...) __attribute__((format(printf,4,5)));
int rpc_handle_auth(Client *client, WebRequest *web);
int rpc_parse_auth_basic_auth(Client *client, WebRequest *web, char **username, char **password);
int rpc_parse_auth_uri(Client *client, WebRequest *web, char **username, char **password);
RPC_CALL_FUNC(rpc_rpc_info);

/* Structs */
typedef struct RPCUser RPCUser;
struct RPCUser {
	RPCUser *prev, *next;
	SecurityGroup *match;
	char *name;
	AuthConfig *auth;
};

/* Macros */
#define RPC_PORT(client)  ((client->local && client->local->listener) ? client->local->listener->rpc_options : 0)
#define WSU(client)     ((WebSocketUser *)moddata_client(client, websocket_md).ptr)

/* Global variables */
ModDataInfo *websocket_md = NULL; /* (imported) */
RPCUser *rpcusers = NULL;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, rpc_config_test_listen);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, rpc_config_test_rpc_user);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_RESPONSE, _rpc_response);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_ERROR, _rpc_error);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_ERROR_FMT, TO_VOIDFUNC(_rpc_error_fmt));

	/* Call MOD_INIT very early, since we manage sockets, but depend on websocket_common */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_INIT+1);

	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	websocket_md = findmoddata_byname("websocket", MODDATATYPE_CLIENT); /* can be NULL */

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN_EX, 0, rpc_config_run_ex_listen);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, rpc_config_run_rpc_user);
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, -5000, rpc_client_accept);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_HANDSHAKE_TIMEOUT, 0, rpc_pre_local_handshake_timeout);
	HookAdd(modinfo->handle, HOOKTYPE_RAWPACKET_IN, INT_MIN, rpc_packet_in_unix_socket);

	memset(&r, 0, sizeof(r));
	r.method = "rpc.info";
	r.call = rpc_rpc_info;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc.info] Could not register RPC handler");
		return MOD_FAILED;
	}

	/* Call MOD_LOAD very late, since we manage sockets, but depend on websocket_common */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_UNLOAD-1);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

void free_config(void)
{
	RPCUser *e, *e_next;
	for (e = rpcusers; e; e = e_next)
	{
		e_next = e->next;
		safe_free(e->name);
		free_security_group(e->match);
		Auth_FreeAuthConfig(e->auth);
		safe_free(e);
	}
	rpcusers = NULL;
}

MOD_UNLOAD()
{
	free_config();
	return MOD_SUCCESS;
}

int rpc_config_test_listen(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	int ext = 0;
	ConfigEntry *cep;

	if (type != CONFIG_LISTEN_OPTIONS)
		return 0;

	/* We are only interested in listen::options::rpc.. */
	if (!ce || !ce->name || strcmp(ce->name, "rpc"))
		return 0;

	/* No options atm */

	*errs = errors;
	return errors ? -1 : 1;
}

int rpc_config_run_ex_listen(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_listen *l;

	if (type != CONFIG_LISTEN_OPTIONS)
		return 0;

	/* We are only interrested in listen::options::rpc.. */
	if (!ce || !ce->name || strcmp(ce->name, "rpc"))
		return 0;

	l = (ConfigItem_listen *)ptr;
	l->options |= LISTENER_NO_CHECK_CONNECT_FLOOD;
	if (l->socket_type == SOCKET_TYPE_UNIX)
	{
		l->start_handshake = rpc_client_handshake_unix_socket;
	} else {
		l->options |= LISTENER_TLS;
		l->start_handshake = rpc_client_handshake_web;
		l->webserver = safe_alloc(sizeof(WebServer));
		l->webserver->handle_request = rpc_handle_webrequest;
		l->webserver->handle_body = rpc_handle_webrequest_data;
	}
	l->rpc_options = 1;

	return 1;
}

int rpc_config_test_rpc_user(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	char has_match = 1, has_password = 1;
	ConfigEntry *cep;

	/* We are only interested in rpc-user { } */
	if ((type != CONFIG_MAIN) || !ce || !ce->name || strcmp(ce->name, "rpc-user"))
		return 0;

	if (!ce->value)
	{
		config_error("%s:%d: rpc-user block needs to have a name, eg: rpc-user apiuser { }",
		             ce->file->filename, ce->line_number);
		errors++;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "match"))
		{
			has_match = 1;
			test_match_block(cf, cep, &errors);
		} else
		if (!strcmp(cep->name, "password"))
		{
			has_password = 1;
			if (Auth_CheckError(cep) < 0)
				errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int rpc_config_run_rpc_user(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	RPCUser *e;

	/* We are only interested in rpc-user { } */
	if ((type != CONFIG_MAIN) || !ce || !ce->name || strcmp(ce->name, "rpc-user"))
		return 0;

	e = safe_alloc(sizeof(RPCUser));
	safe_strdup(e->name, ce->value);
	AddListItem(e, rpcusers);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "match"))
		{
			conf_match_block(cf, cep, &e->match);
		} else
		if (!strcmp(cep->name, "password"))
		{
			e->auth = AuthBlockToAuthConfig(cep);
		}
	}
	return 1;
}

/** Incoming HTTP request: delegate it to websocket handler or HTTP POST */
int rpc_handle_webrequest(Client *client, WebRequest *web)
{
	if (!rpc_handle_auth(client, web))
		return 0; /* rejected */

	if (get_nvplist(web->headers, "Sec-WebSocket-Key"))
		return rpc_handle_webrequest_websocket(client, web);

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

/** Handle HTTP request - websockets handshake.
 */
int rpc_handle_webrequest_websocket(Client *client, WebRequest *web)
{
	NameValuePrioList *r;
	const char *value;

	if (!websocket_md)
	{
		webserver_send_response(client, 405, "Websockets are disabled on this server (module 'websocket_common' not loaded).\n");
		return 0;
	}

	/* Allocate a new WebSocketUser struct for this session */
	moddata_client(client, websocket_md).ptr = safe_alloc(sizeof(WebSocketUser));
	/* ...and set the default protocol (text or binary) */
	WSU(client)->type = WEBSOCKET_TYPE_TEXT;

	value = get_nvplist(web->headers, "Sec-WebSocket-Key");
	if (strchr(value, ':'))
	{
		/* This would cause unserialization issues. Should be base64 anyway */
		webserver_send_response(client, 400, "Invalid characters in Sec-WebSocket-Key");
		return 0; // FIXME: 0 here, -1 in the other, what is it ???
	}
	safe_strdup(WSU(client)->handshake_key, value);

	rpc_websocket_handshake_send_response(client);
	return 1; /* ACCEPT */
}

/** Complete the handshake by sending the appropriate HTTP 101 response etc. */
int rpc_websocket_handshake_send_response(Client *client)
{
	char buf[512], hashbuf[64];
	char sha1out[20]; /* 160 bits */

	WSU(client)->handshake_completed = 1;

	snprintf(buf, sizeof(buf), "%s%s", WSU(client)->handshake_key, WEBSOCKET_MAGIC_KEY);
	sha1hash_binary(sha1out, buf, strlen(buf));
	b64_encode(sha1out, sizeof(sha1out), hashbuf, sizeof(hashbuf));

	snprintf(buf, sizeof(buf),
	         "HTTP/1.1 101 Switching Protocols\r\n"
	         "Upgrade: websocket\r\n"
	         "Connection: Upgrade\r\n"
	         "Sec-WebSocket-Accept: %s\r\n\r\n",
	         hashbuf);

	/* Caution: we bypass sendQ flood checking by doing it this way.
	 * Risk is minimal, though, as we only permit limited text only
	 * once per session.
	 */
	dbuf_put(&client->local->sendQ, buf, strlen(buf));
	send_queued(client);

	return 0;
}

int rpc_handle_webrequest_data(Client *client, WebRequest *web, const char *buf, int len)
{
	if (WSU(client))
	{
		/* Websocket user */
		return rpc_handle_body_websocket(client, web, buf, len);
	}

	/* We only handle POST to /api -- reject all the rest */
	if (strcmp(web->uri, "/api") || (web->method != HTTP_METHOD_POST))
	{
		webserver_send_response(client, 404, "Page not found\n");
		return 0;
	}

	// NB: content_length
	// NB: chunked transfers?
	if (!webserver_handle_body(client, web, buf, len))
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

int rpc_handle_body_websocket(Client *client, WebRequest *web, const char *readbuf2, int length2)
{
	return websocket_handle_websocket(client, web, readbuf2, length2, rpc_packet_in_websocket);
}

int rpc_packet_in_websocket(Client *client, char *readbuf, int length)
{
	rpc_call_text(client, readbuf, length);
	return 0; /* and if dead?? */
}

int rpc_packet_in_unix_socket(Client *client, const char *readbuf, int *length)
{
	if (!RPC_PORT(client) || !(client->local->listener->socket_type == SOCKET_TYPE_UNIX) || (*length <= 0))
		return 1; /* Not for us */

	// FIXME: this assumes a single request in 'readbuf' while in fact:
	// - it could only contain partial JSON, eg no ending } yet
	// - there could be multiple requests
	rpc_call_text(client, readbuf, *length);

	return 0;
}

void rpc_close(Client *client)
{
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
		unreal_log(ULOG_INFO, "rpc", "RPC_INVALID_JSON", client,
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

void rpc_sendto(Client *client, const char *buf, int len)
{
	if (MyConnect(client) && IsRPC(client) && WSU(client) && WSU(client)->handshake_completed)
	{
		/* Websocket */
		int utf8bufsize = len*2 + 16;
		char *utf8buf = safe_alloc(utf8bufsize);
		char *newbuf = unrl_utf8_make_valid(buf, utf8buf, utf8bufsize, 1);
		int newlen = strlen(newbuf);
		int ws_sendbufsize = newlen + 64 + ((newlen / 1024) * 64); // some random magic
		char *ws_sendbuf = safe_alloc(ws_sendbufsize);
		websocket_create_packet_ex(WSOP_TEXT, &newbuf, &newlen, ws_sendbuf, ws_sendbufsize);
		dbuf_put(&client->local->sendQ, newbuf, newlen);
		safe_free(ws_sendbuf);
		safe_free(utf8buf);
	} else {
		/* Unix domain socket or HTTP */
		dbuf_put(&client->local->sendQ, buf, len);
		dbuf_put(&client->local->sendQ, "\n", 1);
	}
	mark_data_to_send(client);
}

void _rpc_error(Client *client, json_t *request, JsonRpcError error_code, const char *error_message)
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

	unreal_log(ULOG_INFO, "rpc", "RPC_CALL_ERROR", client,
	           "[rpc] Client $client: RPC call $method",
	           log_data_string("method", method ? method : "<invalid>"));


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
	rpc_sendto(client, json_serialized, strlen(json_serialized));
#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "rpc", "RPC_CALL_DEBUG", client,
		   "[rpc] Client $client: RPC result error: $response",
		   log_data_string("response", json_serialized));
#endif
	json_decref(j);
	safe_free(json_serialized);
}

void _rpc_error_fmt(Client *client, json_t *request, JsonRpcError error_code, const char *fmt, ...)
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
	json_object_set(j, "result", result);

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
	rpc_sendto(client, json_serialized, strlen(json_serialized));
#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "rpc", "RPC_CALL_DEBUG", client,
		   "[rpc] Client $client: RPC response result: $response",
		   log_data_string("response", json_serialized));
#endif
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
	char params_allocated = 0;
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

	handler = RPCHandlerFind(method);
	if (!handler)
	{
		rpc_error(client, request, JSON_RPC_ERROR_METHOD_NOT_FOUND, "Unsupported method");
		return;
	}

	params = json_object_get(request, "params");
	if (!params)
	{
		/* Params is optional, so create an empty params object instead
		 * to make life easier of the RPC handlers (no need to check NULL).
		 */
		params = json_object();
		params_allocated = 1;
	}

	unreal_log(ULOG_INFO, "rpc", "RPC_CALL", client,
	           "[rpc] Client $client: RPC call $method",
	           log_data_string("method", method));

#ifdef DEBUGMODE
	{
		char *call = json_dumps(request, 0);
		if (call)
		{
			unreal_log(ULOG_DEBUG, "rpc", "RPC_CALL_DEBUG", client,
				   "[rpc] Client $client: RPC call: $call",
				   log_data_string("call", call));
			safe_free(call);
		}
	}
#endif
	handler->call(client, request, params);
	if (params_allocated)
		json_decref(params);
}

/** Called very early on accept() of the socket, before TLS is ready */
int rpc_client_accept(Client *client)
{
	if (RPC_PORT(client))
	{
		SetRPC(client);
		client->local->rpc = safe_alloc(sizeof(RPCClient));
	}
	return 0;
}

/** Called upon handshake of unix socket (direct JSON usage, no auth) */
void rpc_client_handshake_unix_socket(Client *client)
{
	if (client->local->listener->socket_type != SOCKET_TYPE_UNIX)
		abort(); /* impossible */

	strlcpy(client->name, "RPC:local", sizeof(client->name));
	SetRPC(client);
	client->local->rpc = safe_alloc(sizeof(RPCClient));
	safe_strdup(client->local->rpc->rpc_user, "<local>");

	/* Allow incoming data to be read from now on.. */
	fd_setselect(client->local->fd, FD_SELECT_READ, read_packet, client);
}

/** Called upon handshake, after TLS is ready (before any HTTP header parsing) */
void rpc_client_handshake_web(Client *client)
{
	RPCUser *r;
	char found = 0;

	/* Explicitly mark as RPC, since the TLS layer may
	 * have set us to SetUnknown() after the TLS handshake.
	 */
	SetRPC(client);
	if (!client->local->rpc)
		client->local->rpc = safe_alloc(sizeof(RPCClient));

	/* Is the client allowed by any rpc-user { } block?
	 * If not, reject the client immediately, before
	 * processing any HTTP data.
	 */
	for (r = rpcusers; r; r = r->next)
	{
		if (user_allowed_by_security_group(client, r->match))
		{
			found = 1;
			break;
		}
	}
	if (!found)
	{
		webserver_send_response(client, 403, "Access denied");
		return;
	}

	/* Allow incoming data to be read from now on.. */
	fd_setselect(client->local->fd, FD_SELECT_READ, read_packet, client);
}

#define RPC_WEBSOCKET_PING_TIME 120

int rpc_pre_local_handshake_timeout(Client *client, const char **comment)
{
	/* Don't hang up websocket connections */
	if (IsRPC(client) && WSU(client) && WSU(client)->handshake_completed)
	{
		long t = TStime() - client->local->last_msg_received;
		if ((t > RPC_WEBSOCKET_PING_TIME*2) && IsPingSent(client))
		{
			*comment = "No websocket PONG received in time.";
			return HOOK_CONTINUE;
		} else
		if ((t > RPC_WEBSOCKET_PING_TIME) && !IsPingSent(client) && !IsDead(client))
		{
			char pingbuf[4];
			const char *pkt = pingbuf;
			int pktlen = sizeof(pingbuf);
			pingbuf[0] = 0x11;
			pingbuf[1] = 0x22;
			pingbuf[2] = 0x33;
			pingbuf[3] = 0x44;
			websocket_create_packet_simple(WSOP_PING, &pkt, &pktlen);
			dbuf_put(&client->local->sendQ, pkt, pktlen);
			send_queued(client);
			SetPingSent(client);
		}
		return HOOK_ALLOW; /* prevent closing the connection due to timeout */
	}

	return HOOK_CONTINUE;
}

RPCUser *find_rpc_user(const char *username)
{
	RPCUser *r;
	for (r = rpcusers; r; r = r->next)
		if (!strcmp(r->name, username))
			return r;
	return NULL;
}

/** This function deals with authentication after the HTTP request was received.
 * It is called for both ordinary HTTP(S) requests and Websockets.
 * Note that there has also been some pre-filtering done in rpc_client_handshake()
 * to see if the IP address was allowed to connect at all (::match),
 * but here we actually check the 'correct' rpc-user { } block.
 * @param client	The client to authenticate
 * @param web		The webrequest (containing the headers)
 * @return 1 on success, 0 on failure
 */
int rpc_handle_auth(Client *client, WebRequest *web)
{
	char *username = NULL, *password = NULL;
	RPCUser *r;

	if (!rpc_parse_auth_basic_auth(client, web, &username, &password) &&
	    !rpc_parse_auth_uri(client, web, &username, &password))
	{
		webserver_send_response(client, 401, "Authentication required");
		return 0;
	}

	if (username && password && ((r = find_rpc_user(username))))
	{
		if (user_allowed_by_security_group(client, r->match) &&
		    Auth_Check(client, r->auth, password))
		{
			/* Authenticated! */
			snprintf(client->name, sizeof(client->name), "RPC:%s", r->name);
			safe_strdup(client->local->rpc->rpc_user, r->name);
			return 1;
		}
	}
	return 0;
}

int rpc_parse_auth_basic_auth(Client *client, WebRequest *web, char **username, char **password)
{
	const char *auth_header = get_nvplist(web->headers, "Authorization");
	static char buf[512];
	char *p;
	int n;

	if (!auth_header)
		return 0;

	/* We only support basic auth */
	if (strncasecmp(auth_header, "Basic ", 6))
		return 0;

	p = strchr(auth_header, ' ');
	skip_whitespace(&p);
	n = b64_decode(p, buf, sizeof(buf)-1);
	if (n <= 1)
		return 0;
	buf[n] = '\0';

	p = strchr(buf, ':');
	if (!p)
		return 0;
	*p++ = '\0';

	*username = buf;
	*password = p;
	return 1;
}

// TODO: the ?a=b&c=d stuff should be urldecoded by 'webserver'
int rpc_parse_auth_uri(Client *client, WebRequest *web, char **username, char **password)
{
	static char buf[2048];
	char *str, *p;

	if (!web->uri)
		return 0;

	strlcpy(buf, web->uri, sizeof(buf));
	str = strstr(buf, "username=");
	if (!str)
		return 0;
	str += 9;
	*username = str;
	p = strchr(str, '&');
	if (p)
	{
		*p++ = '\0';
		p = strstr(p, "password=");
		if (p)
		{
			p += 9;
			*password = p;
			p = strchr(str, '&');
			if (p)
				*p = '\0';
		}
	}
	return 1;
}

RPC_CALL_FUNC(rpc_rpc_info)
{
	json_t *result, *methods, *item;
	RPCHandler *r;

	result = json_object();
	methods = json_object();
	json_object_set_new(result, "methods", methods);

	for (r = rpchandlers; r; r = r->next)
	{
		item = json_object();
		json_object_set_new(item, "name", json_string_unreal(r->method));
		if (r->owner)
		{
			json_object_set_new(item, "module", json_string_unreal(r->owner->header->name));
			json_object_set_new(item, "version", json_string_unreal(r->owner->header->version));
		}
		json_object_set_new(methods, r->method, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}
