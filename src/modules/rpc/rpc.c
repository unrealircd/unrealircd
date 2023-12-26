/* 
 * IRC - Internet Relay Chat, src/modules/rpc/rpc.c
 * RPC module - for remote management of UnrealIRCd
 * (C)Copyright 2022 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 */
   
#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"rpc/rpc",
	"1.0.4",
	"RPC module for remote management",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/** Maximum length of an rpc-user THIS { }.
 * As we use the "RPC:" prefix it is nicklen minus that.
 */
#define RPCUSERLEN (NICKLEN-4)

/** Timers can be minimum every <this> msec */
#define RPC_MINIMUM_TIMER_MSEC 250

#define RRPC_PACKET_SMALL	450
#define RRPC_PACKET_BIGLINES	16000

/* Structs */
typedef struct RPCUser RPCUser;
struct RPCUser {
	RPCUser *prev, *next;
	SecurityGroup *match;
	char *name;
	AuthConfig *auth;
};

typedef struct RRPC RRPC;
struct RRPC {
	RRPC *prev, *next;
	int request;
	char source[IDLEN+1];
	char destination[IDLEN+1];
	char *requestid;
	dbuf data;
};

typedef struct OutstandingRRPC OutstandingRRPC;
struct OutstandingRRPC {
	OutstandingRRPC *prev, *next;
	time_t sent;
	char source[IDLEN+1];
	char destination[IDLEN+1];
	char *requestid;
};

typedef struct RPCTimer RPCTimer;
struct RPCTimer {
	RPCTimer *prev, *next;
	long every_msec;
	Client *client;
	char *timer_id;
	json_t *request;
	struct timeval last_run;
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
void rpc_call_json(Client *client, json_t *request);
void _rpc_response(Client *client, json_t *request, json_t *result);
void _rpc_error(Client *client, json_t *request, JsonRpcError error_code, const char *error_message);
void _rpc_error_fmt(Client *client, json_t *request, JsonRpcError error_code, FORMAT_STRING(const char *fmt), ...) __attribute__((format(printf,4,5)));
void _rpc_send_request_to_remote(Client *source, Client *target, json_t *request);
void _rpc_send_response_to_remote(Client *source, Client *target, json_t *response);
int _rrpc_supported_simple(Client *target, char **problem_server);
int _rrpc_supported(Client *target, const char *module, const char *minimum_version, char **problem_server);
int rpc_handle_auth(Client *client, WebRequest *web);
int rpc_parse_auth_basic_auth(Client *client, WebRequest *web, char **username, char **password);
int rpc_parse_auth_uri(Client *client, WebRequest *web, char **username, char **password);
RPC_CALL_FUNC(rpc_rpc_info);
RPC_CALL_FUNC(rpc_rpc_set_issuer);
RPC_CALL_FUNC(rpc_rpc_add_timer);
RPC_CALL_FUNC(rpc_rpc_del_timer);
CMD_FUNC(cmd_rrpc);
EVENT(rpc_remote_timeout);
EVENT(rpc_do_timers);
json_t *rrpc_data(RRPC *r);
void free_rrpc_list(ModData *m);
void free_outstanding_rrpc_list(ModData *m);
void free_rpc_timer(RPCTimer *r);
void free_rpc_timer_list(ModData *m);
void rpc_call_remote(RRPC *r);
void rpc_response_remote(RRPC *r);
int rpc_handle_free_client(Client *client);
int rpc_handle_server_quit(Client *client, MessageTag *mtags);
int rpc_json_expand_client_server(Client *client, int detail, json_t *j, json_t *child);
const char *rrpc_md_serialize(ModData *m);
void rrpc_md_unserialize(const char *str, ModData *m);
void rrpc_md_free(ModData *m);
int rpc_config_listener(ConfigItem_listen *listener);

/* Macros */
#define RPC_PORT(client)  ((client->local && client->local->listener) ? client->local->listener->rpc_options : 0)
#define WSU(client)     ((WebSocketUser *)moddata_client(client, websocket_md).ptr)

/* Global variables */
ModDataInfo *websocket_md = NULL; /* (imported) */
RPCUser *rpcusers = NULL;
RRPC *rrpc_list = NULL;
OutstandingRRPC *outstanding_rrpc_list = NULL;
RPCTimer *rpc_timer_list = NULL;
ModDataInfo *rrpc_md;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, rpc_config_test_listen);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, rpc_config_test_rpc_user);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_RESPONSE, _rpc_response);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_ERROR, _rpc_error);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_ERROR_FMT, TO_VOIDFUNC(_rpc_error_fmt));
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_SEND_REQUEST_TO_REMOTE, _rpc_send_request_to_remote);
	EfunctionAddVoid(modinfo->handle, EFUNC_RPC_SEND_RESPONSE_TO_REMOTE, _rpc_send_response_to_remote);
	EfunctionAdd(modinfo->handle, EFUNC_RRPC_SUPPORTED, _rrpc_supported);
	EfunctionAdd(modinfo->handle, EFUNC_RRPC_SUPPORTED_SIMPLE, _rrpc_supported_simple);

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
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_QUIT, 0, rpc_handle_server_quit);
	HookAdd(modinfo->handle, HOOKTYPE_FREE_CLIENT, 0, rpc_handle_free_client);
	HookAdd(modinfo->handle, HOOKTYPE_JSON_EXPAND_CLIENT_SERVER, 0, rpc_json_expand_client_server);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIG_LISTENER, 0, rpc_config_listener);

	memset(&r, 0, sizeof(r));
	r.method = "rpc.info";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_rpc_info;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc.info] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "rpc.set_issuer";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_rpc_set_issuer;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc.set_issuer] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "rpc.add_timer";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_rpc_add_timer;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc.add_timer] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "rpc.del_timer";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_rpc_del_timer;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc.del_timer] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "rrpc";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.serialize = rrpc_md_serialize;
	mreq.unserialize = rrpc_md_unserialize;
	mreq.free = rrpc_md_free;
	mreq.sync = 1;
	mreq.self_write = 1;
	rrpc_md = ModDataAdd(modinfo->handle, mreq);
	if (!rrpc_md)
	{
		config_error("[rpc/rpc] Unable to ModDataAdd() -- too many 3rd party modules loaded perhaps?");
		abort();
	}

	LoadPersistentPointer(modinfo, rrpc_list, free_rrpc_list);
	LoadPersistentPointer(modinfo, outstanding_rrpc_list, free_outstanding_rrpc_list);
	LoadPersistentPointer(modinfo, rpc_timer_list, free_rpc_timer_list);

	CommandAdd(modinfo->handle, "RRPC", cmd_rrpc, MAXPARA, CMD_SERVER|CMD_BIGLINES);

	EventAdd(modinfo->handle, "rpc_remote_timeout", rpc_remote_timeout, NULL, 1000, 0);
	EventAdd(modinfo->handle, "rpc_do_timers", rpc_do_timers, NULL, RPC_MINIMUM_TIMER_MSEC, 0);

	/* Call MOD_LOAD very late, since we manage sockets, but depend on websocket_common */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_UNLOAD-1);

	return MOD_SUCCESS;
}

