/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/tls.c
 *      (C) 2000 hq.alert.sk (base)
 *      (C) 2000 Carsten V. Munk <stskeeps@tspre.org> 
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

/** @file
 * @brief TLS functions
 */

#include "unrealircd.h"
#include "openssl_hostname_validation.h"

#ifdef _WIN32
#define IDC_PASS                        1166
extern HINSTANCE hInst;
extern HWND hwIRCDWnd;
#endif

#define FUNC_TLS_READ 1
#define FUNC_TLS_WRITE 2
#define FUNC_TLS_ACCEPT 3
#define FUNC_TLS_CONNECT 4

/* Forward declarations */
static int fatal_tls_error(int ssl_error, int where, int my_errno, Client *client);
int cipher_check(SSL_CTX *ctx, char **errstr);
int certificate_quality_check(SSL_CTX *ctx, char **errstr);

/* The TLS structures */
SSL_CTX *ctx_server;
SSL_CTX *ctx_client;

char *TLSKeyPasswd;

typedef struct {
	int *size;
	char **buffer;
} StreamIO;

MODVAR int tls_client_index = 0;

#ifdef _WIN32
/** Ask private key password (Windows GUI mode only) */
LRESULT TLS_key_passwd_dialog(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static StreamIO *stream;
	switch (Message) {
		case WM_INITDIALOG:
			stream = (StreamIO*)lParam;
			return TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL) {
				*stream->buffer = NULL;
				EndDialog(hDlg, IDCANCEL);
			}
			else if (LOWORD(wParam) == IDOK) {
				GetDlgItemText(hDlg, IDC_PASS, *stream->buffer, *stream->size);
				EndDialog(hDlg, IDOK);
			}
			return FALSE;
		case WM_CLOSE:
			*stream->buffer = NULL;
			EndDialog(hDlg, IDCANCEL);
		default:
			return FALSE;
	}
}
#endif				

/** Return error string for OpenSSL error.
 * @param err		OpenSSL error number to lookup
 * @param my_errno	The value of errno to use in case we want to call strerror().
 * @returns Error string, only valid until next call to this function.
 */
const char *ssl_error_str(int err, int my_errno)
{
	static char ssl_errbuf[256];
	char *ssl_errstr = NULL;

	switch(err)
	{
		case SSL_ERROR_NONE:
			ssl_errstr = "OpenSSL: No error";
			break;
		case SSL_ERROR_SSL:
			ssl_errstr = "Internal OpenSSL error or protocol error";
			break;
		case SSL_ERROR_WANT_READ:
			ssl_errstr = "OpenSSL functions requested a read()";
			break;
		case SSL_ERROR_WANT_WRITE:
			ssl_errstr = "OpenSSL functions requested a write()";
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			ssl_errstr = "OpenSSL requested a X509 lookup which didn't arrive";
			break;
		case SSL_ERROR_SYSCALL:
			snprintf(ssl_errbuf, sizeof(ssl_errbuf), "%s", STRERROR(my_errno));
			ssl_errstr = ssl_errbuf;
			break;
		case SSL_ERROR_ZERO_RETURN:
			ssl_errstr = "Underlying socket operation returned zero";
			break;
		case SSL_ERROR_WANT_CONNECT:
			ssl_errstr = "OpenSSL functions wanted a connect()";
			break;
		default:
			ssl_errstr = "Unknown OpenSSL error (huh?)";
	}
	return ssl_errstr;
}

/** Ask certificate private key password (rare) */
int TLS_key_passwd_cb(char *buf, int size, int rwflag, void *password)
{
	char *pass;
	static int before = 0;
	static char beforebuf[1024];
#ifdef _WIN32
	StreamIO stream;
	char passbuf[512];	
	int passsize = 512;
#endif
	if (before)
	{
		strlcpy(buf, beforebuf, size);
		return strlen(buf);
	}
#ifndef _WIN32
	pass = getpass("Password for TLS private key: ");
#else
	pass = passbuf;
	stream.buffer = &pass;
	stream.size = &passsize;
	DialogBoxParam(hInst, "TLSKey", hwIRCDWnd, (DLGPROC)TLS_key_passwd_dialog, (LPARAM)&stream); 
#endif
	if (pass)
	{
		strlcpy(buf, pass, size);
		strlcpy(beforebuf, pass, sizeof(beforebuf));
		before = 1;
		TLSKeyPasswd = beforebuf;
		return (strlen(buf));
	}
	return 0;
}

/** Verify certificate callback. */
static int ssl_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	/* We accept the connection. Certificate verifiction takes
	 * place elsewhere, such as in _verify_link().
	 */
	return 1;
}

/** Get Client pointer by SSL pointer */
Client *get_client_by_ssl(SSL *ssl)
{
	return SSL_get_ex_data(ssl, tls_client_index);
}

/** Set requested server name as indicated by SNI */
static void set_client_sni_name(SSL *ssl, char *name)
{
	Client *client = get_client_by_ssl(ssl);
	if (client)
		safe_strdup(client->local->sni_servername, name);
}

/** Hostname callback, used for SNI */
static int ssl_hostname_callback(SSL *ssl, int *unk, void *arg)
{
	char *name = (char *)SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	ConfigItem_sni *sni;

	if (name && (sni = find_sni(name)))
	{
		SSL_set_SSL_CTX(ssl, sni->ssl_ctx);
		set_client_sni_name(ssl, name);
	}

	return SSL_TLSEXT_ERR_OK;
}

