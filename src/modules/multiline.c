/*
 *   IRC - Internet Relay Chat, src/modules/multiline.c
 *   (C) 2026 Syzop & The UnrealIRCd Team
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

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"multiline",
	"1.0",
	"IRCv3 draft/multiline",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* ===================== CONFIGURATION ===================== */

struct {
	int batch_timeout;
} cfg;

/* ===================== DATA STRUCTURES ===================== */

/** State for a locally-initiated multiline batch (one per local client) */
typedef struct MultilineBatch MultilineBatch;
struct MultilineBatch {
	char batch_id[MAXBATCHREFLEN+1];	/**< Client-chosen batch reference tag */
	char *target;			/**< Target channel or nick */
	SendType sendtype;		/**< SEND_TYPE_PRIVMSG or SEND_TYPE_NOTICE */
	int sendtype_set;		/**< Has sendtype been determined (from first line)? */
	char member_modes[2];		/**< Member mode filter from STATUSMSG prefix (e.g. "o"), or empty string */
	MessageTag *client_mtags;	/**< Tags from the opening BATCH command */
	int line_count;
	int received_bytes;		/**< Total user-sent content bytes (for max-bytes policy) */
	time_t start_time;
	int failed;			/**< Batch marked as failed — consume remaining lines, send error at BATCH close */
	char *fail_message;		/**< FAIL response to send at BATCH close (if failed) */
	char label[256];		/**< Saved label for echo-message + labeled-response interaction */
	/* Buffered lines */
	MLine *lines;
	MLine *lines_tail;
	MLine *fallback_lines;	/**< Cached fallback lines (built once, reused for all non-multiline clients) */
};

/** State for an S2S multiline batch being received from a remote server */
typedef struct S2SMultilineBatch S2SMultilineBatch;
struct S2SMultilineBatch {
	S2SMultilineBatch *prev, *next;
	Client *sender;			/**< Remote user who initiated */
	Client *direction;		/**< Server link it came from */
	char batch_id[BATCHLEN+1];
	char *target;
	int is_channel;			/**< 1 if target is a channel, 0 if user */
	SendType sendtype;
	int sendtype_set;
	char member_modes[2];		/**< Member mode filter from STATUSMSG prefix */
	MessageTag *first_mtags;	/**< Message tags from the opening BATCH (for relay) */
	int line_count;
	int received_bytes;
	time_t start_time;
	MLine *lines;
	MLine *lines_tail;
};

/* ===================== FORWARD DECLARATIONS ===================== */

/* Module callbacks */
int multiline_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int multiline_config_run(ConfigFile *cf, ConfigEntry *ce, int type);

/* Command overrides */
CMD_OVERRIDE_FUNC(multiline_override_batch);
CMD_OVERRIDE_FUNC(multiline_override_msg);

/* Message tag handler */
int multiline_concat_mtag_is_ok(Client *client, const char *name, const char *value);

/* Capability parameter callback */
const char *multiline_capability_parameter(Client *client);

/* ModData free callback */
void multiline_mdata_free(ModData *m);

/* Hooks */
int multiline_close_connection(Client *client);
int multiline_remote_quit(Client *client, MessageTag *mtags, const char *comment);
int multiline_server_quit(Client *client, MessageTag *mtags);
int multiline_known_user_cache_change(Client *client);

/* Timer */
EVENT(multiline_timeout_check);

/* Core functions */
static void multiline_deliver(Client *client, MultilineBatch *batch);
static void multiline_deliver_channel(Client *client, MultilineBatch *batch, Channel *channel);
static void multiline_deliver_user(Client *client, MultilineBatch *batch, Client *target);
static void multiline_send_batch_to_client(Client *to, Client *from, MultilineBatch *batch, MessageTag *base_mtags, const char *targetstr, const char *cmd);
static void multiline_send_fallback_to_client(Client *to, Client *from, MultilineBatch *batch, MessageTag *base_mtags, const char *targetstr, const char *cmd);
static void multiline_echo_to_sender(Client *client, MultilineBatch *batch, MessageTag *mtags, const char *targetstr, const char *cmd);
static void multiline_send_s2s_to_direction(Client *direction, Client *from, MultilineBatch *batch, MessageTag *base_mtags, const char *cmd, const char *targetstr, const char *ref);
static void multiline_send_s2s_channel(Client *from, MultilineBatch *batch, Channel *channel, MessageTag *base_mtags, const char *cmd, Client *skip_direction, const char *targetstr);
static void multiline_send_s2s_user(Client *from, MultilineBatch *batch, Client *target, MessageTag *base_mtags, const char *cmd);
static void multiline_deliver_to_local_members(Channel *channel, Client *from, MultilineBatch *batch, MessageTag *mtags, const char *targetstr, const char *cmd, const char *filter_modes, Client *skip, int sendflags);
static void multiline_run_chanmsg_hooks(Client *sender, Channel *channel, int sendflags, const char *member_modes, const char *targetstr, MessageTag *mtags, MLine *lines, SendType sendtype);
static void multiline_run_usermsg_hooks(Client *sender, Client *target, MessageTag *mtags, MLine *lines, SendType sendtype);
static void multiline_fail_batch(Client *client, MultilineBatch *batch, const char *fail_msg);
static void multiline_abort_batch(Client *client, MultilineBatch *batch);
static void multiline_append_line(MLine **lines, MLine **lines_tail, int *line_count, const char *text, int is_concat);
static void multiline_free_lines(MLine *lines);
static void multiline_free_batch(MultilineBatch *batch);
static char *multiline_concat_text(MultilineBatch *batch);

/* S2S receiving */
static void multiline_handle_s2s_batch_open(Client *client, MessageTag *recv_mtags, int parc, const char *parv[], int is_channel);
static void multiline_handle_s2s_batch_close(Client *client, const char *batch_id);
static void multiline_handle_s2s_msg(Client *client, MessageTag *recv_mtags, int parc, const char *parv[], SendType sendtype);
static S2SMultilineBatch *multiline_find_s2s_batch(Client *sender, const char *batch_id);
static void multiline_free_s2s_batch(S2SMultilineBatch *s2s);
static void multiline_deliver_s2s_batch(S2SMultilineBatch *s2s);
static void multiline_free_s2s_batches_all(ModData *m);

/* ===================== VARIABLES ===================== */

static long CAP_MULTILINE = 0L;
static long CAP_BATCH = 0L;
static long CAP_NOTIFY = 0L;
#define HasMultiline(c) (HasCapabilityFast(c, CAP_MULTILINE) && HasCapabilityFast(c, CAP_BATCH))
static ModDataInfo *multiline_md = NULL;
static S2SMultilineBatch *s2s_batches = NULL;

/* ===================== MODULE LIFECYCLE ===================== */

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, multiline_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	LoadPersistentPointer(modinfo, s2s_batches, multiline_free_s2s_batches_all);

	/* Register draft/multiline capability */
	memset(&cap, 0, sizeof(cap));
	cap.name = "draft/multiline";
	cap.parameter = multiline_capability_parameter;
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_MULTILINE);

	/* Register draft/multiline-concat message tag */
	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "draft/multiline-concat";
	mtag.is_ok = multiline_concat_mtag_is_ok;
	mtag.clicap_handler = c;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	/* Register ModData for per-client multiline state */
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "multiline";
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	mreq.free = multiline_mdata_free;
	mreq.serialize = NULL;
	mreq.unserialize = NULL;
	mreq.sync = 0;
	multiline_md = ModDataAdd(modinfo->handle, mreq);

	/* Command overrides */
	CommandOverrideAdd(modinfo->handle, "BATCH", 0, multiline_override_batch);
	CommandOverrideAdd(modinfo->handle, "PRIVMSG", 0, multiline_override_msg);
	CommandOverrideAdd(modinfo->handle, "NOTICE", 0, multiline_override_msg);

	/* Hooks */
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, multiline_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_CLOSE_CONNECTION, 0, multiline_close_connection);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, multiline_remote_quit);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_QUIT, 0, multiline_server_quit);
	HookAdd(modinfo->handle, HOOKTYPE_KNOWN_USER_CACHE_CHANGE, 1000000, multiline_known_user_cache_change); /* (prio: near-last) */

	/* Timer for batch timeout */
	EventAdd(modinfo->handle, "multiline_timeout", multiline_timeout_check, NULL, 5000, 0);

	/* Set default batch timeout (multiline limits are in FloodSettings via conf.c) */
	cfg.batch_timeout = 15;

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	CAP_BATCH = ClientCapabilityBit("batch");
	CAP_NOTIFY = ClientCapabilityBit("cap-notify");
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, s2s_batches);
	return MOD_SUCCESS;
}