#define MYRRPCMODULES		me.moddata[rrpc_md->slot].ptr
#define RRPCMODULES(client)	((NameValuePrioList *)moddata_client(client, rrpc_md).ptr)

void rpc_do_moddata(void)
{
	Module *m;

	free_nvplist(MYRRPCMODULES);
	MYRRPCMODULES = NULL;

	for (m = Modules; m; m = m->next)
		if (!strncmp(m->header->name, "rpc/", 4))
			add_nvplist((NameValuePrioList **)&MYRRPCMODULES, 0, m->header->name + 4, m->header->version);
}

MOD_LOAD()
{
	rpc_do_moddata();
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
	SavePersistentPointer(modinfo, rrpc_list);
	SavePersistentPointer(modinfo, outstanding_rrpc_list);
	SavePersistentPointer(modinfo, rpc_timer_list);
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

void rpc_listener_set_handler(ConfigItem_listen *l)
{
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
	l->rpc_options = 1;

	return 1;
}

int rpc_config_listener(ConfigItem_listen *listener)
{
	if (listener->rpc_options)
		rpc_listener_set_handler(listener);
	return 0;
}

/** Valid name for rpc-user THISNAME { } ? */
static int valid_rpc_user_name(const char *str)
{
	const char *p;

	if (strlen(str) > RPCUSERLEN)
		return 0;

	for (p = str; *p; p++)
		if (!isalnum(*p) && !strchr("_-", *p))
			return 0;

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
		*errs = 1;
		return -1; /* quick return */
	}

	if (!valid_rpc_user_name(ce->value))
	{
		config_error("%s:%d: rpc-user block has invalid name '%s'. "
		             "Can be max %d long and may only contain a-z, A-Z, 0-9, - and _.",
		             ce->file->filename, ce->line_number,
		             ce->value, RPCUSERLEN);
		errors++;
	}
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "match") || !strcmp(cep->name, "mask"))
		{
			has_match = 1;
			test_match_block(cf, cep, &errors);
		} else
		if (!strcmp(cep->name, "password"))
		{
			has_password = 1;
			if (Auth_CheckError(cep, 0) < 0)
				errors++;
		} else
		{
			config_error_unknown(cep->file->filename,
				cep->line_number, "rpc-user", cep->name);
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
		if (!strcmp(cep->name, "match") || !strcmp(cep->name, "mask"))
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
	char buf[READBUFSIZE];

	if (!RPC_PORT(client) || !(client->local->listener->socket_type == SOCKET_TYPE_UNIX) || (*length <= 0))
		return 1; /* Not for us */

	dbuf_put(&client->local->recvQ, readbuf, *length);

	while (DBufLength(&client->local->recvQ))
	{
		int len = dbuf_getmsg(&client->local->recvQ, buf);
		if (len <= 0)
			break;
		rpc_call_text(client, buf, len);
		if (IsDead(client))
			break;
	}

	return 0;
}

void rpc_close(Client *client)
{
	send_queued(client);

	/* May not be a web request actually, but this works: */
	webserver_close_client(client);
}

/** Handle the RPC request: input is a buffer with a certain length.
 * This calls rpc_call_json()
 */