/** Disable TLS protocols as set by config */
void disable_ssl_protocols(SSL_CTX *ctx, TLSOptions *tlsoptions)
{
	/* OpenSSL has three mechanisms for protocol version control... */

#ifdef HAS_SSL_CTX_SET_SECURITY_LEVEL
	/* The first one is setting a "security level" as introduced
	 * by OpenSSL 1.1.0. Some Linux distro's like Ubuntu 20.04
	 * seemingly compile with -DOPENSSL_TLS_SECURITY_LEVEL=2.
	 * This means the application (UnrealIRCd) is unable to allow
	 * TLSv1.0/1.1 even if the application is configured to do so.
	 * So here we set the level to 1, but -again- ONLY if we are
	 * configured to allow TLSv1.0 or v1.1, of course.
	 */
	if ((tlsoptions->protocols & TLS_PROTOCOL_TLSV1) ||
	    (tlsoptions->protocols & TLS_PROTOCOL_TLSV1_1))
	{
		SSL_CTX_set_security_level(ctx, 0);
	}
#endif

	/* The remaining two mechanisms are:
	 * The old way, which is most flexible, is to use:
	 * SSL_CTX_set_options(... SSL_OP_NO_<version>) which allows
	 * you to disable each and every specific TLS version.
	 *
	 * And the new way, which only allows setting a
	 * minimum and maximum protocol version, using:
	 * SSL_CTX_set_min_proto_version(... <version>)
	 * SSL_CTX_set_max_proto_version(....<version>)
	 *
	 * We prefer the old way, but because OpenSSL 1.0.1 and
	 * OS's like Debian use system-wide options we are also
	 * forced to use the new way... or at least to set a
	 * minimum protocol version to begin with.
	 */
#ifdef HAS_SSL_CTX_SET_MIN_PROTO_VERSION
	if (!(tlsoptions->protocols & TLS_PROTOCOL_TLSV1) &&
	    !(tlsoptions->protocols & TLS_PROTOCOL_TLSV1_1))
	{
		SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
	} else
	if (!(tlsoptions->protocols & TLS_PROTOCOL_TLSV1))
	{
		SSL_CTX_set_min_proto_version(ctx, TLS1_1_VERSION);
	} else
	{
		SSL_CTX_set_min_proto_version(ctx, TLS1_VERSION);
	}
#endif
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2); /* always disable SSLv2 */
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3); /* always disable SSLv3 */

#ifdef SSL_OP_NO_TLSv1
	if (!(tlsoptions->protocols & TLS_PROTOCOL_TLSV1))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
#endif

#ifdef SSL_OP_NO_TLSv1_1
	if (!(tlsoptions->protocols & TLS_PROTOCOL_TLSV1_1))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1);
#endif

#ifdef SSL_OP_NO_TLSv1_2
	if (!(tlsoptions->protocols & TLS_PROTOCOL_TLSV1_2))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_2);
#endif

#ifdef SSL_OP_NO_TLSv1_3
	if (!(tlsoptions->protocols & TLS_PROTOCOL_TLSV1_3))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_3);
#endif
}

/** Initialize TLS context
 * @param tlsoptions	The ::tls-options configuration
 * @param server	Set to 1 if we are initializing a server, 0 for client.
 * @returns The TLS context (SSL_CTX) or NULL in case of error.
 */
SSL_CTX *init_ctx(TLSOptions *tlsoptions, int server)
{
	SSL_CTX *ctx;
	char *errstr = NULL;

	if (server)
		ctx = SSL_CTX_new(SSLv23_server_method());
	else
		ctx = SSL_CTX_new(SSLv23_client_method());

	if (!ctx)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_LOAD_FAILED", NULL,
		           "Failed to do SSL_CTX_new() !?\n$tls_error.all",
		           log_data_tls_error());
		return NULL;
	}
	disable_ssl_protocols(ctx, tlsoptions);
	SSL_CTX_set_default_passwd_cb(ctx, TLS_key_passwd_cb);

	if (server && !(tlsoptions->options & TLSFLAG_DISABLECLIENTCERT))
	{
		/* We tell OpenSSL/LibreSSL to verify the certificate and set our callback.
		 * Our callback will always accept the certificate since actual checking
		 * will take place elsewhere. Why? Because certificate is (often) delayed
		 * until after the TLS handshake. Such as in the case of link blocks where
		 * _verify_link() will take care of it only after we learned what server
		 * we are dealing with (and if we should verify certificates for that server).
		 */
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE | (tlsoptions->options & TLSFLAG_FAILIFNOCERT ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0), ssl_verify_callback);
	}
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
#ifndef SSL_OP_NO_TICKET
 #error "Your system has an outdated OpenSSL version. Please upgrade OpenSSL."
#endif
	SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);

	if (SSL_CTX_use_certificate_chain_file(ctx, tlsoptions->certificate_file) <= 0)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_LOAD_FAILED", NULL,
		           "Failed to load TLS certificate $filename\n$tls_error.all",
		           log_data_string("filename", tlsoptions->certificate_file),
		           log_data_tls_error());
		goto fail;
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, tlsoptions->key_file, SSL_FILETYPE_PEM) <= 0)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_LOAD_FAILED", NULL,
		           "Failed to load TLS private key $filename\n$tls_error.all",
		           log_data_string("filename", tlsoptions->key_file),
		           log_data_tls_error());
		goto fail;
	}

	if (!SSL_CTX_check_private_key(ctx))
	{
		unreal_log(ULOG_ERROR, "config", "TLS_LOAD_FAILED", NULL,
		           "Check for TLS private key failed $filename\n$tls_error.all",
		           log_data_string("filename", tlsoptions->key_file),
		           log_data_tls_error());
		goto fail;
	}

	if (SSL_CTX_set_cipher_list(ctx, tlsoptions->ciphers) == 0)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_INVALID_CIPHERS_LIST", NULL,
		           "Failed to set TLS cipher list '$tls_ciphers_list'\n$tls_error.all",
		           log_data_string("tls_ciphers_list", tlsoptions->ciphers),
		           log_data_tls_error());
		goto fail;
	}

#ifdef SSL_OP_NO_TLSv1_3
	if (SSL_CTX_set_ciphersuites(ctx, tlsoptions->ciphersuites) == 0)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_INVALID_CIPHERSUITES_LIST", NULL,
		           "Failed to set TLS ciphersuites list '$tls_ciphersuites_list'\n$tls_error.all",
		           log_data_string("tls_ciphersuites_list", tlsoptions->ciphersuites),
		           log_data_tls_error());
		goto fail;
	}
