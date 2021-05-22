/* src/modules/chathistory.c - IRCv3 CHATHISTORY command.
 * (C) Copyright 2021 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"chathistory",
	"1.0",
	"IRCv3 CHATHISTORY command",
	"UnrealIRCd Team",
	"unrealircd-5",
};

/* Forward declarations */
CMD_FUNC(cmd_chathistory);

/* Global variables */
long CAP_CHATHISTORY = 0L;

// TODO: change to 50 and move to config:
#define CHATHISTORY_LIMIT 50

MOD_INIT()
{
	ClientCapabilityInfo c;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "CHATHISTORY", cmd_chathistory, MAXPARA, CMD_USER);

	memset(&c, 0, sizeof(c));
	c.name = "draft/chathistory";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_CHATHISTORY);
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

int chathistory_token(char *str, char *token, char **store)
{
	char *p = strchr(str, '=');
	if (!p)
		return 0;
	*p = '\0'; // frag
	if (!strcmp(str, token))
	{
		*p = '='; // restore
		*store = strdup(p + 1); // can be \0
		return 1;
	}
	*p = '='; // restore
	return 0;
}

static int chathistory_targets_send_line(Client *client, HistoryResult *r, char *batchid)
{
	MessageTag *mtags = NULL;
	MessageTag *m;
	char *ts;

	if (!r->log || !((m = find_mtag(r->log->mtags, "time"))) || !m->value)
		return 0;
	ts = m->value;

	if (!BadPtr(batchid))
	{
		mtags = safe_alloc(sizeof(MessageTag));
		mtags->name = strdup("batch");
		mtags->value = strdup(batchid);
	}

	sendto_one(client, mtags, ":%s CHATHISTORY TARGETS %s %s",
		me.name, r->object, ts);

	if (mtags)
		free_message_tags(mtags);

	return 1;
}

void chathistory_targets(Client *client, HistoryFilter *filter, int limit)
{
	Membership *mp;
	HistoryResult *r;
	char batch[BATCHLEN+1];
	int sent = 0;

	batch[0] = '\0';
	if (HasCapability(client, "batch"))
	{
		/* Start a new batch */
		generate_batch_id(batch);
		sendto_one(client, NULL, ":%s BATCH +%s draft/chathistory-targets", me.name, batch);
	}

	filter->cmd = HFC_BEFORE;
	if (strcmp(filter->timestamp_a, filter->timestamp_b) < 0)
	{
		/* Swap if needed */
		char *swap = filter->timestamp_a;
		filter->timestamp_a = filter->timestamp_b;
		filter->timestamp_b = swap;
	}
	filter->limit = 1;

	for (mp = client->user->channel; mp; mp = mp->next)
	{
		Channel *channel = mp->channel;
		r = history_request(channel->chname, filter);
		if (r->log && chathistory_targets_send_line(client, r, batch))
		{
			if (++sent >= limit)
				break; /* We are done */
		}
		free_history_result(r);
		r = NULL;
	}

	/* End of batch */
	if (*batch)
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch);
}

CMD_FUNC(cmd_chathistory)
{
	HistoryFilter *filter = NULL;
	HistoryResult *r = NULL;
	Channel *channel;

	memset(&filter, 0, sizeof(filter));

	/* This command is only for local users */
	if (!MyUser(client))
		return;

	if ((parc < 5) || BadPtr(parv[4]))
	{
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS :Insufficient parameters", me.name);
		return;
	}

	if (!HasCapability(client, "server-time"))
	{
		sendnotice(client, "Your IRC client does not support the 'server-time' capability");
		sendnotice(client, "https://ircv3.net/specs/extensions/server-time-3.2.html");
		sendnotice(client, "History request refused.");
		return;
	}

	if (!strcmp(parv[1], "TARGETS"))
	{
		Membership *mp;
		int limit;

		filter = safe_alloc(sizeof(HistoryFilter));
		/* Below this point, instead of 'return', use 'goto end' */

		if (!chathistory_token(parv[2], "timestamp", &filter->timestamp_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_b))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx",
				me.name, parv[1], parv[4]);
			goto end;
		}
		limit = atoi(parv[4]);
		chathistory_targets(client, filter, limit);
		goto end;
	}

	channel = find_channel(parv[2], NULL);
	if (!channel || !IsMember(client, channel) || !has_channel_mode(channel, 'H'))
	{
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_TARGET %s %s :Messages could not be retrieved",
			me.name, parv[1], parv[2]);
		return;
	}

	filter = safe_alloc(sizeof(HistoryFilter));
	/* Below this point, instead of 'return', use 'goto end', which takes care of the freeing of 'filter' and 'history' */

	if (!strcmp(parv[1], "BEFORE"))
	{
		filter->cmd = HFC_BEFORE;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcmp(parv[1], "AFTER"))
	{
		filter->cmd = HFC_AFTER;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcmp(parv[1], "LATEST"))
	{
		filter->cmd = HFC_LATEST;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a) &&
		    strcmp(parv[3], "*"))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx or *",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcmp(parv[1], "AROUND"))
	{
		filter->cmd = HFC_AROUND;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcmp(parv[1], "BETWEEN"))
	{
		filter->cmd = HFC_BETWEEN;
		if (BadPtr(parv[5]))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s :Insufficient parameters", parv[1], me.name);
			goto end;
		}
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		if (!chathistory_token(parv[4], "timestamp", &filter->timestamp_b) &&
		    !chathistory_token(parv[4], "msgid", &filter->msgid_b))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[4]);
			goto end;
		}
		filter->limit = atoi(parv[5]);
	} else {
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s :Invalid subcommand", me.name, parv[1]);
		goto end;
	}

	if (filter->limit <= 0)
	{
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %d :Specified limit is =<0",
			me.name, parv[1], filter->limit);
		goto end;
	}

	if (filter->limit > CHATHISTORY_LIMIT)
		filter->limit = CHATHISTORY_LIMIT;

	if ((r = history_request(channel->chname, filter)))
		history_send_result(client, r);

end:
	if (filter)
		free_history_filter(filter);
	if (r)
		free_history_result(r);
}