void rpc_call_text(Client *client, const char *readbuf, int len)
{
	json_t *request = NULL;
	json_error_t jerr;
#if JANSSON_VERSION_HEX >= 0x020100
	const char *buf = readbuf;
	request = json_loadb(buf, len, JSON_REJECT_DUPLICATES, &jerr);
#else
	char buf[2048];

	*buf = '\0';
	strlncpy(buf, readbuf, sizeof(buf), len);

	request = json_loads(buf, JSON_REJECT_DUPLICATES, &jerr);
#endif
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
	rpc_call_json(client, request);
	json_decref(request);
}

void rpc_sendto(Client *client, const char *buf, int len)
{
	if (IsDead(client))
		return;
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
		json_object_set(j, "id", id);

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

	if (MyConnect(client))
		rpc_sendto(client, json_serialized, strlen(json_serialized));
	else
		rpc_send_response_to_remote(&me, client, j);

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
		json_object_set(j, "id", id); /* 'id' is optional */
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

	if (MyConnect(client))
		rpc_sendto(client, json_serialized, strlen(json_serialized));
	else
		rpc_send_response_to_remote(&me, client, j);

#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "rpc", "RPC_CALL_DEBUG", client,
		   "[rpc] Client $client: RPC response result: $response",
		   log_data_string("response", json_serialized));
#endif
	json_decref(j);
	safe_free(json_serialized);
}

int sanitize_params_actual(Client *client, json_t *request, const char *str)
{
	if (!str)
		return 1;

	if (strlen(str) > 510)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_REQUEST, "Strings cannot be longer than 510 characters in the request");
		return 0;
	}

	if (strchr(str, '\n') || strchr(str, '\r'))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_REQUEST, "Strings may not contain \n or \r in the request");
		return 0;
	}

	return 1;
}

int sanitize_params(Client *client, json_t *request, json_t *j)
{
	/* Check the current object itself */
	const char *str = json_string_value(j);
	if (str && !sanitize_params_actual(client, request, str))
		return 0;

	/* Now walk through the object, if needed */

	if (json_is_array(j))
	{
		size_t index;
		json_t *value;
		json_array_foreach(j, index, value)
		{
			if (!sanitize_params(client, request, value))
				return 0;
		}
	} else
	if (json_is_object(j))
	{
		const char *key;
		json_t *value;
		json_object_foreach(j, key, value)
		{
			if (!sanitize_params_actual(client, request, key))
				return 0;
			if (!sanitize_params(client, request, value))
				return 0;
		}
	}

	return 1;
}

/** Log the RPC request */
void rpc_call_log(Client *client, RPCHandler *handler, json_t *request, const char *method, json_t *params)
{
	const char *key;
	json_t *value_object;
	char params_string[512], tbuf[256];

	*params_string = '\0';
	json_object_foreach(params, key, value_object)
	{
		const char *value = json_get_value(value_object);
		if (value)
		{
			snprintf(tbuf, sizeof(tbuf), "%s='%s', ", key, value);
			strlcat(params_string, tbuf, sizeof(params_string));
		}
	}
	if (*params_string)
		params_string[strlen(params_string)-2] = '\0'; /* cut off last comma */

	// TODO: pass log_data_json() or something, pass the entire 'request' ? For JSON logging

	if (client->rpc && client->rpc->issuer)
	{
		if (*params_string)
		{
			unreal_log(handler->loglevel, "rpc", "RPC_CALL", client,
				   "[rpc] RPC call $method by $client ($issuer): $params_string",
				   log_data_string("issuer", client->rpc->issuer),
				   log_data_string("method", method),
				   log_data_string("params_string", params_string));
		} else {
			unreal_log(handler->loglevel, "rpc", "RPC_CALL", client,
				   "[rpc] RPC call $method by $client ($issuer)",
				   log_data_string("issuer", client->rpc->issuer),
				   log_data_string("method", method));
		}
	} else {
		if (*params_string)
		{
			unreal_log(handler->loglevel, "rpc", "RPC_CALL", client,
				   "[rpc] RPC call $method by $client: $params_string",
				   log_data_string("method", method),
				   log_data_string("params_string", params_string));
		} else {
			unreal_log(handler->loglevel, "rpc", "RPC_CALL", client,
				   "[rpc] RPC call $method by $client",
				   log_data_string("method", method));
		}
	}
}

/** Parse an RPC request, except that it does not validate 'params'.
 * @param client	The client issuing the request
 * @param mainrequest	The underlying request that we should send errors to (usually same as 'request')
 * @param request	The request that needs parsing
 * @param method	This will be filled in if successfully parsed
 * @param handler	This will be filled in if the handler is found
 * @retval 0		An error occured while parsing or the method was not found.
 * @retval 1		All good. You still need to validate 'params', though.
 */