#endif

	if (!cipher_check(ctx, &errstr))
	{
		unreal_log(ULOG_ERROR, "config", "TLS_CIPHER_CHECK_FAILED", NULL,
		           "There is a problem with your TLS 'ciphers' configuration setting: $quality_check_error\n"
		           "Remove the ciphers setting from your configuration file to use safer defaults, or change the cipher setting.",
		           log_data_string("quality_check_error", errstr));
		goto fail;
	}

	if (!certificate_quality_check(ctx, &errstr))
	{
		unreal_log(ULOG_ERROR, "config", "TLS_CERTIFICATE_CHECK_FAILED", NULL,
		           "There is a problem with your TLS certificate '$filename': $quality_check_error\n"
		           "If you use the standard UnrealIRCd certificates then you can simply run 'make pem' and 'make install' "
		           "from your UnrealIRCd source directory (eg: ~/unrealircd-6.X.Y/) to create and install new certificates",
		           log_data_string("filename", tlsoptions->certificate_file),
		           log_data_string("quality_check_error", errstr));
		goto fail;
	}

	if (server)
		SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

	if (tlsoptions->trusted_ca_file)
	{
		if (!SSL_CTX_load_verify_locations(ctx, tlsoptions->trusted_ca_file, NULL))
		{
			unreal_log(ULOG_ERROR, "config", "TLS_LOAD_FAILED", NULL,
				   "Failed to load trusted-ca-file $filename\n$tls_error.all",
				   log_data_string("filename", tlsoptions->trusted_ca_file),
				   log_data_tls_error());
			goto fail;
		}
	}

	if (server)
	{
#if defined(SSL_CTX_set_ecdh_auto)
		/* OpenSSL 1.0.x requires us to explicitly turn this on */
		SSL_CTX_set_ecdh_auto(ctx, 1);
#elif OPENSSL_VERSION_NUMBER < 0x10100000L
		/* Even older versions require require setting a fixed curve.
		 * NOTE: Don't be confused by the <1.1.x check.
		 * Yes, it must be there. Do not remove it!
		 */
		SSL_CTX_set_tmp_ecdh(ctx, EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
#else
		/* If we end up here we don't have SSL_CTX_set_ecdh_auto
		 * and we are on OpenSSL 1.1.0 or later. We don't need to
		 * do anything then, since auto ecdh is the default.
		 */
#endif
		/* Let's see if we need to (and can) set specific curves */
		if (tlsoptions->ecdh_curves)
		{
#ifdef HAS_SSL_CTX_SET1_CURVES_LIST
			if (!SSL_CTX_set1_curves_list(ctx, tlsoptions->ecdh_curves))
			{
				unreal_log(ULOG_ERROR, "config", "TLS_INVALID_ECDH_CURVES_LIST", NULL,
					   "Failed to set ecdh-curves '$ecdh_curves_list'\n$tls_error.all\n"
					   "HINT: o get a list of supported curves with the appropriate names, "
					   "run 'openssl ecparam -list_curves' on the server. "
					   "Separate multiple curves by colon, for example: "
					   "ecdh-curves \"secp521r1:secp384r1\".",
					   log_data_string("ecdh_curves_list", tlsoptions->ecdh_curves),
					   log_data_tls_error());
				goto fail;
			}
#else
			/* We try to avoid this in the config code, but better have
			 * it here too than be sorry if someone screws up:
			 */
			unreal_log(ULOG_ERROR, "config", "BUG_ECDH_CURVES", NULL,
			           "ecdh-curves specified but not supported by library -- BAD!");
			goto fail;
#endif
		}
		/* We really want the ECDHE/ECDHE to be generated per-session.
		 * Added in 2015 for safety. Seems OpenSSL was smart enough
		 * to make this the default in 2016 after a security advisory.
		 */
		SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE|SSL_OP_SINGLE_DH_USE);
	}

	if (server)
	{
		SSL_CTX_set_tlsext_servername_callback(ctx, ssl_hostname_callback);
	}

	return ctx;
fail:
	SSL_CTX_free(ctx);
	return NULL;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
MODVAR EVP_MD *sha256_function; /**< SHA256 function for EVP_DigestInit_ex() call */
MODVAR EVP_MD *sha1_function; /**< SHA1 function for EVP_DigestInit_ex() call */
MODVAR EVP_MD *md5_function; /**< MD5 function for EVP_DigestInit_ex() call */
#endif

/** Early initalization of TLS subsystem - called on startup */
int early_init_tls(void)
{
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	/* This is used to track (SSL *) <--> (Client *) relationships: */
	tls_client_index = SSL_get_ex_new_index(0, "tls_client", NULL, NULL, NULL);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	sha256_function = EVP_MD_fetch(NULL, "SHA2-256", NULL);
	if (!sha256_function)
	{
		fprintf(stderr, "Could not find SHA256 algorithm in TLS library\n");
		exit(6);
	}

	sha1_function = EVP_MD_fetch(NULL, "SHA1", NULL);
	if (!sha1_function)
	{
		fprintf(stderr, "Could not find SHA1 algorithm in TLS library\n");
		exit(6);
	}

	md5_function = EVP_MD_fetch(NULL, "MD5", NULL);
	if (!md5_function)
	{
		fprintf(stderr, "Could not find MD5 algorithm in TLS library\n");
		exit(6);
	}
#endif
	return 1;
}

/** Initialize the server and client contexts.
 * This is only possible after reading the configuration file.
 */
int init_tls(void)
{
	ctx_server = init_ctx(iConf.tls_options, 1);
	if (!ctx_server)
		return 0;
	ctx_client = init_ctx(iConf.tls_options, 0);
	if (!ctx_client)
		return 0;
	return 1;
}

/** Reinitialize TLS server and client contexts - after REHASH -tls
 */
