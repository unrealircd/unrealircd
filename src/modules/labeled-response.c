/*
 *   IRC - Internet Relay Chat, src/modules/labeled-response.c
 *   (C) 2019 Syzop & The UnrealIRCd Team
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
	"labeled-response",
	"5.0",
	"Labeled response CAP",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Data structures */
typedef struct LabeledResponseContext LabeledResponseContext;
struct LabeledResponseContext {
	Client *client; /**< The client who issued the original command with a label */
	char label[256]; /**< The label attached to this command */
	char batch[BATCHLEN+1]; /**< The generated batch id */
	int responses; /**< Number of lines sent back to client */
	int sent_remote; /**< Command has been sent to remote server */
	char firstbuf[MAXLINELENGTH]; /**< First buffered response */
};

/* Forward declarations */
int lr_pre_command(Client *from, MessageTag *mtags, const char *buf);
int lr_post_command(Client *from, MessageTag *mtags, const char *buf);
int lr_close_connection(Client *client);
int lr_packet(Client *from, Client *to, Client *intended_to, char **msg, int *len);
void *_labeled_response_save_context(void);
void _labeled_response_set_context(void *ctx);
void _labeled_response_force_end(void);

/* Our special version of SupportBatch() assumes that remote servers always handle it */
#define SupportBatch(x)		(MyConnect(x) ? HasCapability((x), "batch") : 1)
#define SupportLabel(x)		(HasCapabilityFast((x), CAP_LABELED_RESPONSE))

/* Variables */
static LabeledResponseContext currentcmd;
static long CAP_LABELED_RESPONSE = 0L;

static char packet[MAXLINELENGTH*2];

int labeled_response_mtag_is_ok(Client *client, const char *name, const char *value);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddPVoid(modinfo->handle, EFUNC_LABELED_RESPONSE_SAVE_CONTEXT, _labeled_response_save_context);
	EfunctionAddVoid(modinfo->handle, EFUNC_LABELED_RESPONSE_SET_CONTEXT, _labeled_response_set_context);
	EfunctionAddVoid(modinfo->handle, EFUNC_LABELED_RESPONSE_FORCE_END, _labeled_response_force_end);

	return MOD_SUCCESS;
}

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&currentcmd, 0, sizeof(currentcmd));

	memset(&cap, 0, sizeof(cap));
	cap.name = "labeled-response";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_LABELED_RESPONSE);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "label";
	mtag.is_ok = labeled_response_mtag_is_ok;
	mtag.clicap_handler = c;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAdd(modinfo->handle, HOOKTYPE_PRE_COMMAND, -1000000000, lr_pre_command);
	HookAdd(modinfo->handle, HOOKTYPE_POST_COMMAND, 1000000000, lr_post_command);
	HookAdd(modinfo->handle, HOOKTYPE_CLOSE_CONNECTION, 1000000000, lr_close_connection);
	HookAdd(modinfo->handle, HOOKTYPE_PACKET, 1000000000, lr_packet);

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

int lr_pre_command(Client *from, MessageTag *mtags, const char *buf)
{
	memset(&currentcmd, 0, sizeof(currentcmd));
	labeled_response_inhibit = labeled_response_inhibit_end = labeled_response_force = 0;

	if (IsServer(from))
		return 0;

	for (; mtags; mtags = mtags->next)
	{
		if (!strcmp(mtags->name, "label") && mtags->value)
		{
			strlcpy(currentcmd.label, mtags->value, sizeof(currentcmd.label));
			currentcmd.client = from;
			break;
		}
	}

	return 0;
}

char *gen_start_batch(void)
{
	static char buf[512];

	generate_batch_id(currentcmd.batch);

	if (MyConnect(currentcmd.client))
	{
		/* Local connection */
		snprintf(buf, sizeof(buf), "@label=%s :%s BATCH +%s labeled-response",
			currentcmd.label,
			me.name,
			currentcmd.batch);
	} else {
		/* Remote connection: requires intra-server BATCH syntax */
		snprintf(buf, sizeof(buf), "@label=%s :%s BATCH %s +%s labeled-response",
			currentcmd.label,
			me.name,
			currentcmd.client->name,
			currentcmd.batch);
	}
	return buf;
}