int parse_rpc_call(Client *client, json_t *mainrequest, json_t *request, const char **method, RPCHandler **handler)
{
	const char *jsonrpc;
	json_t *id;
	const char *str;

	*method = NULL;
	*handler = NULL;

	jsonrpc = json_object_get_string(request, "jsonrpc");
	if (!jsonrpc || strcasecmp(jsonrpc, "2.0"))
	{
		rpc_error(client, mainrequest, JSON_RPC_ERROR_INVALID_REQUEST, "Only JSON-RPC version 2.0 is supported");
		return 0;
	}

	id = json_object_get(request, "id");
	if (!id)
	{
		rpc_error(client, mainrequest, JSON_RPC_ERROR_INVALID_REQUEST, "Missing 'id'");
		return 0;
	}

	if ((str = json_string_value(id)))
	{
		if (strlen(str) > 32)
		{
			rpc_error(client, mainrequest, JSON_RPC_ERROR_INVALID_REQUEST, "The 'id' cannot be longer than 32 characters in UnrealIRCd JSON-RPC");
			return 0;
		}
		if (strchr(str, '\n') || strchr(str, '\r'))
		{
			rpc_error(client, mainrequest, JSON_RPC_ERROR_INVALID_REQUEST, "The 'id' may not contain \n or \r in UnrealIRCd JSON-RPC");
			return 0;
		}
	} else if (!json_is_integer(id))
	{
		rpc_error(client, mainrequest, JSON_RPC_ERROR_INVALID_REQUEST, "The 'id' must be a string or an integer in UnrealIRCd JSON-RPC");
		return 0;
	}

	*method = json_object_get_string(request, "method");
	if (!*method)
	{
		rpc_error(client, mainrequest, JSON_RPC_ERROR_INVALID_REQUEST, "Missing 'method' to call");
		return 0;
	}

	*handler = RPCHandlerFind(*method);
	if (!*handler)
	{
		rpc_error(client, mainrequest, JSON_RPC_ERROR_METHOD_NOT_FOUND, "Unsupported method");
		return 0;
	}

	return 1;
}

/** Handle the RPC request: request is in JSON */
void rpc_call_json(Client *client, json_t *request)
{
	const char *method;
	json_t *params;
	RPCHandler *handler;

	if (!parse_rpc_call(client, request, request, &method, &handler))
		return; /* Error already returned to caller */

	params = json_object_get(request, "params");
	if (params)
	{
		if (!(handler->flags & RPC_HANDLER_FLAGS_UNFILTERED) &&
		    !sanitize_params(client, request, params))
		{
			return;
		}
	} else
	{
		/* Params is optional, so create an empty params object instead
		 * to make life easier of the RPC handlers (no need to check NULL).
		 */
		params = json_object();
		json_object_set_new(request, "params", params);
	}

	rpc_call_log(client, handler, request, method, params);

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
}

/** Called very early on accept() of the socket, before TLS is ready */
int rpc_client_accept(Client *client)
{
	if (RPC_PORT(client))
	{
		SetRPC(client);
		client->rpc = safe_alloc(sizeof(RPCClient));
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
	client->rpc = safe_alloc(sizeof(RPCClient));
	safe_strdup(client->rpc->rpc_user, "<local>");

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
	if (!client->rpc)
		client->rpc = safe_alloc(sizeof(RPCClient));

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
		webserver_send_response(client, 401, "Access denied");
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
			safe_strdup(client->rpc->rpc_user, r->name);
			return 1;
		}
	}

	/* Authentication failed */
	webserver_send_response(client, 401, "Authentication required");
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

RPC_CALL_FUNC(rpc_rpc_set_issuer)
{
	json_t *result;
	const char *name;
	char buf[512];

	REQUIRE_PARAM_STRING("name", name);

	/* Do some validation on the name */
	strlcpy(buf, name, sizeof(buf));
	if (!do_remote_nick_name(buf) || strcmp(buf, name))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_NAME,
		          "The 'name' contains illegal characters or is too long. "
		          "The same rules as for nick names apply.");
		return;
	}

	safe_strdup(client->rpc->issuer, name);

	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

void free_rrpc(RRPC *r)
{
	safe_free(r->requestid);
	DBufClear(&r->data);
	DelListItem(r, rrpc_list);
	safe_free(r);
}

/* Admin unloading the RPC module for good (not called on rehash) */
void free_rrpc_list(ModData *m)
{
	RRPC *r, *r_next;

	for (r = rrpc_list; r; r = r_next)
	{
		r_next = r->next;
		free_rrpc(r);
	}
}

void free_outstanding_rrpc(OutstandingRRPC *r)
{
	safe_free(r->requestid);
	DelListItem(r, outstanding_rrpc_list);
	safe_free(r);
}

/* Admin unloading the RPC module for good (not called on rehash) */
void free_outstanding_rrpc_list(ModData *m)
{
	OutstandingRRPC *r, *r_next;

	for (r = outstanding_rrpc_list; r; r = r_next)
	{
		r_next = r->next;
		free_outstanding_rrpc(r);
	}
}

/** Remove timer from rpc_timer_list and free it */
void free_rpc_timer(RPCTimer *r)
{
	safe_free(r->timer_id);
	json_decref(r->request);
	DelListItem(r, rpc_timer_list);
	safe_free(r);
}