int reinit_tls(void)
{
	SSL_CTX *tmp;
	ConfigItem_listen *listen;
	ConfigItem_sni *sni;
	ConfigItem_link *link;

	tmp = init_ctx(iConf.tls_options, 1);
	if (!tmp)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_RELOAD_FAILED", NULL,
		           "TLS Reload failed. See previous errors.");
		return 0;
	}
	if (ctx_server)
		SSL_CTX_free(ctx_server);
	ctx_server = tmp; /* activate */
	
	tmp = init_ctx(iConf.tls_options, 0);
	if (!tmp)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_RELOAD_FAILED", NULL,
		           "TLS Reload failed at client context. See previous errors.");
		return 0;
	}
	if (ctx_client)
		SSL_CTX_free(ctx_client);
	ctx_client = tmp; /* activate */

	/* listen::tls-options.... */
	for (listen = conf_listen; listen; listen = listen->next)
	{
		if (listen->tls_options)
		{
			tmp = init_ctx(listen->tls_options, 1);
			if (!tmp)
			{
				unreal_log(ULOG_ERROR, "config", "TLS_RELOAD_FAILED", NULL,
					   "TLS Reload failed at listen::tls-options. See previous errors.");
				return 0;
			}
			if (listen->ssl_ctx)
				SSL_CTX_free(listen->ssl_ctx);
			listen->ssl_ctx = tmp; /* activate */
		}
	}

	/* sni::tls-options.... */
	for (sni = conf_sni; sni; sni = sni->next)
	{
		if (sni->tls_options)
		{
			tmp = init_ctx(sni->tls_options, 1);
			if (!tmp)
			{
				unreal_log(ULOG_ERROR, "config", "TLS_RELOAD_FAILED", NULL,
					   "TLS Reload failed at sni::tls-options. See previous errors.");
				return 0;
			}
			if (sni->ssl_ctx)
				SSL_CTX_free(sni->ssl_ctx);
			sni->ssl_ctx = tmp; /* activate */
		}
	}

	/* link::outgoing::tls-options.... */
	for (link = conf_link; link; link = link->next)
	{
		if (link->tls_options)
		{
			tmp = init_ctx(link->tls_options, 0);
			if (!tmp)
			{
				unreal_log(ULOG_ERROR, "config", "TLS_RELOAD_FAILED", NULL,
					   "TLS Reload failed at link $servername due to outgoing::tls-options. See previous errors.",
					   log_data_string("servername", link->servername));
				return 0;
			}
			if (link->ssl_ctx)
				SSL_CTX_free(link->ssl_ctx);
			link->ssl_ctx = tmp; /* activate */
		}
	}

	return 1;
}

/** Set SSL connection as nonblocking */
void SSL_set_nonblocking(SSL *s)
{
	BIO_set_nbio(SSL_get_rbio(s),1);
	BIO_set_nbio(SSL_get_wbio(s),1);
}

/** Get TLS ciphersuite */
const char *tls_get_cipher(Client *client)
{
	static char buf[256];
	const char *cached;

	cached = moddata_client_get(client, "tls_cipher");
	if (cached)
		return cached;

	if (!MyConnect(client) || !client->local->ssl)
		return NULL;

	buf[0] = '\0';
	strlcpy(buf, SSL_get_version(client->local->ssl), sizeof(buf));
	strlcat(buf, "-", sizeof(buf));
	strlcat(buf, SSL_get_cipher(client->local->ssl), sizeof(buf));

	return buf;
}

/** Get the applicable ::tls-options block for this local client,
 * which may be defined in the link block, listen block, or set block.
 */
TLSOptions *get_tls_options_for_client(Client *client)
{
	if (!client->local)
		return NULL;
	if (client->server && client->server->conf && client->server->conf->tls_options)
		return client->server->conf->tls_options;
	if (client->local && client->local->listener && client->local->listener->tls_options)
		return client->local->listener->tls_options;
	return iConf.tls_options;
}

/** Outgoing TLS connect (read: handshake) to another server. */
void unreal_tls_client_handshake(int fd, int revents, void *data)
{
	Client *client = data;
	SSL_CTX *ctx = (client->server && client->server->conf && client->server->conf->ssl_ctx) ? client->server->conf->ssl_ctx : ctx_client;
	TLSOptions *tlsoptions = get_tls_options_for_client(client);

	if (!ctx)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_CREATE_SESSION_FAILED", NULL,
		           "Could not start TLS client handshake (no ctx?): TLS was possibly not loaded correctly on this server!?\n$tls_error.all",
		           log_data_tls_error());
		return;
	}

	client->local->ssl = SSL_new(ctx);
	if (!client->local->ssl)
	{
		unreal_log(ULOG_ERROR, "config", "TLS_CREATE_SESSION_FAILED", NULL,
		           "Could not start TLS client handshake: TLS was possibly not loaded correctly on this server!?\n$tls_error.all",
		           log_data_tls_error());
		return;
	}

	SSL_set_fd(client->local->ssl, client->local->fd);
	SSL_set_connect_state(client->local->ssl);
	SSL_set_nonblocking(client->local->ssl);

	if (tlsoptions->renegotiate_bytes > 0)
	{
		BIO_set_ssl_renegotiate_bytes(SSL_get_rbio(client->local->ssl), tlsoptions->renegotiate_bytes);
		BIO_set_ssl_renegotiate_bytes(SSL_get_wbio(client->local->ssl), tlsoptions->renegotiate_bytes);
	}

	if (tlsoptions->renegotiate_timeout > 0)
	{
		BIO_set_ssl_renegotiate_timeout(SSL_get_rbio(client->local->ssl), tlsoptions->renegotiate_timeout);
		BIO_set_ssl_renegotiate_timeout(SSL_get_wbio(client->local->ssl), tlsoptions->renegotiate_timeout);
	}

	if (client->server && client->server->conf)
	{
		/* Client: set hostname for SNI */
		SSL_set_tlsext_host_name(client->local->ssl, client->server->conf->servername);
	}

	SetTLS(client);

	switch (unreal_tls_connect(client, fd))
	{
		case -1:
			fd_close(fd);
			client->local->fd = -1;
			--OpenFiles;
			return;
		case 0: 
			SetTLSConnectHandshake(client);
			return;
		case 1:
			return;
		default:
			return;
	}

}