/* ===================== CAPABILITY ===================== */

const char *multiline_capability_parameter(Client *client)
{
	static char buf[128];
	FloodSettings *f = get_floodsettings_for_user(client, FLD_MULTILINE);

	snprintf(buf, sizeof(buf), "max-bytes=%d,max-lines=%d",
		(int)f->period[FLD_MULTILINE],
		(int)f->limit[FLD_MULTILINE]);
	return buf;
}

/** Called when a user's known-user status changes.
 * Sends an updated CAP NEW with the new multiline parameters
 * so the client knows its updated max-lines/max-bytes limits.
 */
int multiline_known_user_cache_change(Client *client)
{
	if (!MyConnect(client))
		return HOOK_CONTINUE;

	if (!HasCapabilityFast(client, CAP_NOTIFY))
		return HOOK_CONTINUE;

	if (client->local->cap_protocol >= 302)
	{
		const char *args = multiline_capability_parameter(client);
		sendto_one(client, NULL, ":%s CAP %s NEW :draft/multiline=%s",
			me.name, (*client->name ? client->name : "*"), args);
	} else {
		sendto_one(client, NULL, ":%s CAP %s NEW :draft/multiline",
			me.name, (*client->name ? client->name : "*"));
	}

	return HOOK_CONTINUE;
}

/* ===================== MESSAGE TAG ===================== */

int multiline_concat_mtag_is_ok(Client *client, const char *name, const char *value)
{
	/* Tag must have no value per spec */
	if (!BadPtr(value))
		return 0;

	if (IsServer(client))
		return 1;

	/* Allow from local users who have an active multiline batch */
	if (MyUser(client))
	{
		MultilineBatch *batch = moddata_local_client(client, multiline_md).ptr;
		if (batch)
			return 1;
	}

	return 0;
}

/* ===================== CONFIGURATION ===================== */

int multiline_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "multiline"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: blank set::multiline::%s without value",
			             cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}

		if (!strcmp(cep->name, "batch-timeout"))
		{
			int v = atoi(cep->value);
			if (v < 5 || v > 120)
			{
				config_error("%s:%i: set::multiline::batch-timeout must be between 5 and 120 seconds",
				             cep->file->filename, cep->line_number);
				errors++;
			}
		} else
		{
			config_error("%s:%i: unknown directive set::multiline::%s",
			             cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int multiline_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "multiline"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "batch-timeout"))
			cfg.batch_timeout = atoi(cep->value);
	}

	return 1;
}

/* ===================== MODDATA ===================== */

void multiline_mdata_free(ModData *m)
{
	MultilineBatch *batch = m->ptr;
	if (batch)
	{
		multiline_free_batch(batch);
		m->ptr = NULL;
	}
}

/* ===================== HELPER FUNCTIONS ===================== */

/** Find channel for a target that may have a STATUSMSG prefix (e.g. "@#channel") */
static inline Channel *multiline_find_channel(const char *target)
{
	const char *p = strchr(target, '#');
	return p ? find_channel(p) : NULL;
}

/** Duplicate a message tag list, excluding a specific tag name.
 * @param mtags       Source tag list
 * @param exclude     Tag name to exclude
 * @returns           New tag list (caller must free with free_message_tags())
 */
static MessageTag *duplicate_mtags_excluding(MessageTag *mtags, const char *exclude)
{
	MessageTag *out = NULL, *m, *dup;

	for (m = mtags; m; m = m->next)
	{
		if (!strcmp(m->name, exclude))
			continue;
		dup = duplicate_mtag(m);
		AddListItem(dup, out);
	}
	return out;
}

/** Run HOOKTYPE_CHANMSG per-line for a multiline batch.
 * First line carries mtags (incl msgid), subsequent lines don't.
 */
static void multiline_run_chanmsg_hooks(Client *sender, Channel *channel,
                                        int sendflags, const char *member_modes, const char *targetstr,
                                        MessageTag *mtags, MLine *lines, SendType sendtype)
{
	MLine *line;

	echo_message_inhibit = 1;
	history_inhibit = 1;
	for (line = lines; line; line = line->next)
	{
		RunHook(HOOKTYPE_CHANMSG, sender, channel, sendflags,
			member_modes, targetstr,
			(line == lines) ? mtags : NULL,
			line->text, sendtype);
	}
	history_inhibit = 0;
	echo_message_inhibit = 0;

	/* Fire multiline hook for atomic history storage etc. */
	RunHook(HOOKTYPE_CHANMSG_MULTILINE, sender, channel, sendflags,
		member_modes, targetstr, mtags, lines, sendtype);
}

/** Run HOOKTYPE_USERMSG per-line for a multiline batch.
 * First line carries mtags (incl msgid), subsequent lines don't.
 */
static void multiline_run_usermsg_hooks(Client *sender, Client *target,
                                        MessageTag *mtags, MLine *lines, SendType sendtype)
{
	MLine *line;

	echo_message_inhibit = 1;
	history_inhibit = 1;
	for (line = lines; line; line = line->next)
	{
		RunHook(HOOKTYPE_USERMSG, sender, target,
			(line == lines) ? mtags : NULL,
			line->text, sendtype);
	}
	history_inhibit = 0;
	echo_message_inhibit = 0;
}

/** Free a linked list of multiline lines */
static void multiline_free_lines(MLine *lines)
{
	MLine *l, *l_next;
	for (l = lines; l; l = l_next)
	{
		l_next = l->next;
		safe_free(l->text);
		safe_free(l);
	}
}

/** Free a local multiline batch (but not the ModData slot itself) */
static void multiline_free_batch(MultilineBatch *batch)
{
	if (!batch)
		return;
	safe_free(batch->target);
	safe_free(batch->fail_message);
	free_message_tags(batch->client_mtags);
	multiline_free_lines(batch->lines);
	multiline_free_lines(batch->fallback_lines);
	safe_free(batch);
}

/** Append a line to a multiline line list */
static void multiline_append_line(MLine **lines, MLine **lines_tail,
                                  int *line_count, const char *text, int is_concat)
{
	MLine *line = safe_alloc(sizeof(MLine));
	safe_strdup(line->text, text ? text : "");
	line->concat = is_concat;
	line->next = NULL;

	if (*lines_tail)
		(*lines_tail)->next = line;
	else
		*lines = line;
	*lines_tail = line;
	(*line_count)++;
}

/** Calculate the multiline batch fake lag in milliseconds.
 * Uses the same per-line formula as parse.c with a floor of
 * 1 extra line and a hard cap of 15 seconds.
 * FIXME: reconsider exact algo
 */
static long calculate_multiline_fakelag(Client *client, MultilineBatch *batch)
{
	FloodSettings *settings;
	int lag_penalty, lag_penalty_bytes;
	long lag_msec;
	MLine *l;

	if (!batch->lines)
		return 0;

	settings = get_floodsettings_for_user(client, FLD_LAG_PENALTY);
	lag_penalty = settings->period[FLD_LAG_PENALTY];
	lag_penalty_bytes = settings->limit[FLD_LAG_PENALTY];
	if (lag_penalty_bytes < 1)
		lag_penalty_bytes = 1;

	lag_msec = lag_penalty; /* floor: 1 extra line */
	for (l = batch->lines; l; l = l->next)
		lag_msec += (1 + ((int)strlen(l->text) / lag_penalty_bytes)) * lag_penalty;
	if (lag_msec > 15000)
		lag_msec = 15000;
	return lag_msec;
}

/** Mark a local multiline batch as failed.
 * The batch stays open so remaining lines are silently consumed.
 * The error is sent when the client sends BATCH -ref.
 * @param fail_msg  The FAIL response line (after ":<server> "), e.g.
 *                  "FAIL BATCH MULTILINE_MAX_LINES 5 :Too many lines in batch"
 */
static void multiline_fail_batch(Client *client, MultilineBatch *batch, const char *fail_msg)
{
	if (!batch)
		return;
	if (batch->failed)
		return; /* Already marked, keep first error */
	batch->failed = 1;
	safe_strdup(batch->fail_message, fail_msg);
}

/** Abort a local multiline batch: apply half fake lag penalty and free state.
 * Called at BATCH close for failed batches, or for fatal errors where
 * the batch state must be freed immediately.
 */
static void multiline_abort_batch(Client *client, MultilineBatch *batch)
{
	if (!batch)
		return;

	/* Apply half the batch fake lag penalty for aborted batches */
	add_fake_lag(client, 2000 + calculate_multiline_fakelag(client, batch) / 2);

	multiline_free_batch(batch);
	moddata_local_client(client, multiline_md).ptr = NULL;
}