/* Admin unloading the RPC module for good (not called on rehash) */
void free_rpc_timer_list(ModData *m)
{
	RPCTimer *r, *r_next;

	for (r = rpc_timer_list; r; r = r_next)
	{
		r_next = r->next;
		free_rpc_timer(r);
	}
}

/* Admin unloading the RPC module for good (not called on rehash) */
void free_rpc_timers_for_user(Client *client)
{
	RPCTimer *r, *r_next;

	for (r = rpc_timer_list; r; r = r_next)
	{
		r_next = r->next;
		if (r->client == client)
			free_rpc_timer(r);
	}
}

RPCTimer *find_rpc_timer(Client *client, const char *timer_id)
{
	RPCTimer *r;

	for (r = rpc_timer_list; r; r = r->next)
	{
		if ((r->client == client) && !strcmp(timer_id, r->timer_id))
			return r;
	}
	return NULL;
}

/** When a server quits, cancel all the RPC requests to/from those clients */
int rpc_handle_server_quit(Client *client, MessageTag *mtags)
{
	RRPC *r, *r_next;
	OutstandingRRPC *or, *or_next;

	for (r = rrpc_list; r; r = r_next)
	{
		r_next = r->next;
		if (!strncmp(client->id, r->source, SIDLEN) ||
		    !strncmp(client->id, r->destination, SIDLEN))
		{
			free_rrpc(r);
		}
	}

	for (or = outstanding_rrpc_list; or; or = or_next)
	{
		or_next = or->next;
		if (!strcmp(client->id, or->destination))
		{
			Client *client = find_client(or->source, NULL);
			if (client)
			{
				json_t *j = json_object();
				json_object_set_new(j, "id", json_string_unreal(or->requestid));
				rpc_error(client, NULL, JSON_RPC_ERROR_SERVER_GONE, "Remote server disconnected while processing the request");
				json_decref(j);
			}
			free_outstanding_rrpc(or);
		}
	}

	return 0;
}

EVENT(rpc_remote_timeout)
{
	OutstandingRRPC *or, *or_next;
	time_t deadline = TStime() - 15;

	for (or = outstanding_rrpc_list; or; or = or_next)
	{
		or_next = or->next;
		if (or->sent < deadline)
		{
			Client *client = find_client(or->source, NULL);
			if (client)
			{
				json_t *request = json_object();
				json_object_set_new(request, "id", json_string_unreal(or->requestid));
				rpc_error(client, request, JSON_RPC_ERROR_TIMEOUT, "Request timed out");
				json_decref(request);
			}
			free_outstanding_rrpc(or);
		}
	}
}

RRPC *find_rrpc(const char *source, const char *destination, const char *requestid)
{
	RRPC *r;
	for (r = rrpc_list; r; r = r->next)
	{
		if (!strcmp(r->source, source) &&
		    !strcmp(r->destination, destination) &&
		    !strcmp(r->requestid, requestid))
		{
			return r;
		}
	}
	return NULL;
}

OutstandingRRPC *find_outstandingrrpc(const char *source, const char *requestid)
{
	OutstandingRRPC *r;
	for (r = outstanding_rrpc_list; r; r = r->next)
	{
		if (!strcmp(r->source, source) &&
		    !strcmp(r->requestid, requestid))
		{
			return r;
		}
	}
	return NULL;
}

void rrpc_pass_on_split(Client *client, Client *dest, MessageTag *recv_mtags, const char *parv[])
{
	char buf[MAXLINELENGTH];
	char *data;
	char saved;
	char status[8];
	char first = 0;
	char continuation = 0;
	char final = 0;
	int remaining;

	strlcpy(buf, parv[6], sizeof(buf));

	if (strchr(parv[5], 'S'))
		first = 1;
	if (strchr(parv[5], 'C'))
		continuation = 2;
	if (strchr(parv[5], 'F'))
		final = 1;

	remaining = strlen(buf);
	for (data = buf; remaining && *data; data += RRPC_PACKET_SMALL)
	{
		if (remaining > RRPC_PACKET_SMALL)
		{
			saved = data[RRPC_PACKET_SMALL];
			data[RRPC_PACKET_SMALL] = '\0';
			remaining -= RRPC_PACKET_SMALL;
		} else {
			saved = 0;
			remaining = 0;
			continuation = 0;
		}

		*status = '\0';
		if (first)
		{
			strlcat_letter(status, 'S', sizeof(status));
			first = 0;
		}
		if (!saved && final)
		{
			/* Send 'F' - but not just when its the last chunk,
			 * only if the original also indicated it is the
			 * last chunk (caveat :D).
			 */
			strlcat_letter(status, 'F', sizeof(status));
		} else
		if (!first)
		{
			strlcat_letter(status, 'C', sizeof(status));
		}

		sendto_one(dest, recv_mtags, ":%s RRPC %s %s %s %s %s :%s",
			   client->id, parv[1], parv[2], parv[3], parv[4], status, data);

		if (!saved)
			break; /* done! */

		data[RRPC_PACKET_SMALL] = saved;
	}
}

/* Remote RPC call over the network (RRPC)
 * :<server> RRPC <REQ|RES> <source> <destination> <requestid> [S|C|F] :<request data>
 * S = Start
 * C = Continuation
 * F = Finish
 */