/** Called by I/O engine to (re)try accepting an TLS connection */
static void unreal_tls_accept_retry(int fd, int revents, void *data)
{
	Client *client = data;
	unreal_tls_accept(client, fd);
}

/** Accept an TLS connection - that is: do the TLS handshake */
int unreal_tls_accept(Client *client, int fd)
{
	int ssl_err;

#ifdef MSG_PEEK
	if (!IsNextCall(client))
	{
		char buf[1024];
		int n;
		
		n = recv(fd, buf, sizeof(buf), MSG_PEEK);
		if ((n >= 8) && !strncmp(buf, "STARTTLS", 8))
		{
			char buf[512];
			snprintf(buf, sizeof(buf),
				"ERROR :STARTTLS received but this is a TLS-only port. Check your connect settings. "
				"If this is a server linking in then add 'tls' in your link::outgoing::options block.\r\n");
			(void)send(fd, buf, strlen(buf), 0);
			return fatal_tls_error(SSL_ERROR_SSL, FUNC_TLS_ACCEPT, ERRNO, client);
		}
		if ((n >= 4) && (!strncmp(buf, "USER", 4) || !strncmp(buf, "NICK", 4) || !strncmp(buf, "PASS", 4) || !strncmp(buf, "CAP ", 4)))
		{
			char buf[512];
			snprintf(buf, sizeof(buf),
				"ERROR :NON-TLS command received on TLS-only port. Check your connection settings.\r\n");
			(void)send(fd, buf, strlen(buf), 0);
			return fatal_tls_error(SSL_ERROR_SSL, FUNC_TLS_ACCEPT, ERRNO, client);
		}
		if ((n >= 8) && (!strncmp(buf, "PROTOCTL", 8) || !strncmp(buf, "SERVER", 6)))
		{
			char buf[512];
			snprintf(buf, sizeof(buf),
				"ERROR :NON-TLS command received on TLS-only port. Check your connection settings.\r\n");
			(void)send(fd, buf, strlen(buf), 0);
			return fatal_tls_error(SSL_ERROR_SSL, FUNC_TLS_ACCEPT, ERRNO, client);
		}
		if (n > 0)
			SetNextCall(client);
	}
#endif
	if ((ssl_err = SSL_accept(client->local->ssl)) <= 0)
	{
		switch (ssl_err = SSL_get_error(client->local->ssl, ssl_err))
		{
			case SSL_ERROR_SYSCALL:
				if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
				{
					return 1;
				}
				return fatal_tls_error(ssl_err, FUNC_TLS_ACCEPT, ERRNO, client);
			case SSL_ERROR_WANT_READ:
				fd_setselect(fd, FD_SELECT_READ, unreal_tls_accept_retry, client);
				fd_setselect(fd, FD_SELECT_WRITE, NULL, client);
				return 1;
			case SSL_ERROR_WANT_WRITE:
				fd_setselect(fd, FD_SELECT_READ, NULL, client);
				fd_setselect(fd, FD_SELECT_WRITE, unreal_tls_accept_retry, client);
				return 1;
			default:
				return fatal_tls_error(ssl_err, FUNC_TLS_ACCEPT, ERRNO, client);
		}
		/* NOTREACHED */
		return -1;
	}

	client->local->listener->start_handshake(client);

	return 1;
}

/** Called by the I/O engine to (re)try to connect to a remote host */
static void unreal_tls_connect_retry(int fd, int revents, void *data)
{
	Client *client = data;
	unreal_tls_connect(client, fd);
}

/** Connect to a remote host - that is: connect and do the TLS handshake */
int unreal_tls_connect(Client *client, int fd)
{
	int ssl_err;

	if ((ssl_err = SSL_connect(client->local->ssl)) <= 0)
	{
		ssl_err = SSL_get_error(client->local->ssl, ssl_err);
		switch(ssl_err)
		{
			case SSL_ERROR_SYSCALL:
				if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
				{
					/* Hmmm. This implementation is different than in unreal_tls_accept().
					 * One of them must be wrong -- better check! (TODO)
					 */
					fd_setselect(fd, FD_SELECT_READ|FD_SELECT_WRITE, unreal_tls_connect_retry, client);
					return 0;
				}
				return fatal_tls_error(ssl_err, FUNC_TLS_CONNECT, ERRNO, client);
			case SSL_ERROR_WANT_READ:
				fd_setselect(fd, FD_SELECT_READ, unreal_tls_connect_retry, client);
				fd_setselect(fd, FD_SELECT_WRITE, NULL, client);
				return 0;
			case SSL_ERROR_WANT_WRITE:
				fd_setselect(fd, FD_SELECT_READ, NULL, client);
				fd_setselect(fd, FD_SELECT_WRITE, unreal_tls_connect_retry, client);
				return 0;
			default:
				return fatal_tls_error(ssl_err, FUNC_TLS_CONNECT, ERRNO, client);
		}
		/* NOTREACHED */
		return -1;
	}

	fd_setselect(fd, FD_SELECT_READ | FD_SELECT_WRITE, NULL, client);
	completed_connection(fd, FD_SELECT_READ | FD_SELECT_WRITE, client);

	return 1;
}

/** Shutdown a TLS connection (gracefully) */
int SSL_smart_shutdown(SSL *ssl)
{
	char i;
	int rc = 0;

	for(i = 0; i < 4; i++)
	{
		if ((rc = SSL_shutdown(ssl)))
			break;
	}
	return rc;
}

/**
 * Report a fatal TLS error and disconnect the associated client.
 *
 * @param ssl_error The error as from OpenSSL.
 * @param where The location, one of the SAFE_SSL_* defines.
 * @param my_errno A preserved value of errno to pass to ssl_error_str().
 * @param client The client the error is associated with.
 */