/** Calculate the number of bytes a line adds to a multiline batch.
 * Accounts for the \n separator between non-concat lines.
 * If you wonder why this is a separate and surprisingly small function,
 * it is because already screwing up twice, where the local-case
 * diverted from the remote-case, so hence this helper :D.
 */
static inline int multiline_calc_add_bytes(int text_len, int line_count, int is_concat)
{
	return text_len + (((line_count > 0) && !is_concat) ? 1 : 0);
}

/** Concatenate all batch lines into a single string (for spamfilter).
 * Concat lines are joined without separator, normal lines with \n.
 * @returns A dynamically allocated string. Caller must free with safe_free().
 */
static char *multiline_concat_text(MultilineBatch *batch)
{
	MLine *l;
	int bufsize = 1; /* 1 for \0 */
	char *buf;
	int pos = 0;

	/* Calculate buffer size from actual line data, since text
	 * may have been transformed at delivery time (+S, +G, etc.).
	 */
	for (l = batch->lines; l; l = l->next)
	{
		if (l != batch->lines && !l->concat)
			bufsize++; /* \n separator */
		if (l->text)
			bufsize += strlen(l->text);
	}
	buf = safe_alloc(bufsize);

	/* Now build the big string*/
	for (l = batch->lines; l; l = l->next)
	{
		if (l != batch->lines && !l->concat)
		{
			if (pos + 1 >= bufsize)
				break;
			buf[pos++] = '\n';
		}
		if (l->text)
		{
			int len = strlen(l->text);
			if (pos + len >= bufsize)
				break;
			strlcpy(buf + pos, l->text, bufsize - pos);
			pos += len;
		}
	}
	buf[pos] = '\0';

	return buf;
}

/** Build fallback text: merge concat lines into their predecessors.
 * Returns a list of "logical lines" for fallback delivery.
 * Caller must free the returned list with multiline_free_lines().
 */
static MLine *multiline_build_fallback_lines(MultilineBatch *batch)
{
	MLine *out = NULL, *out_tail = NULL;
	MLine *l;

	for (l = batch->lines; l; l = l->next)
	{
		if (l->concat && out_tail)
		{
			/* Append to current logical line without separator */
			const char *append = l->text ? l->text : "";
			int newlen = strlen(out_tail->text) + strlen(append) + 1;
			char *newtext = safe_alloc(newlen);
			strlcpy(newtext, out_tail->text, newlen);
			strlcat(newtext, append, newlen);
			safe_free(out_tail->text);
			out_tail->text = newtext;
		} else {
			/* Start a new logical line */
			MLine *newline = safe_alloc(sizeof(MLine));
			safe_strdup(newline->text, l->text ? l->text : "");
			newline->concat = 0;
			newline->next = NULL;
			if (out_tail)
				out_tail->next = newline;
			else
				out = newline;
			out_tail = newline;
		}
	}
	return out;
}

/** Get cached fallback lines, building them on first call */
static MLine *multiline_get_fallback_lines(MultilineBatch *batch)
{
	if (!batch->fallback_lines)
		batch->fallback_lines = multiline_build_fallback_lines(batch);
	return batch->fallback_lines;
}

/** Check if a batch has at least one non-blank line */
static int multiline_batch_has_content(MultilineBatch *batch)
{
	MLine *l;

	for (l = batch->lines; l; l = l->next)
		if (l->text && l->text[0])
			return 1;
	return 0;
}

/* ===================== BATCH COMMAND OVERRIDE ===================== */