CMD_FUNC(cmd_rrpc)
{
	int request;
	const char *source, *destination, *requestid, *type, *data;
	RRPC *r;
	Client *dest;
	char sid[SIDLEN+1];
	char binarydata[BUFSIZE+1];
	int binarydatalen;

	if ((parc < 7) || BadPtr(parv[6]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "RRPC");
		return;
	}

	if (!strcmp(parv[1], "REQ"))
	{
		request = 1;
	} else if (!strcmp(parv[1], "RES"))
	{
		request = 0;
	} else {
		sendnumeric(client, ERR_CANNOTDOCOMMAND, "RRPC", "Invalid parameter");
		return;
	}

	source = parv[2];
	destination = parv[3];
	requestid = parv[4];
	type = parv[5];
	data = parv[6];

	/* Search by SID (first 3 characters of destination)
	 * so we can always deliver, even for unknown UID destinations
	 * in case this is a response.
	 */
	strlcpy(sid, destination, sizeof(sid));
	dest = find_server_quick(sid);
	if (!dest)
	{
		sendnumeric(client, ERR_NOSUCHSERVER, sid);
		return;
	}

	if (dest != &me)
	{
		/* Not for us, pass it along... */
		if ((strlen(parv[6]) > RRPC_PACKET_SMALL) && !SupportBIGLINES(dest->direction))
		{
			/* Hard case */
			rrpc_pass_on_split(client, dest, recv_mtags, parv);
		} else {
			/* Simple case */
			sendto_one(dest, recv_mtags, ":%s RRPC %s %s %s %s %s :%s",
			           client->id, parv[1], parv[2], parv[3], parv[4], parv[5], parv[6]);
		}
		return;
	}

	/* It's for us! So handle it ;) */

	if (strchr(type, 'S'))
	{
		r = find_rrpc(source, destination, requestid);
		if (r)
		{
			sendnumeric(client, ERR_CANNOTDOCOMMAND, "RRPC", "Duplicate request found");
			/* We actually terminate the existing RRPC as well,
			 * because there's a big risk of the the two different ones
			 * merging in subsequent RRPC... C ... commands. Bad!
			 * (and yeah this does not handle the case where you have
			 *  like 3 or more duplicate request id requests... so be it..)
			 */
			free_rrpc(r);
			return;
		}
		/* A new request */
		r = safe_alloc(sizeof(RRPC));
		strlcpy(r->source, source, sizeof(r->source));
		strlcpy(r->destination, destination, sizeof(r->destination));
		safe_strdup(r->requestid, requestid);
		r->request = request;
		dbuf_queue_init(&r->data);
		AddListItem(r, rrpc_list);
	} else
	if (strchr(type, 'C') || strchr(type, 'F'))
	{
		r = find_rrpc(source, destination, requestid);
		if (!r)
		{
			sendnumeric(client, ERR_CANNOTDOCOMMAND, "RRPC", "Request not found");
			return;
		}
	} else
	{
		sendnumeric(client, ERR_CANNOTDOCOMMAND, "RRPC", "Only actions S/C/F are supported");
		return;
	}

	/* Append the data */
	dbuf_put(&r->data, data, strlen(data));

	/* Now check if the request happens to be terminated */
	if (strchr(type, 'F'))
	{
		if (r->request)
			rpc_call_remote(r);
		else
			rpc_response_remote(r);
		free_rrpc(r);
		return;
	}
}

/** Convert the RRPC data to actual workable JSON output */
json_t *rrpc_data(RRPC *r)
{
	int datalen;
	char *data;
	json_t *j;
	json_error_t jerr;

	datalen = dbuf_get(&r->data, &data);
	j = json_loads(data, JSON_REJECT_DUPLICATES, &jerr);
	safe_free(data);

	return j;
}

/** Received a remote RPC request (from a client on another server) */
void rpc_call_remote(RRPC *r)
{
	json_t *request = NULL;
	Client *server;
	Client *client;
	char sid[SIDLEN+1];

	request = rrpc_data(r);
	if (!request)
	{
		// TODO: handle invalid JSON
		return;
	}

	/* Create a (fake) client structure */
	strlcpy(sid, r->source, sizeof(sid));
	server = find_server_quick(sid);
	if (!server)
	{
		return;
	}
	client = make_client(server->direction, server);
	strlcpy(client->id, r->source, sizeof(client->id));
	client->rpc = safe_alloc(sizeof(RPCClient));
	strlcpy(client->name, "RPC:remote", sizeof(client->name));
	safe_strdup(client->rpc->rpc_user, "<remote>");
	// Note: NOT added to hash table or id table etc.
	list_add(&client->client_node, &rpc_remote_list);
	rpc_call_json(client, request);
	json_decref(request);

	/* And free the temporary client, unless it is async... */
	if (!IsAsyncRPC(client))
		free_client(client);
}