static int fatal_tls_error(int ssl_error, int where, int my_errno, Client *client)
{
	/* don`t alter ERRNO */
	int errtmp = ERRNO;
	const char *ssl_errstr, *ssl_func;
	unsigned long additional_errno = ERR_get_error();
	char additional_info[256];
	char buf[512];
	const char *one, *two;

	if (IsDeadSocket(client))
		return -1;

	switch(where)
	{
		case FUNC_TLS_READ:
			ssl_func = "SSL_read()";
			break;
		case FUNC_TLS_WRITE:
			ssl_func = "SSL_write()";
			break;
		case FUNC_TLS_ACCEPT:
			ssl_func = "SSL_accept()";
			break;
		case FUNC_TLS_CONNECT:
			ssl_func = "SSL_connect()";
			break;
		default:
			ssl_func = "undefined SSL func";
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	/* Fetch additional error information from OpenSSL 3.0.0+ */
	two = ERR_reason_error_string(additional_errno);
	if (two && *two)
	{
		snprintf(additional_info, sizeof(additional_info), ": %s", two);
	} else {
		*additional_info = '\0';
	}
#else
	/* Fetch additional error information from OpenSSL. This is new as of Nov 2017 (4.0.16+) */
	one = ERR_func_error_string(additional_errno);
	two = ERR_reason_error_string(additional_errno);
	if (one && *one && two && *two)
	{
		snprintf(additional_info, sizeof(additional_info), ": %s: %s", one, two);
	} else {
		*additional_info = '\0';
	}
#endif

	ssl_errstr = ssl_error_str(ssl_error, my_errno);

	SetDeadSocket(client);
	unreal_log(ULOG_DEBUG, "tls", "DEBUG_TLS_FATAL_ERROR", client,
		   "Exiting TLS client $client.details: $tls_function: $tls_error_string: $tls_additional_info",
		   log_data_string("tls_function", ssl_func),
		   log_data_string("tls_error_string", ssl_errstr),
		   log_data_string("tls_additional_info", additional_info));

	if (where == FUNC_TLS_CONNECT)
	{
		char extra[256];
		*extra = '\0';
		if (ssl_error == SSL_ERROR_SSL)
		{
			snprintf(extra, sizeof(extra),
			         ". Please verify that listen::options::ssl is enabled on port %d in %s's configuration file.",
			         (client->server && client->server->conf) ? client->server->conf->outgoing.port : -1,
			         client->name);
		}
		snprintf(buf, sizeof(buf), "%s: %s%s%s", ssl_func, ssl_errstr, additional_info, extra);
		lost_server_link(client, buf);
	} else
	if (IsServer(client) || (client->server && client->server->conf))
	{
		/* Either a trusted fully established server (incoming) or an outgoing server link (established or not) */
		snprintf(buf, sizeof(buf), "%s: %s%s", ssl_func, ssl_errstr, additional_info);
		lost_server_link(client, buf);
	}

	if (errtmp)
	{
		SET_ERRNO(errtmp);
		safe_strdup(client->local->error_str, strerror(errtmp));
	} else {
		SET_ERRNO(P_EIO);
		safe_strdup(client->local->error_str, ssl_errstr);
	}

	/* deregister I/O notification since we don't care anymore. the actual closing of socket will happen later. */
	if (client->local->fd >= 0)
		fd_unnotify(client->local->fd);

	return -1;
}

/** Do a TLS handshake after a STARTTLS, as a client */
int client_starttls(Client *client)
{
	if ((client->local->ssl = SSL_new(ctx_client)) == NULL)
		goto fail_starttls;

	SetTLS(client);

	SSL_set_fd(client->local->ssl, client->local->fd);
	SSL_set_nonblocking(client->local->ssl);

	if (client->server && client->server->conf)
	{
		/* Client: set hostname for SNI */
		SSL_set_tlsext_host_name(client->local->ssl, client->server->conf->servername);
	}

	if (unreal_tls_connect(client, client->local->fd) < 0)
	{
		SSL_set_shutdown(client->local->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown(client->local->ssl);
		SSL_free(client->local->ssl);
		goto fail_starttls;
	}

	/* HANDSHAKE IN PROGRESS */
	return 0;
fail_starttls:
	/* Failure */
	sendnumeric(client, ERR_STARTTLS, "STARTTLS failed");
	client->local->ssl = NULL;
	ClearTLS(client);
	SetUnknown(client);
	return 0; /* hm. we allow to continue anyway. not sure if we want that. */
}

/** Find the appropriate TLSOptions structure for a client.
 * NOTE: The default global TLS options will be returned if not found,
 *       or NULL if no such options are available (unlikely, but possible?).
 */
TLSOptions *FindTLSOptionsForUser(Client *client)
{
	ConfigItem_sni *sni;
	TLSOptions *sslopt = iConf.tls_options; /* default */
	
	if (!MyConnect(client) || !IsSecure(client))
		return NULL;

	/* Different sts-policy depending on SNI: */
	if (client->local->sni_servername)
	{
		sni = find_sni(client->local->sni_servername);
		if (sni)
		{
			sslopt = sni->tls_options;
		}
		/* It is perfectly possible that 'name' is not found and 'sni' is NULL,
		 * if a client used a hostname which we do not know about (eg: 'dummy').
		 */
	}

	return sslopt;
}

/** Verify certificate and make sure the certificate is valid for 'hostname'.
 * @param ssl: The SSL structure of the client or server
 * @param hostname: The hostname we should expect the certificate to be valid for
 * @param errstr: Error will be stored in here (optional)
 * @returns Returns 1 on success and 0 on error.
 */
int verify_certificate(SSL *ssl, const char *hostname, char **errstr)
{
	static char buf[512];
	X509 *cert;
	int n;

	*buf = '\0';

	if (errstr)
		*errstr = NULL; /* default */

	if (!ssl)
	{
		strlcpy(buf, "Not using TLS", sizeof(buf));
		if (errstr)
			*errstr = buf;
		return 0; /* Cannot verify a non-TLS connection */
	}

	if (SSL_get_verify_result(ssl) != X509_V_OK)
	{
		// FIXME: there are actually about 25+ different possible errors,
		// this is only the most common one:
		strlcpy(buf, "Certificate is not issued by a trusted Certificate Authority", sizeof(buf));
		if (errstr)
			*errstr = buf;
		return 0; /* Certificate verify failed */
	}

	/* Now verify if the name of the certificate matches hostname */
	cert = SSL_get_peer_certificate(ssl);

	if (!cert)
	{
		strlcpy(buf, "No certificate provided", sizeof(buf));
		if (errstr)
			*errstr = buf;
		return 0;
	}

#if 1
	n = validate_hostname(hostname, cert);
	X509_free(cert);
	if (n == MatchFound)
		return 1; /* Hostname matched. All tests passed. */
#else
	/* TODO: make autoconf test for X509_check_host() and verify that this code works:
	 * (When doing that, also disable the openssl_hostname_validation.c/.h code since
	 *  it would be unused)
	 */
	n = X509_check_host(cert, hostname, strlen(hostname), X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS, NULL);
	X509_free(cert);
	if (n == 1)
		return 1; /* Hostname matched. All tests passed. */
#endif

	/* Certificate is verified but is issued for a different hostname */
	snprintf(buf, sizeof(buf), "Certificate '%s' is not valid for hostname '%s'",
		certificate_name(ssl), hostname);
	if (errstr)
		*errstr = buf;
	return 0;
}

/** Grab the certificate name */
const char *certificate_name(SSL *ssl)
{
	static char buf[384];
	X509 *cert;
	X509_NAME *n;

	if (!ssl)
		return NULL;

	cert = SSL_get_peer_certificate(ssl);
	if (!cert)
		return NULL;

	n = X509_get_subject_name(cert);
	if (n)
	{
		buf[0] = '\0';
		X509_NAME_oneline(n, buf, sizeof(buf));
		X509_free(cert);
		return buf;
	} else {
		X509_free(cert);
		return NULL;
	}
}

/** Check if any weak ciphers are in use */
int cipher_check(SSL_CTX *ctx, char **errstr)
{
	SSL *ssl;
	static char errbuf[256];
	int i;
	const char *cipher;

	*errbuf = '\0'; // safety

	if (errstr)
		*errstr = errbuf;

	/* there isn't an SSL_CTX_get_cipher_list() unfortunately. */
	ssl = SSL_new(ctx);
	if (!ssl)
	{
		snprintf(errbuf, sizeof(errbuf), "Could not create TLS structure");
		return 0;
	}

	/* Very weak */
	i = 0;
	while ((cipher = SSL_get_cipher_list(ssl, i++)))
	{
		if (strstr(cipher, "DES-"))
		{
			snprintf(errbuf, sizeof(errbuf), "DES is enabled but is a weak cipher");
			SSL_free(ssl);
			return 0;
		}
		else if (strstr(cipher, "3DES-"))
		{
			snprintf(errbuf, sizeof(errbuf), "3DES is enabled but is a weak cipher");
			SSL_free(ssl);
			return 0;
		}
		else if (strstr(cipher, "RC4-"))
		{
			snprintf(errbuf, sizeof(errbuf), "RC4 is enabled but is a weak cipher");
			SSL_free(ssl);
			return 0;
		}
		else if (strstr(cipher, "NULL-"))
		{
			snprintf(errbuf, sizeof(errbuf), "NULL cipher provides no encryption");
			SSL_free(ssl);
			return 0;
		}
	}

	SSL_free(ssl);
	return 1;
}

/** Check if a certificate (or actually: key) is weak */
int certificate_quality_check(SSL_CTX *ctx, char **errstr)
{
#if OPENSSL_VERSION_NUMBER < 0x30000000L
	// FIXME: this only works on OpenSSL <3.0.0
	SSL *ssl;
	X509 *cert;
	EVP_PKEY *public_key;
	RSA *rsa_key;
	int key_length;
	static char errbuf[256];

	*errbuf = '\0'; // safety

	if (errstr)
		*errstr = errbuf;

	/* there isn't an SSL_CTX_get_cipher_list() unfortunately. */
	ssl = SSL_new(ctx);
	if (!ssl)
	{
		snprintf(errbuf, sizeof(errbuf), "Could not create TLS structure");
		return 0;
	}

	cert = SSL_get_certificate(ssl);
	if (!cert)
	{
		snprintf(errbuf, sizeof(errbuf), "Could not retrieve TLS certificate");
		SSL_free(ssl);
		return 0;
	}

	public_key = X509_get_pubkey(cert);
	if (!public_key)
	{
		/* Now this is unexpected.. */
		config_warn("certificate_quality_check(): could not check public key !? BUG?");
		SSL_free(ssl);
		return 1;
	}
	rsa_key = EVP_PKEY_get1_RSA(public_key);
	if (!rsa_key)
	{
		/* Not an RSA key, then we are done. */
		EVP_PKEY_free(public_key);
		SSL_free(ssl);
		return 1;
	}
	key_length = RSA_size(rsa_key) * 8;

	EVP_PKEY_free(public_key);
	RSA_free(rsa_key);
	SSL_free(ssl);

	if (key_length < 2048)
	{
		snprintf(errbuf, sizeof(errbuf), "Your TLS certificate key is only %d bits, which is insecure", key_length);
		return 0;
	}

#endif
	return 1;
}

const char *spki_fingerprint_ex(X509 *x509_cert);

/** Return the SPKI Fingerprint for a client.
 *
 * This is basically the same output as
 * openssl x509 -noout -in certificate.pem -pubkey | openssl asn1parse -noout -inform pem -out public.key
 * openssl dgst -sha256 -binary public.key | openssl enc -base64
 * ( from https://tools.ietf.org/html/draft-ietf-websec-key-pinning-21#appendix-A )
 */
const char *spki_fingerprint(Client *cptr)
{
	X509 *x509_cert = NULL;
	const char *ret;

	if (!MyConnect(cptr) || !cptr->local->ssl)
		return NULL;

	x509_cert = SSL_get_peer_certificate(cptr->local->ssl);
	if (!x509_cert)
		return NULL;
	ret = spki_fingerprint_ex(x509_cert);
	X509_free(x509_cert);
	return ret;
}

const char *spki_fingerprint_ex(X509 *x509_cert)
{
	unsigned char *der_cert = NULL, *p;
	int der_cert_len, n;
	static char retbuf[256];
	unsigned char checksum[SHA256_DIGEST_LENGTH];

	memset(retbuf, 0, sizeof(retbuf));

	der_cert_len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x509_cert), NULL);
	if ((der_cert_len > 0) && (der_cert_len < 16384))
	{
		der_cert = p = safe_alloc(der_cert_len);
		n = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x509_cert), &p);

		if ((n > 0) && ((p - der_cert) == der_cert_len))
		{
			/* The DER encoded SPKI is stored in 'der_cert' with length 'der_cert_len'.
			 * Now we need to create an SHA256 hash out of it.
			 */
			sha256hash_binary(checksum, der_cert, der_cert_len);

			/* And convert the binary to a base64 string... */
			n = b64_encode(checksum, SHA256_DIGEST_LENGTH, retbuf, sizeof(retbuf));
			safe_free(der_cert);
			return retbuf; /* SUCCESS */
		}
		safe_free(der_cert);
	}
	return NULL;
}

