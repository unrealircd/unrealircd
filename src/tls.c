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

#include "unrealircd.h"
#include "openssl_hostname_validation.h"

#ifdef _WIN32
#define IDC_PASS                        1166
extern HINSTANCE hInst;
extern HWND hwIRCDWnd;
#endif

#define SAFE_SSL_READ 1
#define SAFE_SSL_WRITE 2
#define SAFE_SSL_ACCEPT 3
#define SAFE_SSL_CONNECT 4

extern void start_of_normal_client_handshake(aClient *acptr);
static int fatal_ssl_error(int ssl_error, int where, int my_errno, aClient *sptr);

/* The SSL structures */
SSL_CTX *ctx_server;
SSL_CTX *ctx_client;

char *SSLKeyPasswd;


typedef struct {
	int *size;
	char **buffer;
} StreamIO;

MODVAR int ssl_client_index = 0;

#define CHK_SSL(err) if ((err)==-1) { ERR_print_errors_fp(stderr); }
#ifdef _WIN32
LRESULT SSLPassDLG(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam) {
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

/**
 * Retrieve a static string for the given SSL error.
 *
 * \param err The error to look up.
 * \param my_errno The value of errno to use in case we want to call strerror().
 */
char *ssl_error_str(int err, int my_errno)
{
	static char ssl_errbuf[256];
	char *ssl_errstr = NULL;

	switch(err)
	{
		case SSL_ERROR_NONE:
			ssl_errstr = "SSL: No error";
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

/** Write official OpenSSL error string to ircd log / sendto_realops, using config_status.
 * Note that you are expected to announce earlier that you actually encountered an SSL error.
 * Also note that multiple error strings may be written out (with a slight chance of including
 * irrelevent ones[?]).
 */
void config_report_ssl_error()
{
unsigned long e;
char buf[512];

	do {
		e = ERR_get_error();
		if (e == 0)
			break; /* no (more) errors */
		ERR_error_string_n(e, buf, sizeof(buf));
		config_status(" %s", buf);
	} while(e);
}
				
int  ssl_pem_passwd_cb(char *buf, int size, int rwflag, void *password)
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
	pass = getpass("Password for SSL private key: ");
#else
	pass = passbuf;
	stream.buffer = &pass;
	stream.size = &passsize;
	DialogBoxParam(hInst, "SSLPass", hwIRCDWnd, (DLGPROC)SSLPassDLG, (LPARAM)&stream); 
#endif
	if (pass)
	{
		strlcpy(buf, pass, size);
		strlcpy(beforebuf, pass, sizeof(beforebuf));
		before = 1;
		SSLKeyPasswd = beforebuf;
		return (strlen(buf));
	}
	return 0;
}

static int ssl_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	int verify_err = 0;

	verify_err = X509_STORE_CTX_get_error(ctx);
	/* We accept the connection. Certificate verifiction takes
	 * place elsewhere, such as in _verify_link().
	 */
	return 1;
}

/** get aClient pointerd by SSL pointer */
aClient *get_client_by_ssl(SSL *ssl)
{
	return SSL_get_ex_data(ssl, ssl_client_index);
}

static void set_client_sni_name(SSL *ssl, char *name)
{
	aClient *acptr = get_client_by_ssl(ssl);
	if (acptr)
		safestrdup(acptr->local->sni_servername, name);
}

static int ssl_hostname_callback(SSL *ssl, int *unk, void *arg)
{
	char *name = (char *)SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	ConfigItem_sni *sni;

	if (name && (sni = Find_sni(name)))
	{
		SSL_set_SSL_CTX(ssl, sni->ssl_ctx);
		set_client_sni_name(ssl, name);
	}

	return SSL_TLSEXT_ERR_OK;
}

static void mylog(char *fmt, ...)
{
va_list vl;
static char buf[2048];

	va_start(vl, fmt);
	ircvsnprintf(buf, sizeof(buf), fmt, vl);
	va_end(vl);
	sendto_realops("[SSL rehash] %s", buf);
	ircd_log(LOG_ERROR, "%s", buf);
}

static int setup_dh_params(SSL_CTX *ctx)
{
	DH *dh;
	BIO *bio;
	char *dh_file = iConf.tls_options ? iConf.tls_options->dh_file : tempiConf.tls_options->dh_file;
	/* ^^ because we can be called both before config file initalization or after */

	if (dh_file == NULL)
		return 1;

	bio = BIO_new_file(dh_file, "r");
	if (bio == NULL)
	{
		config_error("Failed to load DH parameters %s", dh_file);
		config_report_ssl_error();
		return 0;
	}

	dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	if (dh == NULL)
	{
		config_error("Failed to use DH parameters %s", dh_file);
		config_report_ssl_error();
		BIO_free(bio);
		return 0;
	}

	BIO_free(bio);
	SSL_CTX_set_tmp_dh(ctx, dh);
	return 1;
}

/** Disable SSL/TLS protocols as set by config */
void disable_ssl_protocols(SSL_CTX *ctx, TLSOptions *tlsoptions)
{
	/* OpenSSL has two mechanisms for protocol version control:
	 *
	 * The old way, which is most flexible, is to use:
	 * SSL_CTX_set_options(... SSL_OP_NO_<version>) which allows
	 * you to disable each and every specific SSL/TLS version.
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
		config_error("Failed to do SSL CTX new");
		config_report_ssl_error();
		return NULL;
	}
	disable_ssl_protocols(ctx, tlsoptions);
	SSL_CTX_set_default_passwd_cb(ctx, ssl_pem_passwd_cb);

	if (server && !(tlsoptions->options & TLSFLAG_DISABLECLIENTCERT))
	{
		/* We tell OpenSSL/LibreSSL to verify the certificate and set our callback.
		 * Our callback will always accept the certificate since actual checking
		 * will take place elsewhere. Why? Because certificate is (often) delayed
		 * until after the SSL handshake. Such as in the case of link blocks where
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

	if (!setup_dh_params(ctx))
		goto fail;

	if (!tlsoptions->certificate_file)
	{
		config_error("No SSL certificate configured (set::options::ssl::certificate or in a listen block)");
		config_report_ssl_error();
		goto fail;
	}

	if (SSL_CTX_use_certificate_chain_file(ctx, tlsoptions->certificate_file) <= 0)
	{
		config_error("Failed to load SSL certificate %s", tlsoptions->certificate_file);
		config_report_ssl_error();
		goto fail;
	}

	if (!tlsoptions->key_file)
	{
		config_error("No SSL key configured (set::options::ssl::key or in a listen block)");
		config_report_ssl_error();
		goto fail;
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, tlsoptions->key_file, SSL_FILETYPE_PEM) <= 0)
	{
		config_error("Failed to load SSL private key %s", tlsoptions->key_file);
		config_report_ssl_error();
		goto fail;
	}

	if (!SSL_CTX_check_private_key(ctx))
	{
		config_error("Failed to check SSL private key");
		config_report_ssl_error();
		goto fail;
	}

	if (SSL_CTX_set_cipher_list(ctx, tlsoptions->ciphers) == 0)
	{
		config_error("Failed to set SSL cipher list");
		config_report_ssl_error();
		goto fail;
	}

#ifdef SSL_OP_NO_TLSv1_3
	if (SSL_CTX_set_ciphersuites(ctx, tlsoptions->ciphersuites) == 0)
	{
		config_error("Failed to set SSL ciphersuites list");
		config_report_ssl_error();
		goto fail;
	}
#endif

	if (!cipher_check(ctx, &errstr))
	{
		config_error("There is a problem with your SSL/TLS 'ciphers' configuration setting: %s", errstr);
		config_error("Remove the ciphers setting from your configuration file to use safer defaults, or change the cipher setting.");
		config_report_ssl_error();
		goto fail;
	}

	if (server)
		SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

	if (tlsoptions->trusted_ca_file)
	{
		if (!SSL_CTX_load_verify_locations(ctx, tlsoptions->trusted_ca_file, NULL))
		{
			config_error("Failed to load Trusted CA's from %s", tlsoptions->trusted_ca_file);
			config_report_ssl_error();
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
				config_error("Failed to apply ecdh-curves '%s'. "
				             "To get a list of supported curves with the "
				             "appropriate names, run "
				             "'openssl ecparam -list_curves' on the server. "
				             "Separate multiple curves by colon, "
				             "for example: ecdh-curves \"secp521r1:secp384r1\".",
				             tlsoptions->ecdh_curves);
				config_report_ssl_error();
				goto fail;
			}
#else
			/* We try to avoid this in the config code, but better have
			 * it here too than be sorry if someone screws up:
			 */
			config_error("ecdh-curves specified but not supported by library -- BAD!");
			config_report_ssl_error();
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

int early_init_ssl(void)
{
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	/* This is used to track (SSL *) <--> (aClient *) relationships: */
	ssl_client_index = SSL_get_ex_new_index(0, "ssl_client", NULL, NULL, NULL);
	return 1;
}

int init_ssl(void)
{
	/* SSL preliminaries. We keep the certificate and key with the context. */
	ctx_server = init_ctx(iConf.tls_options, 1);
	if (!ctx_server)
		return 0;
	ctx_client = init_ctx(iConf.tls_options, 0);
	if (!ctx_client)
		return 0;
	return 1;
}

void reinit_ssl(aClient *acptr)
{
	SSL_CTX *tmp;
	ConfigItem_listen *listen;
	ConfigItem_sni *sni;
	ConfigItem_link *link;

	if (!acptr)
		mylog("Reloading all SSL related data (./unrealircd reloadtls)");
	else if (IsPerson(acptr))
		mylog("%s (%s@%s) requested a reload of all SSL related data (/rehash -ssl)",
			acptr->name, acptr->user->username, acptr->user->realhost);
	else
		mylog("%s requested a reload of all SSL related data (/rehash -ssl)",
			acptr->name);

	tmp = init_ctx(iConf.tls_options, 1);
	if (!tmp)
	{
		config_error("SSL Reload failed.");
		config_report_ssl_error();
		return;
	}
	ctx_server = tmp; /* activate */
	
	tmp = init_ctx(iConf.tls_options, 0);
	if (!tmp)
	{
		config_error("SSL Reload partially failed. Server context is reloaded, client context failed");
		config_report_ssl_error();
		return;
	}
	ctx_client = tmp; /* activate */

	/* listen::ssl-options.... */
	for (listen = conf_listen; listen; listen = listen->next)
	{
		if (listen->tls_options)
		{
			tmp = init_ctx(listen->tls_options, 1);
			if (!tmp)
			{
				config_error("SSL Reload partially failed. listen::ssl-options error, see above");
				config_report_ssl_error();
				return;
			}
			listen->ssl_ctx = tmp; /* activate */
		}
	}

	/* sni::ssl-options.... */
	for (sni = conf_sni; sni; sni = sni->next)
	{
		if (sni->tls_options)
		{
			tmp = init_ctx(sni->tls_options, 1);
			if (!tmp)
			{
				config_error("SSL Reload partially failed. sni::ssl-options error, see above");
				config_report_ssl_error();
				return;
			}
			sni->ssl_ctx = tmp; /* activate */
		}
	}

	/* link::outgoing::ssl-options.... */
	for (link = conf_link; link; link = link->next)
	{
		if (link->tls_options)
		{
			tmp = init_ctx(link->tls_options, 1);
			if (!tmp)
			{
				config_error("SSL Reload partially failed. link::outgoing::ssl-options error in link %s { }, see above",
					link->servername);
				config_report_ssl_error();
				return;
			}
			link->ssl_ctx = tmp; /* activate */
		}
	}
}

void SSL_set_nonblocking(SSL *s)
{
	BIO_set_nbio(SSL_get_rbio(s),1);
	BIO_set_nbio(SSL_get_wbio(s),1);
}

char *ssl_get_cipher(SSL *ssl)
{
	static char buf[256];
	int bits;
	const SSL_CIPHER *c; 
	
	buf[0] = '\0';
	strlcpy(buf, SSL_get_version(ssl), sizeof(buf));
	strlcat(buf, "-", sizeof(buf));
	strlcat(buf, SSL_get_cipher(ssl), sizeof(buf));

	return buf;
}

/** Get the applicable ::ssl-options block for this local client,
 * which may be defined in the link block, listen block, or set block.
 */
TLSOptions *get_tls_options_for_client(aClient *acptr)
{
	if (!acptr->local)
		return NULL;
	if (acptr->serv && acptr->serv->conf && acptr->serv->conf->tls_options)
		return acptr->serv->conf->tls_options;
	if (acptr->local && acptr->local->listener && acptr->local->listener->tls_options)
		return acptr->local->listener->tls_options;
	return iConf.tls_options;
}

/** Outgoing SSL connect (read: handshake) to another server. */
void ircd_SSL_client_handshake(int fd, int revents, void *data)
{
	aClient *acptr = data;
	SSL_CTX *ctx = (acptr->serv && acptr->serv->conf && acptr->serv->conf->ssl_ctx) ? acptr->serv->conf->ssl_ctx : ctx_client;
	TLSOptions *tlsoptions = get_tls_options_for_client(acptr);

	if (!ctx)
	{
		sendto_realops("Could not start SSL client handshake: SSL was not loaded correctly on this server (failed to load cert or key)");
		return;
	}

	acptr->local->ssl = SSL_new(ctx);
	if (!acptr->local->ssl)
	{
		sendto_realops("Failed to SSL_new(ctx)");
		return;
	}

	SSL_set_fd(acptr->local->ssl, acptr->fd);
	SSL_set_connect_state(acptr->local->ssl);
	SSL_set_nonblocking(acptr->local->ssl);

	if (tlsoptions->renegotiate_bytes > 0)
	{
		BIO_set_ssl_renegotiate_bytes(SSL_get_rbio(acptr->local->ssl), tlsoptions->renegotiate_bytes);
		BIO_set_ssl_renegotiate_bytes(SSL_get_wbio(acptr->local->ssl), tlsoptions->renegotiate_bytes);
	}

	if (tlsoptions->renegotiate_timeout > 0)
	{
		BIO_set_ssl_renegotiate_timeout(SSL_get_rbio(acptr->local->ssl), tlsoptions->renegotiate_timeout);
		BIO_set_ssl_renegotiate_timeout(SSL_get_wbio(acptr->local->ssl), tlsoptions->renegotiate_timeout);
	}

	if (acptr->serv && acptr->serv->conf)
	{
		/* Client: set hostname for SNI */
		SSL_set_tlsext_host_name(acptr->local->ssl, acptr->serv->conf->servername);
	}

	acptr->flags |= FLAGS_TLS;

	switch (ircd_SSL_connect(acptr, fd))
	{
		case -1:
			fd_close(fd);
			acptr->fd = -1;
			--OpenFiles;
			return;
		case 0: 
			Debug((DEBUG_DEBUG, "SetTLSConnectHandshake(%s)", get_client_name(acptr, TRUE)));
			SetTLSConnectHandshake(acptr);
			return;
		case 1:
			Debug((DEBUG_DEBUG, "SSL_init_finished should finish this job (%s)", get_client_name(acptr, TRUE)));
			return;
		default:
			return;
	}

}

static void ircd_SSL_accept_retry(int fd, int revents, void *data)
{
	aClient *acptr = data;
	ircd_SSL_accept(acptr, fd);
}

int ircd_SSL_accept(aClient *acptr, int fd)
{
	int ssl_err;

#ifdef MSG_PEEK
	if (!(acptr->flags & FLAGS_NCALL))
	{
		char buf[1024];
		int n;
		
		n = recv(fd, buf, sizeof(buf), MSG_PEEK);
		if ((n >= 8) && !strncmp(buf, "STARTTLS", 8))
		{
			char buf[512];
			snprintf(buf, sizeof(buf),
				"ERROR :STARTTLS received but this is an SSL-only port. Check your connect settings. "
				"If this is a server linking in then add 'ssl' in your link::outgoing::options block.\r\n");
			(void)send(fd, buf, strlen(buf), 0);
			return fatal_ssl_error(SSL_ERROR_SSL, SAFE_SSL_ACCEPT, ERRNO, acptr);
		}
		if ((n >= 4) && (!strncmp(buf, "USER", 4) || !strncmp(buf, "NICK", 4) || !strncmp(buf, "PASS", 4) || !strncmp(buf, "CAP ", 4)))
		{
			char buf[512];
			snprintf(buf, sizeof(buf),
				"ERROR :NON-SSL command received on SSL-only port. Check your connection settings.\r\n");
			(void)send(fd, buf, strlen(buf), 0);
			return fatal_ssl_error(SSL_ERROR_SSL, SAFE_SSL_ACCEPT, ERRNO, acptr);
		}
		if ((n >= 8) && (!strncmp(buf, "PROTOCTL", 8) || !strncmp(buf, "SERVER", 6)))
		{
			char buf[512];
			snprintf(buf, sizeof(buf),
				"ERROR :NON-SSL command received on SSL-only port. Check your connection settings.\r\n");
			(void)send(fd, buf, strlen(buf), 0);
			return fatal_ssl_error(SSL_ERROR_SSL, SAFE_SSL_ACCEPT, ERRNO, acptr);
		}
		if (n > 0)
			acptr->flags |= FLAGS_NCALL;
	}
#endif
	if ((ssl_err = SSL_accept(acptr->local->ssl)) <= 0)
	{
		switch (ssl_err = SSL_get_error(acptr->local->ssl, ssl_err))
		{
			case SSL_ERROR_SYSCALL:
				if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
				{
					return 1;
				}
				return fatal_ssl_error(ssl_err, SAFE_SSL_ACCEPT, ERRNO, acptr);
			case SSL_ERROR_WANT_READ:
				fd_setselect(fd, FD_SELECT_READ, ircd_SSL_accept_retry, acptr);
				fd_setselect(fd, FD_SELECT_WRITE, NULL, acptr);
				return 1;
			case SSL_ERROR_WANT_WRITE:
				fd_setselect(fd, FD_SELECT_READ, NULL, acptr);
				fd_setselect(fd, FD_SELECT_WRITE, ircd_SSL_accept_retry, acptr);
				return 1;
			default:
				return fatal_ssl_error(ssl_err, SAFE_SSL_ACCEPT, ERRNO, acptr);
		}
		/* NOTREACHED */
		return -1;
	}

	start_of_normal_client_handshake(acptr);

	return 1;
}

static void ircd_SSL_connect_retry(int fd, int revents, void *data)
{
	aClient *acptr = data;
	ircd_SSL_connect(acptr, fd);
}

int ircd_SSL_connect(aClient *acptr, int fd)
{
	int ssl_err;

	if ((ssl_err = SSL_connect(acptr->local->ssl)) <= 0)
	{
		ssl_err = SSL_get_error(acptr->local->ssl, ssl_err);
		switch(ssl_err)
		{
			case SSL_ERROR_SYSCALL:
				if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
				{
					/* Hmmm. This implementation is different than in ircd_SSL_accept().
					 * One of them must be wrong -- better check! (TODO)
					 */
					fd_setselect(fd, FD_SELECT_READ|FD_SELECT_WRITE, ircd_SSL_connect_retry, acptr);
					return 0;
				}
				return fatal_ssl_error(ssl_err, SAFE_SSL_CONNECT, ERRNO, acptr);
			case SSL_ERROR_WANT_READ:
				fd_setselect(fd, FD_SELECT_READ, ircd_SSL_connect_retry, acptr);
				fd_setselect(fd, FD_SELECT_WRITE, NULL, acptr);
				return 0;
			case SSL_ERROR_WANT_WRITE:
				fd_setselect(fd, FD_SELECT_READ, NULL, acptr);
				fd_setselect(fd, FD_SELECT_WRITE, ircd_SSL_connect_retry, acptr);
				return 0;
			default:
				return fatal_ssl_error(ssl_err, SAFE_SSL_CONNECT, ERRNO, acptr);
		}
		/* NOTREACHED */
		return -1;
	}

	fd_setselect(fd, FD_SELECT_READ | FD_SELECT_WRITE, NULL, acptr);
	completed_connection(fd, FD_SELECT_READ | FD_SELECT_WRITE, acptr);

	return 1;
}

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
 * Report a fatal SSL error and disconnect the associated client.
 *
 * \param ssl_error The error as from OpenSSL.
 * \param where The location, one of the SAFE_SSL_* defines.
 * \param my_errno A preserved value of errno to pass to ssl_error_str().
 * \param sptr The client the error is associated with.
 */
static int fatal_ssl_error(int ssl_error, int where, int my_errno, aClient *sptr)
{
	/* don`t alter ERRNO */
	int errtmp = ERRNO;
	char *ssl_errstr, *ssl_func;
	unsigned long additional_errno = ERR_get_error();
	char additional_info[256];
	const char *one, *two;

	if (IsDead(sptr))
	{
#ifdef DEBUGMODE
		/* This is quite possible I guess.. especially if we don't pay attention upstream :p */
		ircd_log(LOG_ERROR, "Warning: fatal_ssl_error() called for already-dead-socket (%d/%s)",
			sptr->fd, sptr->name);
#endif
		return -1;
	}

	switch(where)
	{
		case SAFE_SSL_READ:
			ssl_func = "SSL_read()";
			break;
		case SAFE_SSL_WRITE:
			ssl_func = "SSL_write()";
			break;
		case SAFE_SSL_ACCEPT:
			ssl_func = "SSL_accept()";
			break;
		case SAFE_SSL_CONNECT:
			ssl_func = "SSL_connect()";
			break;
		default:
			ssl_func = "undefined SSL func";
	}

	/* Fetch additional error information from OpenSSL. This is new as of Nov 2017 (4.0.16+) */
	one = ERR_func_error_string(additional_errno);
	two = ERR_reason_error_string(additional_errno);
	if (one && *one && two && *two)
	{
		snprintf(additional_info, sizeof(additional_info), ": %s: %s", one, two);
	} else {
		*additional_info = '\0';
	}

	ssl_errstr = ssl_error_str(ssl_error, my_errno);

	/* if we reply() something here, we might just trigger another
	 * fatal_ssl_error() call and loop until a stack overflow... 
	 * the client won`t get the ERROR : ... string, but this is
	 * the only way to do it.
	 * IRC protocol wasn`t SSL enabled .. --vejeta
	 */
	sptr->flags |= FLAGS_DEADSOCKET;
	sendto_snomask(SNO_JUNK, "Exiting ssl client %s: %s: %s%s",
		get_client_name(sptr, TRUE), ssl_func, ssl_errstr, additional_info);

	if (where == SAFE_SSL_CONNECT)
	{
		char extra[256];
		*extra = '\0';
		if (ssl_error == SSL_ERROR_SSL)
		{
			snprintf(extra, sizeof(extra),
			         ". Please verify that listen::options::ssl is enabled on port %d in %s's configuration file.",
			         (sptr->serv && sptr->serv->conf) ? sptr->serv->conf->outgoing.port : -1,
			         sptr->name);
		}
		sendto_umode(UMODE_OPER, "Lost connection to %s: %s: %s%s%s",
			get_client_name(sptr, FALSE), ssl_func, ssl_errstr, additional_info, extra);
		/* This is a connect() that fails, we don't broadcast that for non-SSL either (noisy) */
	} else
	if ((IsServer(sptr) || (sptr->serv && sptr->serv->conf)) && (where != SAFE_SSL_WRITE))
	{
		/* if server (either judged by IsServer() or clearly an outgoing connect),
		 * and not writing (since otherwise deliver_it will take care of the error), THEN
		 * send a closing link error...
		 */
		sendto_umode_global(UMODE_OPER, "Lost connection to %s: %s: %d (%s%s)",
			get_client_name(sptr, FALSE), ssl_func, ssl_error, ssl_errstr, additional_info);
	}

	if (errtmp)
	{
		SET_ERRNO(errtmp);
		sptr->local->error_str = strdup(strerror(errtmp));
	} else {
		SET_ERRNO(P_EIO);
		sptr->local->error_str = strdup(ssl_errstr);
	}

	/* deregister I/O notification since we don't care anymore. the actual closing of socket will happen later. */
	if (sptr->fd >= 0)
		fd_unnotify(sptr->fd);

	return -1;
}

int client_starttls(aClient *acptr)
{
	if ((acptr->local->ssl = SSL_new(ctx_client)) == NULL)
		goto fail_starttls;

	acptr->flags |= FLAGS_TLS;

	SSL_set_fd(acptr->local->ssl, acptr->fd);
	SSL_set_nonblocking(acptr->local->ssl);

	if (acptr->serv && acptr->serv->conf)
	{
		/* Client: set hostname for SNI */
		SSL_set_tlsext_host_name(acptr->local->ssl, acptr->serv->conf->servername);
	}

	if (ircd_SSL_connect(acptr, acptr->fd) < 0)
	{
		Debug((DEBUG_DEBUG, "Failed SSL connect handshake in instance 1: %s", acptr->name));
		SSL_set_shutdown(acptr->local->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown(acptr->local->ssl);
		SSL_free(acptr->local->ssl);
		goto fail_starttls;
	}

	/* HANDSHAKE IN PROGRESS */
	return 0;
fail_starttls:
	/* Failure */
	sendnumeric(acptr, ERR_STARTTLS, "STARTTLS failed");
	acptr->local->ssl = NULL;
	acptr->flags &= ~FLAGS_TLS;
	SetUnknown(acptr);
	return 0; /* hm. we allow to continue anyway. not sure if we want that. */
}

/** Find the appropriate TLSOptions structure for a client.
 * NOTE: The default global SSL options will be returned if not found,
 *       or NULL if no such options are available (unlikely, but possible?).
 */
TLSOptions *FindTLSOptionsForUser(aClient *acptr)
{
	ConfigItem_sni *sni;
	TLSOptions *sslopt = iConf.tls_options; /* default */
	
	if (!MyConnect(acptr) || !IsSecure(acptr))
		return NULL;

	/* Different sts-policy depending on SNI: */
	if (acptr->local->sni_servername)
	{
		sni = Find_sni(acptr->local->sni_servername);
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
int verify_certificate(SSL *ssl, char *hostname, char **errstr)
{
	static char buf[512];
	X509 *cert;
	int n;

	*buf = '\0';

	if (errstr)
		*errstr = NULL; /* default */

	if (!ssl)
	{
		strlcpy(buf, "Not using SSL/TLS", sizeof(buf));
		if (errstr)
			*errstr = buf;
		return 0; /* Cannot verify a non-SSL connection */
	}

	if (SSL_get_verify_result(ssl) != X509_V_OK)
	{
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
char *certificate_name(SSL *ssl)
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
		snprintf(errbuf, sizeof(errbuf), "Could not create SSL structure");
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

/** Return the SPKI Fingerprint for a client.
 *
 * This is basically the same output as
 * openssl x509 -noout -in certificate.pem -pubkey | openssl asn1parse -noout -inform pem -out public.key
 * openssl dgst -sha256 -binary public.key | openssl enc -base64
 * ( from https://tools.ietf.org/html/draft-ietf-websec-key-pinning-21#appendix-A )
 */
char *spki_fingerprint(aClient *cptr)
{
	X509 *x509_cert = NULL;
	unsigned char *der_cert = NULL, *p;
	int der_cert_len, n;
	static char retbuf[256];
	SHA256_CTX ckctx;
	unsigned char checksum[SHA256_DIGEST_LENGTH];

	if (!MyConnect(cptr) || !cptr->local->ssl)
		return NULL;

	x509_cert = SSL_get_peer_certificate(cptr->local->ssl);

	if (x509_cert)
	{
		memset(retbuf, 0, sizeof(retbuf));

		der_cert_len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x509_cert), NULL);
		if ((der_cert_len > 0) && (der_cert_len < 16384))
		{
			der_cert = p = MyMallocEx(der_cert_len);
			n = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x509_cert), &p);

			if ((n > 0) && ((p - der_cert) == der_cert_len))
			{
				/* The DER encoded SPKI is stored in 'der_cert' with length 'der_cert_len'.
				 * Now we need to create an SHA256 hash out of it.
				 */
				SHA256_Init(&ckctx);
				SHA256_Update(&ckctx, der_cert, der_cert_len);
				SHA256_Final(checksum, &ckctx);

				/* And convert the binary to a base64 string... */
				n = b64_encode(checksum, SHA256_DIGEST_LENGTH, retbuf, sizeof(retbuf));
				MyFree(der_cert);
				X509_free(x509_cert);
				return retbuf; /* SUCCESS */
			}
			MyFree(der_cert);
		}
		X509_free(x509_cert);
	}
	return NULL;
}

/** Returns 1 if the client is using an outdated protocol or cipher, 0 otherwise */
int outdated_tls_client(aClient *acptr)
{
	TLSOptions *tlsoptions = get_tls_options_for_client(acptr);
	char buf[1024], *name, *p;
	const char *client_protocol = SSL_get_version(acptr->local->ssl);
	const char *client_ciphersuite = SSL_get_cipher(acptr->local->ssl);
	int bad = 0;

	if (!tlsoptions)
		return 0; /* odd.. */

	strlcpy(buf, tlsoptions->outdated_protocols, sizeof(buf));
	for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
	{
		if (!_match(name, client_protocol))
			 return 1; /* outdated protocol */
	}

	strlcpy(buf, tlsoptions->outdated_ciphers, sizeof(buf));
	for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
	{
		if (!_match(name, client_ciphersuite))
			return 1; /* outdated cipher */
	}

	return 0; /* OK, not outdated */
}

char *outdated_tls_client_build_string(char *pattern, aClient *acptr)
{
	static char buf[512];
	const char *name[3], *value[3];
	const char *str;

	str = SSL_get_version(acptr->local->ssl);
	name[0] = "protocol";
	value[0] = str ? str : "???";

	str = SSL_get_cipher(acptr->local->ssl);
	name[1] = "cipher";
	value[1] = str ? str : "???";

	name[2] = value[2] = NULL;

	buildvarstring(pattern, buf, sizeof(buf), name, value);
	return buf;
}