/** Received a remote RPC response (from another server) to our local RPC client */
void rpc_response_remote(RRPC *r)
{
	OutstandingRRPC *or;
	Client *client = find_client(r->destination, NULL);
	json_t *json, *j;

	if (!client)
		return;

	or = find_outstandingrrpc(client->id, r->requestid);
	if (!or)
		return; /* Not a known outstanding request, maybe the client left already */

	json = rrpc_data(r);
	if (!json)
		return;

	if ((j = json_object_get(json, "result")))
	{
		rpc_response(client, json, j);
	} else if ((j = json_object_get(json, "error")))
	{
		json_t *x;
		int errorcode = 0;
		const char *error_message = json_object_get_string(j, "message");
		if ((x = json_object_get(j, "errorcode")))
			errorcode = json_integer_value(x);
		if (errorcode == 0)
			errorcode = JSON_RPC_ERROR_INTERNAL_ERROR;
		rpc_error(client, json, errorcode, error_message ? error_message : "");
	}

	json_decref(json);

	free_outstanding_rrpc(or);
}

const char *rpc_id(json_t *request)
{
	static char rid[128];
	const char *requestid;
	json_t *j;

	j = json_object_get(request, "id");
	if (!j)
		return NULL;

	requestid = json_string_value(j);
	if (!requestid)
	{
		json_int_t v = json_integer_value(j);
		if (v == 0)
			return NULL;
		snprintf(rid, sizeof(rid), "%lld", (long long)v);
		requestid = rid;
	}

	return requestid;
}

/** Send a remote RPC (RRPC) request 'request' to server 'target'. */
void rpc_send_generic_to_remote(Client *source, Client *target, const char *requesttype, json_t *json)
{
	char *json_serialized;
	json_t *j;
	const char *type;
	const char *requestid;
	char *str;
	int bytes; /* bytes in this frame */
	int bytes_remaining; /* bytes remaining overall */
	int start_frame = 1; /* set to 1 if this is the start frame */
	int packet_split_size; /* chunk size of outgoing packets (depends on BIGLINES support) */
	char data[RRPC_PACKET_BIGLINES+1];

	requestid = rpc_id(json);
	if (!requestid)
		return;

	json_serialized = json_dumps(json, 0);
	if (!json_serialized)
		return;

	/* :<server> RRPC REQ <source> <destination> <requestid> [S|C|F] :<request data>
	 * S = Start
	 * C = Continuation
	 * F = Finish
	 */

	if (SupportBIGLINES(target->direction))
		packet_split_size = RRPC_PACKET_BIGLINES;
	else
		packet_split_size = RRPC_PACKET_SMALL;

	bytes_remaining = strlen(json_serialized);
	for (str = json_serialized, bytes = MIN(bytes_remaining, packet_split_size);
	     str && *str && bytes_remaining;
	     str += bytes, bytes = MIN(bytes_remaining, packet_split_size))
	{
		bytes_remaining -= bytes;
		if (start_frame == 1)
		{
			start_frame = 0;
			if (bytes_remaining > 0)
				type = "S"; /* start (with later continuation frames) */
			else
				type = "SF"; /* start and finish */
		} else
		if (bytes_remaining > 0)
		{
			type = "C"; /* continuation frame (with later a finish frame) */
		} else {
			type = "F"; /* finish frame (the last frame) */
		}

		strlncpy(data, str, packet_split_size+1, bytes);

		sendto_one(target, NULL, ":%s RRPC %s %s %s %s %s :%s",
		           me.id,
		           requesttype,
		           source->id,
		           target->id,
		           requestid,
		           type,
		           data);
	}

	safe_free(json_serialized);
}

int _rrpc_supported_simple(Client *target, char **problem_server)
{
	if (!moddata_client_get(target, "rrpc"))
	{
		if (problem_server)
			*problem_server = target->name;
		return 0;
	}
	if ((target != target->direction) && !rrpc_supported_simple(target->direction, problem_server))
		return 0;
	return 1;
}

int _rrpc_supported(Client *target, const char *module, const char *minimum_version, char **problem_server)
{
	if (!moddata_client_get(target, "rrpc"))
	{
		if (problem_server)
			*problem_server = target->name;
		return 0;
	}
	if ((target != target->direction) && !rrpc_supported_simple(target->direction, problem_server))
		return 0;
	return 1;
}

/** Send a remote RPC (RRPC) request 'request' to server 'target'. */
void _rpc_send_request_to_remote(Client *source, Client *target, json_t *request)
{
	OutstandingRRPC *r;
	const char *requestid = rpc_id(request);
	char *problem_server = NULL;

	if (!requestid)
	{
		/* should never happen, since already covered upstream, but just to be sure... */
		rpc_error(source, NULL, JSON_RPC_ERROR_INVALID_REQUEST, "The 'id' must be a string or an integer in UnrealIRCd JSON-RPC");
		return;
	}

	if (find_outstandingrrpc(source->id, requestid))
	{
		rpc_error(source, NULL, JSON_RPC_ERROR_INVALID_REQUEST, "A request with that id is already in progress. Use unique id's!");
		return;
	}

	/* If we already detect that next server cannot satisfy the request then stop it right now */
	if (!rrpc_supported_simple(target, &problem_server))
	{
		rpc_error_fmt(source, request, JSON_RPC_ERROR_REMOTE_SERVER_NO_RPC, "Server %s does not support remote JSON-RPC", problem_server);
		return;
	}

	/* Add the request to the "Outstanding RRPC list" */
	r = safe_alloc(sizeof(OutstandingRRPC));
	r->sent = TStime();
	strlcpy(r->source, source->id, sizeof(r->source));
	strlcpy(r->destination, target->id, sizeof(r->destination));
	safe_strdup(r->requestid, requestid);
	AddListItem(r, outstanding_rrpc_list);

	/* And send it! */
	rpc_send_generic_to_remote(source, target, "REQ", request);
}