CMD_OVERRIDE_FUNC(multiline_override_batch)
{
	const char *ref;
	MultilineBatch *batch;

	/* Handle S2S multiline batches.
	 * Both channel and user cases use the same wire format:
	 *   BATCH <target> +ref draft/multiline [extra]
	 *   BATCH <target> -ref
	 * where <target> is a channel name or user UID.
	 * We distinguish channel vs user by checking if parv[1] contains '#'.
	 */
	if (IsServer(client) || !MyConnect(client))
	{
		if (parc >= 3 && (parv[2][0] == '+' || parv[2][0] == '-'))
		{
			int is_channel = (strchr(parv[1], '#') != NULL);

			if (parv[2][0] == '+' && parc >= 4 && !strcmp(parv[3], "draft/multiline"))
			{
				multiline_handle_s2s_batch_open(client, recv_mtags, parc, parv, is_channel);
				return;
			}
			if (parv[2][0] == '-')
			{
				S2SMultilineBatch *s2s = multiline_find_s2s_batch(client, parv[2] + 1);
				if (s2s)
				{
					multiline_handle_s2s_batch_close(client, parv[2] + 1);
					return;
				}
			}
		}
		/* Not a multiline batch - fall through to batch.c */
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	/* Not a local user - fall through */
	if (!MyUser(client))
	{
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	if (parc < 2 || BadPtr(parv[1]))
	{
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	ref = parv[1];

	/* Opening batch: BATCH +ref draft/multiline <target> */
	if (ref[0] == '+')
	{
		const char *batch_type;
		const char *target;

		ref++; /* skip '+' */

		if (!valid_batch_reference_tag(ref))
		{
			sendto_one(client, NULL, ":%s FAIL BATCH INVALID_REFTAG %s :Invalid batch reference tag", me.name, ref);
			return;
		}

		if (parc < 4 || BadPtr(parv[2]) || BadPtr(parv[3]))
		{
			sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :Insufficient parameters", me.name);
			return;
		}

		batch_type = parv[2];
		target = parv[3];

		/* Only handle draft/multiline batch type */
		if (strcmp(batch_type, "draft/multiline"))
		{
			/* Not a multiline batch - fall through */
			CALL_NEXT_COMMAND_OVERRIDE();
			return;
		}

		/* Check capabilities */
		if (!HasMultiline(client))
		{
			sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :You must negotiate the draft/multiline and batch capabilities", me.name);
			return;
		}

		/* Check for existing open batch */
		if (moddata_local_client(client, multiline_md).ptr)
		{
			sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :You already have an open multiline batch", me.name);
			return;
		}

		/* Allocate and initialize batch state */
		batch = safe_alloc(sizeof(MultilineBatch));
		strlcpy(batch->batch_id, ref, sizeof(batch->batch_id));

		/* Parse STATUSMSG prefix from target, modeled on message.c cmd_message() */
		{
			const char *p2 = strchr(target, '#');
			if (p2 && (p2 - target > 0))
			{
				char prefix_tmp[32];
				char prefix;
				Channel *channel;

				strlncpy(prefix_tmp, target, sizeof(prefix_tmp), p2 - target);
				prefix = lowest_ranking_prefix(prefix_tmp);
				if (!prefix)
				{
					sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :Invalid prefix", me.name);
					safe_free(batch);
					return;
				}
				channel = find_channel(p2);
				if (!channel)
				{
					sendnumeric(client, ERR_NOSUCHNICK, p2);
					safe_free(batch);
					return;
				}
				/* STATUSMSG access is checked at delivery time
				 * (in multiline_deliver_channel) to avoid TOCTOU.
				 */

				/* Store normalized prefixed target (e.g., "@#channel") */
				{
					char pfixchan[CHANNELLEN + 4];
					snprintf(pfixchan, sizeof(pfixchan), "%c%s", prefix, channel->name);
					safe_strdup(batch->target, pfixchan);
				}

				/* Store member mode */
				batch->member_modes[0] = prefix_to_mode(prefix);
				batch->member_modes[1] = '\0';
			} else {
				safe_strdup(batch->target, target);
				batch->member_modes[0] = '\0';
			}
		}

		batch->sendtype_set = 0;
		batch->line_count = 0;
		batch->received_bytes = 0;
		batch->start_time = TStime();
		batch->lines = NULL;
		batch->lines_tail = NULL;

		/* Save client tags from the BATCH command (spec: other client tags go here) */
		batch->client_mtags = duplicate_mtags_excluding(recv_mtags, "batch");

		/* For echo-message + labeled-response interaction: save the label
		 * and clear the LR context so lr_post_command won't send a
		 * premature ACK. The label will be placed on the echo batch's
		 * BATCH open line at delivery time instead.
		 * Spec: "Servers MUST only include the label tag on the opening
		 * BATCH command when replying to a client using echo-message."
		 */
		batch->label[0] = '\0';
		if (HasCapability(client, "echo-message") &&
		    HasCapability(client, "labeled-response"))
		{
			MessageTag *mt;
			for (mt = recv_mtags; mt; mt = mt->next)
			{
				if (!strcmp(mt->name, "label") && mt->value)
				{
					strlcpy(batch->label, mt->value, sizeof(batch->label));
					labeled_response_set_context(NULL);
					break;
				}
			}
		}

		moddata_local_client(client, multiline_md).ptr = batch;
		return; /* Consumed, don't fall through */
	}

	/* Closing batch: BATCH -ref */
	if (ref[0] == '-')
	{
		ref++; /* skip '-' */

		batch = moddata_local_client(client, multiline_md).ptr;
		if (!batch || strcmp(batch->batch_id, ref))
		{
			sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :No matching open batch", me.name);
			return;
		}

		/* If the batch was marked as failed during line processing,
		 * send the deferred error now and clean up.
		 */
		if (batch->failed)
		{
			sendto_one(client, NULL, ":%s %s", me.name, batch->fail_message);
			multiline_abort_batch(client, batch);
			return;
		}

		/* Validate: batch must have at least one line */
		if (batch->line_count == 0)
		{
			sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :Empty batch", me.name);
			multiline_abort_batch(client, batch);
			return;
		}

		/* Validate: batch must not be entirely blank lines */
		if (!multiline_batch_has_content(batch))
		{
			sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :Batch consists entirely of blank lines", me.name);
			multiline_abort_batch(client, batch);
			return;
		}

		/* Deliver the batch */
		multiline_deliver(client, batch);

		/* Apply batch fake lag */
		add_fake_lag(client, calculate_multiline_fakelag(client, batch));

		/* Clean up */
		multiline_free_batch(batch);
		moddata_local_client(client, multiline_md).ptr = NULL;
		return;
	}

	/* Not +/- prefixed, fall through */
	CALL_NEXT_COMMAND_OVERRIDE();
}

/* ===================== PRIVMSG/NOTICE OVERRIDE ===================== */

CMD_OVERRIDE_FUNC(multiline_override_msg)
{
	MultilineBatch *batch;
	MessageTag *m;
	FloodSettings *f;
	const char *batch_ref = NULL;
	Channel *channel;
	const char *target;
	const char *text;
	int is_concat = 0;
	int text_len;
	int add_bytes;
	SendType sendtype;

	/* Only intercept from local users */
	if (!MyUser(client))
	{
		/* Check for S2S multiline batch line */
		if (IsServer(client) || !MyConnect(client))
		{
			m = find_mtag(recv_mtags, "batch");
			if (m && m->value)
			{
				S2SMultilineBatch *s2s = multiline_find_s2s_batch(client, m->value);
				if (s2s)
				{
					/* Determine sendtype from command name */
					sendtype = !strcmp(ovr->command->cmd, "NOTICE") ? SEND_TYPE_NOTICE : SEND_TYPE_PRIVMSG;
					multiline_handle_s2s_msg(client, recv_mtags, parc, parv, sendtype);
					return;
				}
			}
		}
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	/* Check for @batch= tag */
	m = find_mtag(recv_mtags, "batch");
	if (!m || !m->value)
	{
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}
	batch_ref = m->value;

	/* Look up active batch */
	batch = moddata_local_client(client, multiline_md).ptr;
	if (!batch || strcmp(batch->batch_id, batch_ref))
	{
		/* No matching batch - process as normal message */
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	/* If the batch was already marked as failed, silently consume
	 * remaining lines. The error will be sent at BATCH close.
	 */
	if (batch->failed)
	{
		/* Undo fakelag for discarded lines, but only up to
		 * max-lines total to prevent flood evasion.
		 */
		FloodSettings *f = get_floodsettings_for_user(client, FLD_MULTILINE);
		int max_lines = f->limit[FLD_MULTILINE];
		batch->failed++;
		if (batch->line_count + batch->failed <= max_lines)
			subtract_fake_lag(client, clictx->fake_lag_added_msec);
		return;
	}

	/* We are inside an active multiline batch - buffer this line */
	if (parc < 2 || BadPtr(parv[1]))
	{
		sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_INVALID :Insufficient parameters", me.name);
		multiline_abort_batch(client, batch);
		return;
	}

	target = parv[1];
	text = (parc >= 3 && parv[2]) ? parv[2] : "";

	/* Determine SendType from command name */
	sendtype = !strcmp(ovr->command->cmd, "NOTICE") ? SEND_TYPE_NOTICE : SEND_TYPE_PRIVMSG;

	/* First line sets sendtype, subsequent must match (no mixing PRIVMSG/NOTICE) */
	if (!batch->sendtype_set)
	{
		batch->sendtype = sendtype;
		batch->sendtype_set = 1;
	} else if (batch->sendtype != sendtype) {
		char buf[512];
		snprintf(buf, sizeof(buf), "FAIL BATCH MULTILINE_INVALID :Cannot mix PRIVMSG and NOTICE in a multiline batch");
		multiline_fail_batch(client, batch, buf);
		subtract_fake_lag(client, clictx->fake_lag_added_msec);
		return;
	}

	/* Validate target matches batch target */
	if (strcasecmp(target, batch->target))
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "FAIL BATCH MULTILINE_INVALID_TARGET %s %s :Target mismatch",
		         batch->target, target);
		multiline_fail_batch(client, batch, buf);
		subtract_fake_lag(client, clictx->fake_lag_added_msec);
		return;
	}

	/* Check for draft/multiline-concat tag */
	if (find_mtag(recv_mtags, "draft/multiline-concat"))
		is_concat = 1;

	/* Validate: blank line with concat is invalid */
	if (is_concat && (!text || !text[0]))
	{
		multiline_fail_batch(client, batch, "FAIL BATCH MULTILINE_INVALID :Blank line with concat tag");
		subtract_fake_lag(client, clictx->fake_lag_added_msec);
		return;
	}

	/* Reject CTCP in multiline batches (including ACTION).
	 * A batch mixing CTCP and non-CTCP lines has no coherent
	 * semantics, and even a pure-CTCP batch would behave
	 * differently for multiline-capable vs fallback recipients.
	 * Fallback delivery sends each line as an individual PRIVMSG,
	 * turning one batch into N separate CTCP requests — a flood
	 * amplification vector triggering auto-replies from each client.
	 */
	if (text && *text == '\001')
	{
		multiline_fail_batch(client, batch, "FAIL BATCH MULTILINE_INVALID :CTCP not permitted in multiline batches");
		subtract_fake_lag(client, clictx->fake_lag_added_msec);
		return;
	}

	/* Check max-lines and max-bytes (per-group limits) */
	if ((f = get_floodsettings_for_user(client, FLD_MULTILINE)))
	{
		int max_lines = f->limit[FLD_MULTILINE];
		int max_bytes = (int)f->period[FLD_MULTILINE];

		/* If target is a channel, also respect +f/+F 'm' and 't' limits */
		channel = multiline_find_channel(batch->target);
		if (channel)
			max_lines = MIN(max_lines, get_floodprot_channel_max_lines(channel));

		if (batch->line_count >= max_lines)
		{
			char buf[512];
			snprintf(buf, sizeof(buf), "FAIL BATCH MULTILINE_MAX_LINES %d :Too many lines in batch", max_lines);
			multiline_fail_batch(client, batch, buf);
			subtract_fake_lag(client, clictx->fake_lag_added_msec);
			return;
		}

		/* Check max-bytes (text length + 1 for newline separator, except for concat and first line) */
		text_len = text ? strlen(text) : 0;
		add_bytes = multiline_calc_add_bytes(text_len, batch->line_count, is_concat);

		if (batch->received_bytes + add_bytes > max_bytes)
		{
			char buf[512];
			snprintf(buf, sizeof(buf), "FAIL BATCH MULTILINE_MAX_BYTES %d :Too many bytes in batch", max_bytes);
			multiline_fail_batch(client, batch, buf);
			subtract_fake_lag(client, clictx->fake_lag_added_msec);
			return;
		}
		batch->received_bytes += add_bytes;
	}

	/* Permission checks (can_send_to_channel / can_send_to_user) and text
	 * transformations (+S, +G, etc.) are intentionally NOT done here during
	 * buffering. They are deferred to delivery time (multiline_deliver_channel
	 * / multiline_deliver_user) to avoid TOCTOU issues: channel modes, bans,
	 * or user modes could change between buffering and delivery.
	 * Raw (untransformed) text is buffered here.
	 */

	/* Buffer the line */
	multiline_append_line(&batch->lines, &batch->lines_tail, &batch->line_count, text, is_concat);

	/* Undo the fake lag that parse_addlag() added for this buffered line,
	 * the batch formula at BATCH close will apply the correct lag instead.
	 */
	subtract_fake_lag(client, clictx->fake_lag_added_msec);

	/* Line buffered - do NOT call CALL_NEXT_COMMAND_OVERRIDE() */
}

/* ===================== BATCH DELIVERY ===================== */

/** Deliver a completed multiline batch */
static void multiline_deliver(Client *client, MultilineBatch *batch)
{
	Channel *channel;
	Client *target;
	const char *text, *errmsg;
	char *concat_text;

	/* Force labeled-response (like message.c does) */
	labeled_response_force = 1;

	/* Resolve target (channel, or NULL for user targets / vanished channels) */
	channel = multiline_find_channel(batch->target);
	if (channel)
	{
		multiline_deliver_channel(client, batch, channel);
	} else if (strchr(batch->target, '#')) {
		/* Target had a '#' but channel not found (parted/destroyed during batch) */
		sendnumeric(client, ERR_NOSUCHNICK, batch->target);
		return;
	} else {
		target = hash_find_nickatserver(batch->target, NULL);
		if (!target)
		{
			if (SERVICES_NAME)
			{
				char *server = strchr(batch->target, '@');
				if (server && strncasecmp(server + 1, SERVICES_NAME, strlen(SERVICES_NAME)) == 0)
				{
					sendnumeric(client, ERR_SERVICESDOWN, batch->target);
					return;
				}
			}
			sendnumeric(client, ERR_NOSUCHNICK, batch->target);
			return;
		}
		multiline_deliver_user(client, batch, target);
	}
}

/** Deliver multiline batch to a channel */
static void multiline_deliver_channel(Client *client, MultilineBatch *batch, Channel *channel)
{
	char *concat_text;
	const char *cmd = sendtype_to_cmd(batch->sendtype);
	MessageTag *mtags = NULL;
	int sendflags = SEND_ALL;
	MLine *line;
	char expanded_modes[64];
	const char *filter_modes = NULL;

	/* Run permission checks and text transformations at delivery time
	 * (not during buffering) to avoid TOCTOU: channel modes, bans,
	 * or membership could change between buffering and batch close.
	 */
	if (MyUser(client) && !IsULine(client))
	{
		for (line = batch->lines; line; line = line->next)
		{
			TextAnalysis ta;
			ClientContext delivery_clictx;
			const char *check_text;
			const char *check_errmsg = NULL;

			if (!line->text || !*line->text)
				continue; /* blank lines are valid paragraph breaks */

			memset(&ta, 0, sizeof(ta));
			memset(&delivery_clictx, 0, sizeof(delivery_clictx));
			delivery_clictx.textanalysis = &ta;
			RunHook(HOOKTYPE_ANALYZE_TEXT, client, line->text, &ta);

			check_text = line->text;
			if (!can_send_to_channel(client, channel, &check_text, &check_errmsg, batch->sendtype, &delivery_clictx))
			{
				if (IsDead(client))
					return;
				return; /* Batch rejected */
			}
			/* Apply transformed text (e.g. +S stripped colors, +G censored) */
			if (check_text != line->text)
			{
				safe_free(line->text);
				safe_strdup(line->text, check_text);
			}
		}

		/* Re-check STATUSMSG access (user may have lost op/voice during batch) */
		if (batch->member_modes[0])
		{
			if (!op_can_override("channel:override:message:prefix", client, channel, NULL))
			{
				Membership *lp = find_membership_link(client->user->channel, channel);
				if (!lp || !check_channel_access_membership(lp, "vhoaq"))
				{
					sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
					return;
				}
			}
		}
	}

	/* Expand member_modes to include equal-or-higher ranks for STATUSMSG filtering */
	if (batch->member_modes[0])
	{
		channel_member_modes_generate_equal_or_greater(batch->member_modes, expanded_modes, sizeof(expanded_modes));
		filter_modes = expanded_modes;
	}

	/* Spamfilter on concatenated text */
	if (MyUser(client))
	{
		int spamtype = (batch->sendtype == SEND_TYPE_NOTICE ? SPAMF_CHANNOTICE : SPAMF_CHANMSG);
		concat_text = multiline_concat_text(batch);
		if (match_spamfilter(client, concat_text, spamtype, cmd, channel->name, 0, NULL, NULL))
		{
			safe_free(concat_text);
			return;
		}
		safe_free(concat_text);
	}

	/* Check channel +f 'p' (paste flood) limit */
	if (MyUser(client) && floodprot_check_multiline_batch(channel, client, batch->line_count))
	{
		sendto_one(client, NULL, ":%s FAIL BATCH MULTILINE_PASTE_LIMIT :Too many paste events in channel (+f)", me.name);
		return;
	}

	/* Generate outgoing message tags */
	new_message(client, batch->client_mtags, &mtags);

	/* Compute sendflags */
	if (batch->lines && batch->lines->text && !strchr(CHANCMDPFX, batch->lines->text[0]))
		sendflags |= SKIP_DEAF;

	/* Hook: PRE_CHANMSG (per-line, matching single-message behavior) */
	for (line = batch->lines; line; line = line->next)
		RunHook(HOOKTYPE_PRE_CHANMSG, client, channel, &mtags, line->text, batch->sendtype);

	/* Deliver to local channel members */
	multiline_deliver_to_local_members(channel, client, batch, mtags,
	                                  batch->target, cmd, filter_modes, client, sendflags);

	/* Echo-message for sender */
	multiline_echo_to_sender(client, batch, mtags, batch->target, cmd);

	/* S2S relay */
	multiline_send_s2s_channel(client, batch, channel, mtags, cmd, client->direction, batch->target);

	multiline_run_chanmsg_hooks(client, channel, sendflags,
	                           batch->member_modes[0] ? batch->member_modes : NULL,
	                           batch->target, mtags, batch->lines, batch->sendtype);

	free_message_tags(mtags);
}

/** Deliver multiline batch to a user */
static void multiline_deliver_user(Client *client, MultilineBatch *batch, Client *target)
{
	char *concat_text;
	const char *cmd = sendtype_to_cmd(batch->sendtype);
	MessageTag *mtags = NULL;

	/* Run permission checks and text transformations at delivery time
	 * (not during buffering) to avoid TOCTOU: user modes or target
	 * identity could change between buffering and batch close.
	 */
	if (MyUser(client) && !IsULine(client))
	{
		MLine *line;
		for (line = batch->lines; line; line = line->next)
		{
			TextAnalysis ta;
			ClientContext delivery_clictx;
			const char *check_text;
			const char *check_errmsg = NULL;

			if (!line->text || !*line->text)
				continue;

			memset(&ta, 0, sizeof(ta));
			memset(&delivery_clictx, 0, sizeof(delivery_clictx));
			delivery_clictx.textanalysis = &ta;
			RunHook(HOOKTYPE_ANALYZE_TEXT, client, line->text, &ta);

			check_text = line->text;
			if (!can_send_to_user(client, target, &check_text, &check_errmsg, batch->sendtype, &delivery_clictx, 0))
			{
				if (IsDead(client))
					return;
				return; /* Batch rejected */
			}
			if (check_text != line->text)
			{
				safe_free(line->text);
				safe_strdup(line->text, check_text);
			}
		}
	}

	/* Spamfilter on concatenated text (not per-line, to catch
	 * patterns spanning multiple lines and match channel behavior).
	 */
	if (MyUser(client) && (batch->sendtype != SEND_TYPE_TAGMSG))
	{
		int spamtype = (batch->sendtype == SEND_TYPE_NOTICE ? SPAMF_USERNOTICE : SPAMF_USERMSG);
		concat_text = multiline_concat_text(batch);
		if (match_spamfilter(client, concat_text, spamtype, cmd, target->name, 0, NULL, NULL))
		{
			safe_free(concat_text);
			return;
		}
		safe_free(concat_text);
	}

	/* Inform sender of away status */
	if (batch->sendtype == SEND_TYPE_PRIVMSG && MyConnect(client) && target->user && target->user->away)
		sendnumeric(client, RPL_AWAY, target->name, target->user->away);

	/* Generate outgoing message tags */
	new_message(client, batch->client_mtags, &mtags);

	labeled_response_inhibit = 1;

	if (MyUser(target))
	{
		/* Target is local */
		if (HasMultiline(target))
		{
			multiline_send_batch_to_client(target, client, batch, mtags, target->name, cmd);
		} else {
			multiline_send_fallback_to_client(target, client, batch, mtags, target->name, cmd);
		}
	} else {
		/* Target is remote - relay as S2S batch */
		multiline_send_s2s_user(client, batch, target, mtags, cmd);
	}

	labeled_response_inhibit = 0;

	/* Echo-message for sender */
	multiline_echo_to_sender(client, batch, mtags, target->name, cmd);

	multiline_run_usermsg_hooks(client, target, mtags, batch->lines, batch->sendtype);

	free_message_tags(mtags);
}

/** Echo-message: send the multiline batch back to the sender, with label if applicable */
static void multiline_echo_to_sender(Client *client, MultilineBatch *batch,
                                     MessageTag *mtags, const char *targetstr, const char *cmd)
{
	if (!MyUser(client) || !HasCapability(client, "echo-message"))
		return;

	MessageTag *echo_mtags = mtags;
	MessageTag *label_mtag = NULL;

	/* labeled-response: include label on the echo's BATCH open */
	if (batch->label[0])
	{
		label_mtag = safe_alloc(sizeof(MessageTag));
		safe_strdup(label_mtag->name, "label");
		safe_strdup(label_mtag->value, batch->label);
		AddListItem(label_mtag, mtags);
		echo_mtags = mtags;
	}

	if (HasMultiline(client))
	{
		multiline_send_batch_to_client(client, client, batch, echo_mtags, targetstr, cmd);
	} else {
		multiline_send_fallback_to_client(client, client, batch, echo_mtags, targetstr, cmd);
	}

	if (label_mtag)
		DelListItem(label_mtag, mtags);
	safe_free_message_tags(label_mtag);
}

/* ===================== SENDING HELPERS ===================== */

/** Send a multiline batch to a single local client that supports draft/multiline.
 * Used for user-to-user delivery, echo-message, and S2S incoming.
 * For channel broadcast, use multiline_deliver_to_local_members() instead.
 */
static void multiline_send_batch_to_client(Client *to, Client *from, MultilineBatch *batch,
                                           MessageTag *base_mtags, const char *targetstr, const char *cmd)
{
	char server_batch_id[BATCHLEN+1];
	MLine *l;
	MessageTag *m;

	generate_batch_id(server_batch_id);

	/* Send BATCH open with base_mtags (msgid, time, etc.) */
	sendto_prefix_one(to, from, base_mtags,
	                  ":%s BATCH +%s draft/multiline %s",
	                  from->name, server_batch_id, targetstr);

	/* Send each line */
	for (l = batch->lines; l; l = l->next)
	{
		/* Build line_mtags: base_mtags minus msgid, plus @batch=
		 * and optionally ;draft/multiline-concat
		 */
		MessageTag *line_mtags = duplicate_mtags_for_subsequent_lines(base_mtags);

		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "batch");
		safe_strdup(m->value, server_batch_id);
		AddListItem(m, line_mtags);

		if (l->concat)
		{
			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, "draft/multiline-concat");
			AddListItem(m, line_mtags);
		}

		sendto_prefix_one(to, from, line_mtags,
		                  ":%s %s %s :%s",
		                  from->name, cmd, targetstr, l->text ? l->text : "");

		free_message_tags(line_mtags);
	}

	/* Send BATCH close */
	sendto_prefix_one(to, from, NULL, ":%s BATCH -%s", from->name, server_batch_id);
}

/** Send multiline as individual fallback lines to a single local client.
 * Used for user-to-user delivery, echo-message, and S2S incoming.
 * For channel broadcast, use multiline_deliver_to_local_members() instead.
 */
static void multiline_send_fallback_to_client(Client *to, Client *from, MultilineBatch *batch,
                                              MessageTag *base_mtags, const char *targetstr, const char *cmd)
{
	MLine *fallback_lines, *l;
	MessageTag *rest_mtags;
	int first = 1;

	fallback_lines = multiline_get_fallback_lines(batch);

	/* Build rest_mtags: base_mtags minus msgid.
	 * Spec: msgid MUST only be on the first line, but other tags
	 * (time, account, etc.) MAY be on subsequent lines.
	 */
	rest_mtags = duplicate_mtags_for_subsequent_lines(base_mtags);

	for (l = fallback_lines; l; l = l->next)
	{
		/* Spec: "Servers MUST NOT send blank lines to clients
		 * that have not negotiated the multiline capability."
		 */
		if (!l->text || !l->text[0])
			continue;

		if (first)
		{
			/* First non-blank line gets full mtags (msgid, time, account, etc.) */
			sendto_prefix_one(to, from, base_mtags,
			                  ":%s %s %s :%s",
			                  from->name, cmd, targetstr, l->text);
			first = 0;
		} else {
			/* Subsequent lines get all tags except msgid */
			sendto_prefix_one(to, from, rest_mtags,
			                  ":%s %s %s :%s",
			                  from->name, cmd, targetstr, l->text);
		}
	}

	free_message_tags(rest_mtags);
}

/** Deliver multiline batch to local channel members.
 * Uses two passes (multiline-capable, then fallback) with pre-built
 * message tags. This uses LineCache, which makes this function
 * a bit complex but saves A LOT of CPU so is necessary.
 * @param skip         Client to skip (sender for local delivery, NULL for S2S)
 * @param sendflags    Send flags (SKIP_DEAF etc.), 0 if not applicable
 */
static void multiline_deliver_to_local_members(Channel *channel, Client *from,
                                               MultilineBatch *batch, MessageTag *base_mtags, const char *targetstr,
                                               const char *cmd, const char *filter_modes, Client *skip, int sendflags)
{
	LocalMember *lm;
	MLine *l;
	MLine *fallback_lines; // merged lines for non-multiline clients
	MessageTag *m; // temporary for building tag lists
	MessageTag *rest_mtags; // base_mtags minus first-only tags (msgid, +draft/reply)
	MessageTag *mtags_line; // rest_mtags + batch=<id>, for multiline content lines
	MessageTag *mtags_line_concat; // mtags_line + draft/multiline-concat
	LineCache *cache_batch_open; // cache for "BATCH +id draft/multiline target"
	LineCache *cache_batch_close; // cache for "BATCH -id"
	LineCache **cache_lines = NULL; // one cache per multiline content line
	LineCache **cache_fb = NULL; // one cache per non-blank fallback line
	char server_batch_id[BATCHLEN+1]; // shared across all multiline recipients
	int i;
	int fallback_count = 0; // number of non-blank fallback lines
	int first; // tracks first non-blank line in fallback pass

	/* === Phase 1: Pre-build shared resources (once, not per-client) === */

	/* Single batch ID shared across all multiline-capable recipients.
	 * Safe: each client connection tracks batches independently.
	 */
	generate_batch_id(server_batch_id);

	/* rest_mtags: base_mtags minus first-only tags (msgid, +draft/reply).
	 * Used for subsequent lines in both multiline and fallback paths.
	 */
	rest_mtags = duplicate_mtags_for_subsequent_lines(base_mtags);

	/* mtags_line: rest_mtags + batch=<id>, for non-concat multiline lines */
	mtags_line = duplicate_mtags(rest_mtags);
	m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, "batch");
	safe_strdup(m->value, server_batch_id);
	AddListItem(m, mtags_line);

	/* mtags_line_concat: mtags_line + draft/multiline-concat */
	mtags_line_concat = duplicate_mtags(mtags_line);
	m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, "draft/multiline-concat");
	AddListItem(m, mtags_line_concat);

	/* === Phase 2: Allocate LineCaches === */

	/* Multiline path: BATCH open + N content lines + BATCH close */
	cache_batch_open = linecache_init();
	cache_batch_close = linecache_init();
	if (batch->line_count > 0)
		cache_lines = safe_alloc(sizeof(LineCache *) * batch->line_count);
	for (i = 0; i < batch->line_count; i++)
		cache_lines[i] = linecache_init();

	/* Fallback path: one cache per non-blank fallback line */
	fallback_lines = multiline_get_fallback_lines(batch);
	for (l = fallback_lines; l; l = l->next)
		if (l->text && l->text[0])
			fallback_count++;
	if (fallback_count > 0)
		cache_fb = safe_alloc(sizeof(LineCache *) * fallback_count);
	for (i = 0; i < fallback_count; i++)
		cache_fb[i] = linecache_init();

	/* === Phase 3: Multiline pass === */
	for (lm = channel->local_members; lm; lm = lm->next)
	{
		Client *acptr = lm->ptr->client;

		if (acptr == skip)
			continue;
		if (IsDeaf(acptr) && (sendflags & SKIP_DEAF))
			continue;
		if (filter_modes && !check_channel_access_member(lm->ptr, filter_modes))
			continue;
		if (!HasMultiline(acptr))
			continue;

		/* BATCH open */
		sendto_prefix_one_cached(cache_batch_open, 0, acptr, from, base_mtags,
		                         ":%s BATCH +%s draft/multiline %s",
		                         from->name, server_batch_id, targetstr);

		/* Content lines */
		i = 0;
		for (l = batch->lines; l; l = l->next, i++)
		{
			sendto_prefix_one_cached(cache_lines[i], 0, acptr, from,
			                         l->concat ? mtags_line_concat : mtags_line,
			                         ":%s %s %s :%s",
			                         from->name, cmd, targetstr, l->text ? l->text : "");
		}

		/* BATCH close */
		sendto_prefix_one_cached(cache_batch_close, 0, acptr, from, NULL,
		                         ":%s BATCH -%s", from->name, server_batch_id);
	}

	/* === Phase 4: Fallback pass === */
	for (lm = channel->local_members; lm; lm = lm->next)
	{
		Client *acptr = lm->ptr->client;

		if (acptr == skip)
			continue;
		if (IsDeaf(acptr) && (sendflags & SKIP_DEAF))
			continue;
		if (filter_modes && !check_channel_access_member(lm->ptr, filter_modes))
			continue;
		if (HasMultiline(acptr))
			continue;

		first = 1;
		i = 0;
		for (l = fallback_lines; l; l = l->next)
		{
			/* Spec: "Servers MUST NOT send blank lines to clients
			 * that have not negotiated the multiline capability."
			 */
			if (!l->text || !l->text[0])
				continue;

			sendto_prefix_one_cached(cache_fb[i], 0, acptr, from,
			                         first ? base_mtags : rest_mtags,
			                         ":%s %s %s :%s",
			                         from->name, cmd, targetstr, l->text);
			first = 0;
			i++;
		}
	}

	/* === Phase 5: Cleanup === */
	linecache_free(cache_batch_open);
	linecache_free(cache_batch_close);
	for (i = 0; i < batch->line_count; i++)
		linecache_free(cache_lines[i]);
	safe_free(cache_lines);
	for (i = 0; i < fallback_count; i++)
		linecache_free(cache_fb[i]);
	safe_free(cache_fb);
	free_message_tags(rest_mtags);
	free_message_tags(mtags_line);
	free_message_tags(mtags_line_concat);
}