int lr_post_command(Client *from, MessageTag *mtags, const char *buf)
{
	/* ** IMPORTANT **
	 * Take care NOT to return here, use 'goto done' instead
	 * as some variables need to be cleared.
	 */

	/* We may have to send a response or end a BATCH here, if all of
	 * the following is true:
	 * 1. The client is still online (from is not NULL)
	 * 2. A "label" was attached
	 * 3. The client supports BATCH (or is remote)
	 * 4. The command has not been forwarded to a remote server
	 *    (in which case they would be handling it, and not us)
	 * 5. Unless labeled_response_force is set, in which case
	 *    we are assumed to have handled it anyway (necessary for
	 *    commands like PRIVMSG, quite rare).
	 */
	if (from && currentcmd.client &&
	    !(currentcmd.sent_remote && !currentcmd.responses && !labeled_response_force))
	{
		Client *savedptr;

		if (currentcmd.responses == 0)
		{
			MessageTag *m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, "label");
			safe_strdup(m->value, currentcmd.label);
			memset(&currentcmd, 0, sizeof(currentcmd));
			sendto_one(from, m, ":%s ACK", me.name);
			free_message_tags(m);
			goto done;
		} else
		if (currentcmd.responses == 1)
		{
			/* We have buffered this response earlier,
			 * now we will send it
			 */
			int more_tags = currentcmd.firstbuf[0] == '@';
			currentcmd.client = NULL; /* prevent lr_packet from interfering */
			snprintf(packet, sizeof(packet)-3,
				 "@label=%s%s%s",
				 currentcmd.label,
				 more_tags ? ";" : " ",
				 more_tags ? currentcmd.firstbuf+1 : currentcmd.firstbuf);
			/* Format the IRC message correctly here, so we can take the
			 * quick path through sendbufto_one().
			 */
			strlcat(packet, "\r\n", sizeof(packet));
			sendbufto_one(from, packet, strlen(packet));
			goto done;
		}

		/* End the batch */
		if (!labeled_response_inhibit_end)
		{
			savedptr = currentcmd.client;
			currentcmd.client = NULL;
			if (MyConnect(savedptr))
				sendto_one(from, NULL, ":%s BATCH -%s", me.name, currentcmd.batch);
			else
				sendto_one(from, NULL, ":%s BATCH %s -%s", me.name, savedptr->name, currentcmd.batch);
		}
	}
done:
	memset(&currentcmd, 0, sizeof(currentcmd));
	labeled_response_inhibit = labeled_response_inhibit_end = labeled_response_force = 0;
	return 0;
}

int lr_close_connection(Client *client)
{
	/* Flush all data before closing connection */
	lr_post_command(client, NULL, NULL);
	return 0;
}

/** Helper function for lr_packet() to skip the message tags prefix,
 * and possibly @batch as well.
 */
char *skip_tags(char *msg)
{
	if (*msg != '@')
		return msg;
	if (!strncmp(msg, "@batch", 6))
	{
		char *p;
		for (p = msg; *p; p++)
			if ((*p == ';') || (*p == ' '))
				return p;
	}
	return msg+1; /* just skip the '@' */
}

