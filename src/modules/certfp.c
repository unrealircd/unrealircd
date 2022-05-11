/*
 * Certificate Fingerprint Module
 * This grabs the SHA256 fingerprint of the TLS client certificate
 * the user is using, shares it with the other servers (and rest of
 * UnrealIRCd) and shows it in /WHOIS etc.
 *
 * (C) Copyright 2014-2015 The UnrealIRCd team (Syzop, DBoyz, Nath and others)
 * 
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"certfp",
	"5.0",
	"Certificate fingerprint",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
void certfp_free(ModData *m);
const char *certfp_serialize(ModData *m);
void certfp_unserialize(const char *str, ModData *m);
int certfp_handshake(Client *client);
int certfp_connect(Client *client);
int certfp_whois(Client *client, Client *target, NameValuePrioList **list);

ModDataInfo *certfp_md; /* Module Data structure which we acquire */

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "certfp";
	mreq.free = certfp_free;
	mreq.serialize = certfp_serialize;
	mreq.unserialize = certfp_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	certfp_md = ModDataAdd(modinfo->handle, mreq);
	if (!certfp_md)
		abort();

	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, certfp_connect);
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, certfp_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_HANDSHAKE_OUT, 0, certfp_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, certfp_whois);

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

/* 
 * Obtain client's fingerprint.
 */
char *get_fingerprint_for_client(Client *client)
{
	unsigned int n;
	unsigned int l;
	unsigned char md[EVP_MAX_MD_SIZE];
	static char hex[EVP_MAX_MD_SIZE * 2 + 1];
	char hexchars[16] = "0123456789abcdef";
	const EVP_MD *digest = EVP_sha256();
	X509 *x509_clientcert = NULL;

	if (!MyConnect(client) || !client->local->ssl)
		return NULL;
	
	x509_clientcert = SSL_get_peer_certificate(client->local->ssl);

	if (x509_clientcert)
	{
		if (X509_digest(x509_clientcert, digest, md, &n)) {
			int j = 0;
			for	(l=0; l<n; l++) {
				hex[j++] = hexchars[(md[l] >> 4) & 0xF];
				hex[j++] = hexchars[md[l] & 0xF];
			}
			hex[j] = '\0';
			X509_free(x509_clientcert);
			return hex;
		}
		X509_free(x509_clientcert);
	}
	return NULL;
}

int certfp_handshake(Client *client)
{
	if (client->local->ssl)
	{
		char *fp = get_fingerprint_for_client(client);

		if (!fp)
			return 0;

		moddata_client_set(client, "certfp", fp); /* set & broadcast */
	}
	return 0;
}

int certfp_connect(Client *client)
{
	if (IsSecure(client))
	{
		const char *fp = moddata_client_get(client, "certfp");
	
		if (fp && !iConf.no_connect_tls_info)
			sendnotice(client, "*** Your TLS certificate fingerprint is %s", fp);
	}

	return 0;
}

int certfp_whois(Client *client, Client *target, NameValuePrioList **list)
{
	const char *fp = moddata_client_get(target, "certfp");
	char buf[512];

	if (!fp)
		return 0;

	if (whois_get_policy(client, target, "certfp") == WHOIS_CONFIG_DETAILS_FULL)
		add_nvplist_numeric(list, 0, "certfp", client, RPL_WHOISCERTFP, target->name, fp);

	return 0;
}

void certfp_free(ModData *m)
{
	safe_free(m->str);
}

const char *certfp_serialize(ModData *m)
{
	if (!m->str)
		return NULL;
	return m->str;
}

void certfp_unserialize(const char *str, ModData *m)
{
	safe_strdup(m->str, str);
}