/** Send a multiline batch (BATCH open + lines + BATCH close) to a single server direction */
static void multiline_send_s2s_to_direction(Client *direction, Client *from,
                                            MultilineBatch *batch, MessageTag *base_mtags, const char *cmd,
                                            const char *targetstr, const char *ref)
{
	MLine *l;
	int first;

	sendto_one(direction, NULL,
	           ":%s BATCH %s +%s draft/multiline",
	           from->id, targetstr, ref);

	first = 1;
	for (l = batch->lines; l; l = l->next)
	{
		MessageTag *line_mtags = NULL;
		MessageTag *m;

		if (first)
		{
			/* First line carries all base_mtags (msgid, time, etc.) */
			line_mtags = duplicate_mtags(base_mtags);
			first = 0;
		} else {
			/* Subsequent lines carry base_mtags minus msgid */
			line_mtags = duplicate_mtags_for_subsequent_lines(base_mtags);
		}

		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "batch");
		safe_strdup(m->value, ref);
		AddListItem(m, line_mtags);

		if (l->concat)
		{
			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, "draft/multiline-concat");
			AddListItem(m, line_mtags);
		}

		sendto_prefix_one(direction, from, line_mtags,
		                  ":%s %s %s :%s",
		                  from->id, cmd, targetstr, l->text ? l->text : "");
		free_message_tags(line_mtags);
	}

	sendto_one(direction, NULL,
	           ":%s BATCH %s -%s",
	           from->id, targetstr, ref);
}