/** Send a remote RPC (RRPC) request 'request' to server 'target'. */
void _rpc_send_response_to_remote(Client *source, Client *target, json_t *response)
{
	rpc_send_generic_to_remote(source, target, "RES", response);
}

const char *rrpc_md_serialize(ModData *m)
{
	static char buf[512];
	char tmp[128];
	NameValuePrioList *nv;

	if (m->ptr == NULL)
		return NULL;

	*buf = '\0';
	for (nv = m->ptr; nv; nv = nv->next)
	{
		snprintf(tmp, sizeof(tmp), "%s:%s,", nv->name, nv->value);
		strlcat(buf, tmp, sizeof(buf));
	}
	if (*buf)
		buf[strlen(buf)-1] = '\0'; // strip last comma

	return buf;
}

void rrpc_md_unserialize(const char *str, ModData *m)
{
	char buf[1024], *p, *name, *value;

	/* First free everything if needed */
	if (m->ptr)
	{
		free_nvplist(m->ptr);
		m->ptr = NULL;
	}

	if (BadPtr(str))
		return; /* Done already */

	strlcpy(buf, str, sizeof(buf));
	for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
	{
		value = strchr(name, ':');
		if (!value)
			continue;
		*value++ = '\0';
		add_nvplist((NameValuePrioList **)&m->ptr, 0, name, value);
	}
}

void rrpc_md_free(ModData *m)
{
	if (m->ptr)
	{
		free_nvplist(m->ptr);
		m->ptr = NULL;
	}
}

int rpc_json_expand_client_server(Client *client, int detail, json_t *j, json_t *child)
{
	NameValuePrioList *nv = RRPCMODULES(client);
	json_t *rpc_modules;

	if (!nv || (detail < 2))
		return 0;

	/* All this belongs under 'features' */
	child = json_object_get(child, "features");
	if (!child)
		return 0;

	rpc_modules = json_array();
	json_object_set_new(child, "rpc_modules", rpc_modules);
	for (; nv; nv = nv->next)
	{
		json_t *e = json_object();
		json_object_set_new(e, "name", json_string_unreal(nv->name));
		json_object_set_new(e, "version", json_string_unreal(nv->value));
		json_array_append_new(rpc_modules, e);
	}
	return 0;
}

RPC_CALL_FUNC(rpc_rpc_add_timer)
{
	json_t *result;
	json_t *subrequest;
	long every_msec;
	const char *timer_id;
	const char *method;
	RPCHandler *handler;
	RPCTimer *timer;

	REQUIRE_PARAM_INTEGER("every_msec", every_msec);
	REQUIRE_PARAM_STRING("timer_id", timer_id);

	subrequest = json_object_get(params, "request");
	if (!subrequest)
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: '%s'", "request");
		return;
	}

	if (every_msec < RPC_MINIMUM_TIMER_MSEC)
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS,
		              "Value for every_msec may not be less than %d",
		              (int)RPC_MINIMUM_TIMER_MSEC);
		return;
	}

	/* Do some validation on the name */
	if (!parse_rpc_call(client, request, subrequest, &method, &handler))
		return; /* Error already returned to caller */

	/* We don't validate 'params' here, but do so at runtime */

	/* Check if a timer with that same name already exists FOR THIS CLIENT */
	if (find_rpc_timer(client, timer_id))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "Timer already exists with timer_id '%s'", timer_id);
		return;
	}

	timer = safe_alloc(sizeof(RPCTimer));
	timer->every_msec = every_msec;
	timer->client = client;
	safe_strdup(timer->timer_id, timer_id);
	json_incref(subrequest);
	timer->request = subrequest;
	AddListItem(timer, rpc_timer_list);
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

EVENT(rpc_do_timers)
{
	RPCTimer *e, *e_next;

	for (e = rpc_timer_list; e; e = e_next)
	{
		e_next = e->next;
		if (minimum_msec_since_last_run(&e->last_run, e->every_msec))
		{
			rpc_call_json(e->client, e->request);
		}
		// TODO: maybe do counts as well?
	}
}

/** Client being freed? If RPC then cancel timers, if any */
int rpc_handle_free_client(Client *client)
{
	if (IsRPC(client))
		free_rpc_timers_for_user(client);
	return 0;
}

RPC_CALL_FUNC(rpc_rpc_del_timer)
{
	const char *timer_id;
	RPCTimer *r;
	json_t *result;

	REQUIRE_PARAM_STRING("timer_id", timer_id);

	r = find_rpc_timer(client, timer_id);
	if (!r)
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_NOT_FOUND, "Timer not found with timer_id '%s'", timer_id);
		return;
	}
	free_rpc_timer(r);

	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}