/** Returns 1 if the client is using an outdated protocol or cipher, 0 otherwise */
int outdated_tls_client(Client *client)
{
	TLSOptions *tlsoptions = get_tls_options_for_client(client);
	char buf[1024], *name, *p;
	const char *client_protocol = SSL_get_version(client->local->ssl);
	const char *client_ciphersuite = SSL_get_cipher(client->local->ssl);

	if (!tlsoptions)
		return 0; /* odd.. */

	strlcpy(buf, tlsoptions->outdated_protocols, sizeof(buf));
	for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
	{
		if (match_simple(name, client_protocol))
			 return 1; /* outdated protocol */
	}

	strlcpy(buf, tlsoptions->outdated_ciphers, sizeof(buf));
	for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
	{
		if (match_simple(name, client_ciphersuite))
			return 1; /* outdated cipher */
	}

	return 0; /* OK, not outdated */
}

/** Returns the expanded string used for set::outdated-tls-policy::user-message etc. */
const char *outdated_tls_client_build_string(const char *pattern, Client *client)
{
	static char buf[512];
	const char *name[3], *value[3];
	const char *str;

	str = SSL_get_version(client->local->ssl);
	name[0] = "protocol";
	value[0] = str ? str : "???";

	str = SSL_get_cipher(client->local->ssl);
	name[1] = "cipher";
	value[1] = str ? str : "???";

	name[2] = value[2] = NULL;

	buildvarstring(pattern, buf, sizeof(buf), name, value);
	return buf;
}