/** Send multiline batch to remote servers (channel target) */
static void multiline_send_s2s_channel(Client *from, MultilineBatch *batch, Channel *channel,
                                       MessageTag *base_mtags, const char *cmd, Client *skip_direction, const char *targetstr)
{
	char ref[BATCHLEN+1];
	Member *lp;
	Client *acptr;
	char expanded_modes[64];
	const char *filter_modes = NULL;

	generate_batch_id(ref);

	/* Expand member_modes for STATUSMSG filtering */
	if (batch->member_modes[0])
	{
		channel_member_modes_generate_equal_or_greater(batch->member_modes, expanded_modes, sizeof(expanded_modes));
		filter_modes = expanded_modes;
	}

	/* Track which server directions we've already sent to */
	++current_serial;

	for (lp = channel->members; lp; lp = lp->next)
	{
		acptr = lp->client;

		if (MyUser(acptr))
			continue; /* Already handled locally */

		if (acptr->direction == skip_direction)
			continue;

		/* STATUSMSG filtering: only relay to directions with matching members */
		if (filter_modes && !check_channel_access_member(lp, filter_modes))
			continue;

		if (acptr->direction->local->serial == current_serial)
			continue; /* Already sent to this direction */

		acptr->direction->local->serial = current_serial;

		multiline_send_s2s_to_direction(acptr->direction, from, batch, base_mtags, cmd, targetstr, ref);
	}

	/* Also handle broadcast-channel-messages for +H channels etc. */
	if ((iConf.broadcast_channel_messages == BROADCAST_CHANNEL_MESSAGES_ALWAYS) ||
	    ((iConf.broadcast_channel_messages == BROADCAST_CHANNEL_MESSAGES_AUTO) && has_channel_mode(channel, 'H')))
	{
		list_for_each_entry(acptr, &server_list, special_node)
		{
			if (acptr->direction == skip_direction)
				continue;
			if (acptr->direction->local->serial == current_serial)
				continue;
			acptr->direction->local->serial = current_serial;

			multiline_send_s2s_to_direction(acptr->direction, from, batch, base_mtags, cmd, targetstr, ref);
		}
	}
}