int lr_packet(Client *from, Client *to, Client *intended_to, char **msg, int *len)
{
	if (currentcmd.client && !labeled_response_inhibit)
	{
		/* Labeled response is active */
		if (currentcmd.client == intended_to)
		{
			/* Add the label */
			if (currentcmd.responses == 0)
			{
				int n = *len;
				if (n > sizeof(currentcmd.firstbuf))
					n = sizeof(currentcmd.firstbuf);
				strlcpy(currentcmd.firstbuf, *msg, n);
				/* Don't send anything -- yet */
				*msg = NULL;
				*len = 0;
			} else
			if (currentcmd.responses == 1)
			{
				/* Start the batch now, normally this would be a sendto_one()
				 * but doing so is not possible since we are in the sending code :(
				 * The code below is almost unbearable to see, but the alternative
				 * is to use an intermediate buffer or pointer jugling, of which
				 * the former is slower than this implementation and with the latter
				 * it is easy to make a mistake and create an overflow issue.
				 * So guess I'll stick with this...
				 */
				char *batchstr = gen_start_batch();
				int more_tags_one = currentcmd.firstbuf[0] == '@';
				int more_tags_two = **msg == '@';

				if (!strncmp(*msg, "@batch", 6))
				{
					/* Special case: current message (*msg) already contains a batch */
					snprintf(packet, sizeof(packet),
						 "%s\r\n"
						 "@batch=%s%s%s\r\n"
						 "%s",
						 batchstr,
						 currentcmd.batch,
						 more_tags_one ? ";" : " ",
						 more_tags_one ? currentcmd.firstbuf+1 : currentcmd.firstbuf,
						 *msg);
				} else
				{
					/* Regular case: current message (*msg) contains no batch yet, add one.. */
					snprintf(packet, sizeof(packet),
						 "%s\r\n"
						 "@batch=%s%s%s\r\n"
						 "@batch=%s%s%s",
						 batchstr,
						 currentcmd.batch,
						 more_tags_one ? ";" : " ",
						 more_tags_one ? currentcmd.firstbuf+1 : currentcmd.firstbuf,
						 currentcmd.batch,
						 more_tags_two ? ";" : " ",
						 more_tags_two ? *msg+1 : *msg);
				}
				*msg = packet;
				*len = strlen(*msg);
			} else {
				/* >2 responses.... the first 2 have already been sent */
				if (!strncmp(*msg, "@batch", 6))
				{
					/* No buffer change needed, already contains a (now inner) batch */
				} else {
					int more_tags = **msg == '@';
					snprintf(packet, sizeof(packet), "@batch=%s%s%s",
						currentcmd.batch,
						more_tags ? ";" : " ",
						more_tags ? *msg+1 : *msg);
					*msg = packet;
					*len = strlen(*msg);
				}
			}
			currentcmd.responses++;
		}
		else if (IsServer(to) || !MyUser(to))
		{
			currentcmd.sent_remote = 1;
		}
	}

	return 0;
}

/** This function verifies if the client sending the
 * tag is permitted to do so and uses a permitted syntax.
 */
int labeled_response_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (BadPtr(value))
		return 0;

	if (IsServer(client))
		return 1;

	/* Ignore the label if the client does not support both
	 * (draft/)labeled-response and batch. Yeah, batch too,
	 * it's too much hassle to support labeled-response without
	 * batch and the end result is quite broken too.
	 */
	if (MyUser(client) && (!SupportLabel(client) || !SupportBatch(client)))
		return 0;

	/* Do some basic sanity checking for non-servers */
	if (strlen(value) <= 64)
		return 1;

	return 0;
}

/** Save current context for later use in labeled-response.
 * Currently used in /LIST. Is not planned for other places tbh.
 */
void *_labeled_response_save_context(void)
{
	LabeledResponseContext *ctx = safe_alloc(sizeof(LabeledResponseContext));
	memcpy(ctx, &currentcmd, sizeof(LabeledResponseContext));
	return (void *)ctx;
}

/** Set previously saved context 'ctx', or clear the context.
 * @param ctx    The context, or NULL to clear the context.
 * @note The client from the previously saved context should be
 *       the same. Don't save one context when processing
 *       client A and then restore it when processing client B (duh).
 */
void _labeled_response_set_context(void *ctx)
{
	if (ctx == NULL)
	{
		/* This means: clear the current context */
		memset(&currentcmd, 0, sizeof(currentcmd));
	} else {
		/* Set the current context to the provided one */
		memcpy(&currentcmd, ctx, sizeof(LabeledResponseContext));
	}
}

/** Force an end of the labeled-response (only used in /LIST atm) */
void _labeled_response_force_end(void)
{
	if (currentcmd.client)
		lr_post_command(currentcmd.client, NULL, NULL);
}