int check_certificate_expiry_ctx(SSL_CTX *ctx, char **errstr)
{
#if !defined(HAS_ASN1_TIME_diff) || !defined(HAS_X509_get0_notAfter)
	return 0;
#else
	static char errbuf[512];
	SSL *ssl;
	X509 *cert;
	const ASN1_TIME *cert_expiry_time;
	int days_expiry = 0, seconds_expiry = 0;
	long duration;

	*errstr = NULL;

	ssl = SSL_new(ctx);
	if (!ssl)
		return 0;

	cert = SSL_get_certificate(ssl);
	if (!cert)
	{
		SSL_free(ssl);
		return 0;
	}

	/* get certificate time */
	cert_expiry_time = X509_get0_notAfter(cert);

	/* calculate difference */
	ASN1_TIME_diff(&days_expiry, &seconds_expiry, cert_expiry_time, NULL);
	duration = (days_expiry * 86400) + seconds_expiry;

	/* certificate expiry? */
	if ((days_expiry > 0) || (seconds_expiry > 0))
	{
		snprintf(errbuf, sizeof(errbuf), "certificate expired %s ago", pretty_time_val(duration));
		SSL_free(ssl);
		*errstr = errbuf;
		return 1;
	} else
	/* or near-expiry? */
	if (((days_expiry < 0) || (seconds_expiry < 0)) && (days_expiry > -7))
	{
		snprintf(errbuf, sizeof(errbuf), "certificate will expire in %s", pretty_time_val(0 - duration));
		SSL_free(ssl);
		*errstr = errbuf;
		return 1;
	}

	/* All good */
	SSL_free(ssl);
	return 0;
#endif
}

void check_certificate_expiry_tlsoptions_and_warn(TLSOptions *tlsoptions)
{
	SSL_CTX *ctx;
	int ret;
	char *errstr = NULL;

	ctx = init_ctx(tlsoptions, 1);
	if (!ctx)
		return;

	if (check_certificate_expiry_ctx(ctx, &errstr))
	{
		unreal_log(ULOG_ERROR, "tls", "TLS_CERT_EXPIRING", NULL,
		           "Warning: TLS certificate '$filename': $error_string",
		           log_data_string("filename", tlsoptions->certificate_file),
		           log_data_string("error_string", errstr));
	}
	SSL_CTX_free(ctx);
}

EVENT(tls_check_expiry)
{
	ConfigItem_listen *listen;
	ConfigItem_sni *sni;
	ConfigItem_link *link;

	/* set block */
	check_certificate_expiry_tlsoptions_and_warn(iConf.tls_options);

	for (listen = conf_listen; listen; listen = listen->next)
		if (listen->tls_options)
			check_certificate_expiry_tlsoptions_and_warn(listen->tls_options);

	/* sni::tls-options.... */
	for (sni = conf_sni; sni; sni = sni->next)
		if (sni->tls_options)
			check_certificate_expiry_tlsoptions_and_warn(sni->tls_options);

	/* link::outgoing::tls-options.... */
	for (link = conf_link; link; link = link->next)
		if (link->tls_options)
			check_certificate_expiry_tlsoptions_and_warn(link->tls_options);
}