/** Send multiline batch to remote server (user-to-user target) */
static void multiline_send_s2s_user(Client *from, MultilineBatch *batch, Client *target,
                                    MessageTag *base_mtags, const char *cmd)
{
	char ref[BATCHLEN+1];

	generate_batch_id(ref);
	multiline_send_s2s_to_direction(target->direction, from, batch, base_mtags, cmd, target->id, ref);
}

/* ===================== S2S RECEIVING ===================== */

/** Find an active S2S batch by sender and batch_id */
static S2SMultilineBatch *multiline_find_s2s_batch(Client *sender, const char *batch_id)
{
	S2SMultilineBatch *s2s;

	for (s2s = s2s_batches; s2s; s2s = s2s->next)
	{
		if (s2s->sender == sender && !strcmp(s2s->batch_id, batch_id))
			return s2s;
	}
	return NULL;
}

/** Free an S2S batch and remove from the global list */
static void multiline_free_s2s_batch(S2SMultilineBatch *s2s)
{
	if (!s2s)
		return;
	DelListItem(s2s, s2s_batches);
	safe_free(s2s->target);
	free_message_tags(s2s->first_mtags);
	multiline_free_lines(s2s->lines);
	safe_free(s2s);
}

/** Free all S2S batches (called when module is permanently removed) */
static void multiline_free_s2s_batches_all(ModData *m)
{
	S2SMultilineBatch *s2s, *s2s_next;

	if (!m->ptr)
		return;

	s2s_batches = m->ptr;
	for (s2s = s2s_batches; s2s; s2s = s2s_next)
	{
		s2s_next = s2s->next;
		multiline_free_s2s_batch(s2s);
	}
	s2s_batches = NULL;
	m->ptr = NULL;
}

/** Handle an incoming S2S multiline batch open */
static void multiline_handle_s2s_batch_open(Client *client, MessageTag *recv_mtags,
                                            int parc, const char *parv[], int is_channel)
{
	S2SMultilineBatch *s2s;
	const char *ref;
	const char *target;

	/* BATCH <target> +ref draft/multiline */
	target = parv[1];
	ref = parv[2] + 1; /* skip '+' */

	/* Check for duplicate */
	if (multiline_find_s2s_batch(client, ref))
		return; /* Duplicate, ignore */

	s2s = safe_alloc(sizeof(S2SMultilineBatch));
	strlcpy(s2s->batch_id, ref, sizeof(s2s->batch_id));
	safe_strdup(s2s->target, target);
	s2s->is_channel = is_channel;

	/* Parse STATUSMSG prefix from target (e.g., "@#channel") */
	s2s->member_modes[0] = '\0';
	if (is_channel)
	{
		const char *p2 = strchr(target, '#');
		if (p2 && (p2 - target > 0))
		{
			char prefix_tmp[32];
			char prefix;
			strlncpy(prefix_tmp, target, sizeof(prefix_tmp), p2 - target);
			prefix = lowest_ranking_prefix(prefix_tmp);
			if (prefix)
			{
				s2s->member_modes[0] = prefix_to_mode(prefix);
				s2s->member_modes[1] = '\0';
			}
		}
	}

	s2s->sender = client;
	s2s->direction = client->direction;
	s2s->sendtype_set = 0;
	s2s->line_count = 0;
	s2s->received_bytes = 0;
	s2s->start_time = TStime();
	s2s->lines = NULL;
	s2s->lines_tail = NULL;
	s2s->first_mtags = NULL;

	/* Save message tags from the BATCH open (for relay) */
	s2s->first_mtags = duplicate_mtags_excluding(recv_mtags, "batch");

	AddListItem(s2s, s2s_batches);
}

