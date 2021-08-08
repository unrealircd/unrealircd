/*
 *   IRC - Internet Relay Chat, src/modules/monitor.c
 *   (C) 2021 Bram Matthys and The UnrealIRCd Team
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
	"slog",
	"5.0",
	"S2S logging", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* Forward declarations */
CMD_FUNC(cmd_slog);
void _do_unreal_log_remote_deliver(LogLevel loglevel, char *subsystem, char *event_id, MultiLine *msg, char *json_serialized);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_DO_UNREAL_LOG_REMOTE_DELIVER, _do_unreal_log_remote_deliver);
	return MOD_SUCCESS;
}

MOD_INIT()
{	
	MARK_AS_OFFICIAL_MODULE(modinfo);

	CommandAdd(modinfo->handle, "SLOG", cmd_slog, MAXPARA, CMD_SERVER);

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

CMD_FUNC(cmd_slog)
{
	LogLevel loglevel;
	char *subsystem;
	char *event_id;
	char *msg;
	char *json_incoming = NULL;
	char *json_serialized = NULL;
	MessageTag *m;
	MultiLine *mmsg = NULL;
	json_t *j, *jt;
	json_error_t jerr;
	const char *original_timestamp;

	if ((parc < 4) || BadPtr(parv[4]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SLOG");
		return;
	}

	loglevel = log_level_stringtoval(parv[1]);
	if (loglevel == ULOG_INVALID)
		return;
	subsystem = parv[2];
	if (!valid_subsystem(subsystem))
		return;
	event_id = parv[3];
	if (!valid_event_id(event_id))
		return;
	msg = parv[4];

	m = find_mtag(recv_mtags, "unrealircd.org/json-log");
	if (m)
		json_incoming = m->value;

	if (!json_incoming)
		return;
	// Was previously: unreal_log_raw(loglevel, subsystem, event_id, NULL, msg); // WRONG: this may re-broadcast too, so twice, including back to direction!!!

	/* Validate the JSON */
	j = json_loads(json_incoming, JSON_REJECT_DUPLICATES, &jerr);
	if (!j)
	{
		unreal_log(ULOG_INFO, "log", "REMOTE_LOG_INVALID", client,
		           "Received malformed JSON in server-to-server log message (SLOG) from $client",
		           log_data_string("bad_json_serialized", json_incoming));
		return;
	}

	jt = json_object_get(j, "msg");
	if (!jt)
	{
		unreal_log(ULOG_INFO, "log", "REMOTE_LOG_INVALID", client,
		           "Missing 'msg' in JSON in server-to-server log message (SLOG) from $client",
		           log_data_string("bad_json_serialized", json_incoming));
		json_decref(j);
		return;
	}
	mmsg = line2multiline(msg);

	/* Set "timestamp", and save the original one in "original_timestamp" (if it existed) */
	jt = json_object_get(j, "timestamp");
	if (jt)
	{
		original_timestamp = json_string_value(jt);
		if (original_timestamp)
			json_object_set_new(j, "original_timestamp", json_string(original_timestamp));
	}
	json_object_set_new(j, "timestamp", json_string(timestamp_iso8601_now()));
	json_object_set_new(j, "log_source", json_string(client->name));

	/* Re-serialize the result */
	json_serialized = json_dumps(j, JSON_COMPACT);

	if (json_serialized)
		do_unreal_log_internal_from_remote(loglevel, subsystem, event_id, mmsg, json_serialized);

	/* Broadcast to the other servers */
	sendto_server(client, 0, 0, recv_mtags, ":%s SLOG %s %s %s :%s",
	              client->id,
	              parv[1], parv[2], parv[3], parv[4]);

	/* Free everything */
	safe_free(json_serialized);
	json_decref(j);
	safe_free_multiline(mmsg);
}

void _do_unreal_log_remote_deliver(LogLevel loglevel, char *subsystem, char *event_id, MultiLine *msg, char *json_serialized)
{
	MessageTag *mtags = safe_alloc(sizeof(MessageTag));

	safe_strdup(mtags->name, "unrealircd.org/json-log");
	safe_strdup(mtags->value, json_serialized);

	/* Note that we only send the first line (msg->line),
	 * even for a multi-line event.
	 * If the recipient really wants to see everything then
	 * they can use the JSON data.
	 */
	sendto_server(NULL, 0, 0, mtags, ":%s SLOG %s %s %s :%s",
	              me.id,
	              log_level_valtostring(loglevel), subsystem, event_id, msg->line);

	free_message_tags(mtags);
}