/** Handle a line within an S2S multiline batch */
static void multiline_handle_s2s_msg(Client *client, MessageTag *recv_mtags,
                                     int parc, const char *parv[], SendType sendtype)
{
	MessageTag *m;
	S2SMultilineBatch *s2s;
	const char *text;
	int is_concat = 0;

	m = find_mtag(recv_mtags, "batch");
	if (!m || !m->value)
		return;

	s2s = multiline_find_s2s_batch(client, m->value);
	if (!s2s)
		return;

	/* Set or validate sendtype */
	if (!s2s->sendtype_set)
	{
		s2s->sendtype = sendtype;
		s2s->sendtype_set = 1;
	} else if (s2s->sendtype != sendtype) {
		/* Mismatch - discard batch */
		multiline_free_s2s_batch(s2s);
		return;
	}

	text = (parc >= 3 && parv[2]) ? parv[2] : "";

	/* Check for concat tag */
	if (find_mtag(recv_mtags, "draft/multiline-concat"))
		is_concat = 1;

	/* Check limits (use hardcoded max, the originating server already enforces per-user limits) */
	{
		int add_bytes = multiline_calc_add_bytes(strlen(text), s2s->line_count, is_concat);

		if ((s2s->line_count >= MULTILINE_MAX_CONFIGURABLE_LINES) ||
		    (s2s->received_bytes + add_bytes > MULTILINE_MAX_CONFIGURABLE_BYTES))
		{
			/* Over limit - discard batch */
			multiline_free_s2s_batch(s2s);
			return;
		}
		s2s->received_bytes += add_bytes;
	}

	/* Save first line's mtags for relay (msgid, time, etc.) */
	if (s2s->line_count == 0 && !s2s->first_mtags)
	{
		MessageTag *mtag_iter;
		for (mtag_iter = recv_mtags; mtag_iter; mtag_iter = mtag_iter->next)
		{
			if (!strcmp(mtag_iter->name, "batch"))
				continue;
			if (!strcmp(mtag_iter->name, "draft/multiline-concat"))
				continue;
			MessageTag *dup = duplicate_mtag(mtag_iter);
			AddListItem(dup, s2s->first_mtags);
		}
	}

	/* Buffer the line */
	multiline_append_line(&s2s->lines, &s2s->lines_tail, &s2s->line_count, text, is_concat);
}

/** Handle an S2S multiline batch close */
static void multiline_handle_s2s_batch_close(Client *client, const char *batch_id)
{
	S2SMultilineBatch *s2s = multiline_find_s2s_batch(client, batch_id);
	if (!s2s)
		return;

	if (s2s->line_count == 0)
	{
		multiline_free_s2s_batch(s2s);
		return;
	}

	multiline_deliver_s2s_batch(s2s);
	multiline_free_s2s_batch(s2s);
}

/** Deliver a completed S2S multiline batch to local members and relay further */
static void multiline_deliver_s2s_batch(S2SMultilineBatch *s2s)
{
	const char *cmd = sendtype_to_cmd(s2s->sendtype);
	MessageTag *mtags = NULL;
	MultilineBatch *batch = safe_alloc(sizeof(MultilineBatch));

	/* Build a temporary MultilineBatch struct for the sending helpers */
	strlcpy(batch->batch_id, s2s->batch_id, sizeof(batch->batch_id));
	batch->target = s2s->target;
	batch->sendtype = s2s->sendtype;
	strlcpy(batch->member_modes, s2s->member_modes, sizeof(batch->member_modes));
	batch->lines = s2s->lines;
	batch->lines_tail = s2s->lines_tail;
	batch->line_count = s2s->line_count;
	batch->received_bytes = s2s->received_bytes;

	if (s2s->is_channel)
	{
		Channel *channel = multiline_find_channel(s2s->target);
		char expanded_modes[64];
		const char *filter_modes = NULL;

		if (channel)
		{
			/* Expand member_modes for STATUSMSG filtering */
			if (s2s->member_modes[0])
			{
				channel_member_modes_generate_equal_or_greater(s2s->member_modes, expanded_modes, sizeof(expanded_modes));
				filter_modes = expanded_modes;
			}

			/* Generate outgoing mtags from the first line's tags */
			new_message(s2s->sender, s2s->first_mtags, &mtags);

			/* Deliver to local members */
			multiline_deliver_to_local_members(channel, s2s->sender, batch, mtags,
			                                  s2s->target, cmd, filter_modes, NULL, 0);

			/* Relay to further server directions (excluding where it came from) */
			multiline_send_s2s_channel(s2s->sender, batch, channel, mtags, cmd, s2s->direction, s2s->target);

			multiline_run_chanmsg_hooks(s2s->sender, channel, SEND_ALL,
			                           s2s->member_modes[0] ? s2s->member_modes : NULL,
			                           s2s->target, mtags, s2s->lines, s2s->sendtype);
		}
	} else {
		/* User-to-user */
		Client *target = find_client(s2s->target, NULL);

		if (target)
		{
			new_message(s2s->sender, s2s->first_mtags, &mtags);

			if (MyUser(target))
			{
				if (HasMultiline(target))
					multiline_send_batch_to_client(target, s2s->sender, batch, mtags, target->name, cmd);
				else
					multiline_send_fallback_to_client(target, s2s->sender, batch, mtags, target->name, cmd);
			} else {
				/* Relay further */
				multiline_send_s2s_user(s2s->sender, batch, target, mtags, cmd);
			}

			multiline_run_usermsg_hooks(s2s->sender, target, mtags, s2s->lines, s2s->sendtype);
		}
	}

	/* Clean up - NULL out borrowed pointers before freeing */
	batch->target = NULL;
	batch->lines = NULL;
	batch->lines_tail = NULL;
	multiline_free_batch(batch);
	free_message_tags(mtags);
}

/* ===================== TIMEOUT AND CLEANUP ===================== */

/** Periodic timer: check for timed-out multiline batches */
EVENT(multiline_timeout_check)
{
	Client *client;
	S2SMultilineBatch *s2s, *s2s_next;

	/* Check local client batches */
	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		MultilineBatch *batch;

		if (!IsUser(client))
			continue;

		batch = moddata_local_client(client, multiline_md).ptr;
		if (batch && (TStime() - batch->start_time > cfg.batch_timeout))
		{
			sendto_one(client, NULL, ":%s FAIL BATCH TIMEOUT %s :Batch timed out", me.name, batch->batch_id);
			multiline_abort_batch(client, batch);
		}
	}

	/* Check S2S batches */
	for (s2s = s2s_batches; s2s; s2s = s2s_next)
	{
		s2s_next = s2s->next;
		if (TStime() - s2s->start_time > cfg.batch_timeout)
		{
			multiline_free_s2s_batch(s2s);
		}
	}
}

/** Hook: local client disconnecting */
int multiline_close_connection(Client *client)
{
	/* ModData free callback handles cleanup, but we might need
	 * additional cleanup here in the future.
	 */
	return 0;
}

/** Hook: remote user quitting */
int multiline_remote_quit(Client *client, MessageTag *mtags, const char *comment)
{
	S2SMultilineBatch *s2s, *s2s_next;

	for (s2s = s2s_batches; s2s; s2s = s2s_next)
	{
		s2s_next = s2s->next;
		if (s2s->sender == client)
			multiline_free_s2s_batch(s2s);
	}
	return 0;
}

/** Hook: server disconnecting */
int multiline_server_quit(Client *client, MessageTag *mtags)
{
	S2SMultilineBatch *s2s, *s2s_next;

	for (s2s = s2s_batches; s2s; s2s = s2s_next)
	{
		s2s_next = s2s->next;
		if (s2s->direction == client)
			multiline_free_s2s_batch(s2s);
	}
	return 0;
}
