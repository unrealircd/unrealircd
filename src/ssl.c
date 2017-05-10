/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/ssl.c
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
     switch(err) {
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
		strlcpy(buf, (char *)beforebuf, size);
		return (strlen(buf));
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
		strlcpy(buf, (char *)pass, size);
		strlcpy(beforebuf, (char *)pass, sizeof(beforebuf));
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
	if (preverify_ok)
		return 1;
	if (iConf.ssl_options->options & SSLFLAG_VERIFYCERT)
	{
		if (verify_err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
			if (!(iConf.ssl_options->options & SSLFLAG_DONOTACCEPTSELFSIGNED))
			{
				return 1;
			}
		return preverify_ok;
	}
	else
		return 1;
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

static void setup_dh_params(SSL_CTX *ctx)
{
	DH *dh;
	BIO *bio;
	char *dh_file = iConf.ssl_options ? iConf.ssl_options->dh_file : tempiConf.ssl_options->dh_file;
	/* ^^ because we can be called both before config file initalization or after */

	if (dh_file == NULL)
		return;

	bio = BIO_new_file(dh_file, "r");
	if (bio == NULL)
	{
		config_warn("Failed to load DH parameters %s", dh_file);
		config_report_ssl_error();
		return;
	}

	dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	if (dh == NULL)
	{
		config_warn("Failed to use DH parameters %s", dh_file);
		config_report_ssl_error();
		BIO_free(bio);
		return;
	}

	BIO_free(bio);
	SSL_CTX_set_tmp_dh(ctx, dh);
}

/** Disable SSL/TLS protocols as set by config */
void disable_ssl_protocols(SSL_CTX *ctx, SSLOptions *ssloptions)
{
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2); /* always disable SSLv2 */
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3); /* always disable SSLv3 */

#ifdef SSL_OP_NO_TLSv1
	if (!(ssloptions->protocols & SSL_PROTOCOL_TLSV1))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
#endif

#ifdef SSL_OP_NO_TLSv1_1
	if (!(ssloptions->protocols & SSL_PROTOCOL_TLSV1_1))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1);
#endif

#ifdef SSL_OP_NO_TLSv1_2
	if (!(ssloptions->protocols & SSL_PROTOCOL_TLSV1_2))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_2);
#endif

#ifdef SSL_OP_NO_TLSv1_3
	if (!(ssloptions->protocols & SSL_PROTOCOL_TLSV1_3))
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_3);
#endif
}

SSL_CTX *init_ctx(SSLOptions *ssloptions, int server)
{
	SSL_CTX *ctx;

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
	disable_ssl_protocols(ctx, ssloptions);
	SSL_CTX_set_default_passwd_cb(ctx, ssl_pem_passwd_cb);

	if (server && !(ssloptions->options & SSLFLAG_DISABLECLIENTCERT))
	{
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE | (ssloptions->options & SSLFLAG_FAILIFNOCERT ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0), ssl_verify_callback);
	}
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
#ifndef SSL_OP_NO_TICKET
 #error "Your system has an outdated OpenSSL version. Please upgrade OpenSSL."
#endif
	SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);

	setup_dh_params(ctx);

	if (!ssloptions->certificate_file)
	{
		config_warn("No SSL certificate configured (set::options::ssl::certificate or in a listen block)");
		config_report_ssl_error();
		goto fail;
	}

	if (SSL_CTX_use_certificate_chain_file(ctx, ssloptions->certificate_file) <= 0)
	{
		config_warn("Failed to load SSL certificate %s", ssloptions->certificate_file);
		config_report_ssl_error();
		goto fail;
	}

	if (!ssloptions->key_file)
	{
		config_warn("No SSL key configured (set::options::ssl::key or in a listen block)");
		config_report_ssl_error();
		goto fail;
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, ssloptions->key_file, SSL_FILETYPE_PEM) <= 0)
	{
		config_warn("Failed to load SSL private key %s", ssloptions->key_file);
		config_report_ssl_error();
		goto fail;
	}

	if (!SSL_CTX_check_private_key(ctx))
	{
		config_warn("Failed to check SSL private key");
		config_report_ssl_error();
		goto fail;
	}

	if (SSL_CTX_set_cipher_list(ctx, ssloptions->ciphers) == 0)
	{
		config_warn("Failed to set SSL cipher list for clients");
		config_report_ssl_error();
		goto fail;
	}

	if (server)
		SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

	if (ssloptions->trusted_ca_file)
	{
		if (!SSL_CTX_load_verify_locations(ctx, ssloptions->trusted_ca_file, NULL))
		{
			config_warn("Failed to load Trusted CA's from %s", ssloptions->trusted_ca_file);
			config_report_ssl_error();
			goto fail;
		}
	}

	if (server)
	{
#if defined(SSL_CTX_set_ecdh_auto)
		SSL_CTX_set_ecdh_auto(ctx, 1);
#else
		SSL_CTX_set_tmp_ecdh(ctx, EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
#endif
		SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE|SSL_OP_SINGLE_DH_USE);
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
	return 1;
}

int init_ssl(void)
{
	/* SSL preliminaries. We keep the certificate and key with the context. */
	ctx_server = init_ctx(iConf.ssl_options, 1);
	if (!ctx_server)
		return 0;
	ctx_client = init_ctx(iConf.ssl_options, 0);
	if (!ctx_client)
		return 0;
	return 1;
}

void reinit_ssl(aClient *acptr)
{
SSL_CTX *tmp;

	if (!acptr)
		mylog("Reloading all SSL related data (./unrealircd reloadtls)");
	else if (IsPerson(acptr))
		mylog("%s (%s@%s) requested a reload of all SSL related data (/rehash -ssl)",
			acptr->name, acptr->user->username, acptr->user->realhost);
	else
		mylog("%s requested a reload of all SSL related data (/rehash -ssl)",
			acptr->name);

	tmp = init_ctx(iConf.ssl_options, 1);
	if (!tmp)
	{
		config_error("SSL Reload failed.");
		config_report_ssl_error();
		return;
	}
	/* free and do it for real */
	SSL_CTX_free(tmp);
	SSL_CTX_free(ctx_server);
	ctx_server = init_ctx(iConf.ssl_options, 1);
	
	tmp = init_ctx(iConf.ssl_options, 0);
	if (!tmp)
	{
		config_error("SSL Reload partially failed. Server context is reloaded, client context failed");
		config_report_ssl_error();
		return;
	}
	/* free and do it for real */
	SSL_CTX_free(tmp);
	SSL_CTX_free(ctx_client);
	ctx_client = init_ctx(iConf.ssl_options, 0);
}

#define CHK_NULL(x) if ((x)==NULL) {\
        sendto_snomask(SNO_JUNK, "Lost connection to %s:Error in SSL", \
                     get_client_name(cptr, TRUE)); \
	return 0;\
	}

int  ssl_handshake(aClient *cptr)
{
#ifdef NO_CERTCHECKING
	char *str;
#endif

	if (!ctx_server)
	{
		sendto_realops("Could not start SSL handshake: SSL was not loaded correctly on this server (failed to load cert or key during boot process)");
		return -1;
	}

	cptr->local->ssl = SSL_new(ctx_server);
	CHK_NULL(cptr->local->ssl);
	SSL_set_fd(cptr->local->ssl, cptr->fd);
	set_non_blocking(cptr->fd, cptr);
	/* 
	 *  if necessary, SSL_write() will negotiate a TLS/SSL session, if not already explicitly
	 *  performed by SSL_connect() or SSL_accept(). If the peer requests a
	 *  re-negotiation, it will be performed transparently during the SSL_write() operation.
	 *    The behaviour of SSL_write() depends on the underlying BIO. 
	 *   
	 */
	if (!ircd_SSL_accept(cptr, cptr->fd)) {
		SSL_set_shutdown(cptr->local->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown(cptr->local->ssl);
		SSL_free(cptr->local->ssl);
		cptr->local->ssl = NULL;
		return -1;
	}
	return 0;

}

/* This is a bit homemade to fix IRCd's cleaning madness -- Stskeeps */
int	SSL_change_fd(SSL *s, int fd)
{
	BIO_set_fd(SSL_get_rbio(s), fd, BIO_NOCLOSE);
	BIO_set_fd(SSL_get_wbio(s), fd, BIO_NOCLOSE);
	return 1;
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
	c = SSL_get_current_cipher(ssl);
	SSL_CIPHER_get_bits(c, &bits);
	strlcat(buf, "-", sizeof(buf));
	strlcat(buf, my_itoa(bits), sizeof(buf));
	strlcat(buf, "bits", sizeof(buf));

	return buf;
}

void ircd_SSL_client_handshake(int fd, int revents, void *data)
{
	aClient *acptr = data;
	SSL_CTX *ctx = (acptr->serv && acptr->serv->conf && acptr->serv->conf->ssl_ctx) ? acptr->serv->conf->ssl_ctx : ctx_client;
	SSLOptions *ssloptions = (acptr->serv && acptr->serv->conf && acptr->serv->conf->ssl_options) ? acptr->serv->conf->ssl_options : iConf.ssl_options;

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

	if (ssloptions->renegotiate_bytes > 0)
	{
		BIO_set_ssl_renegotiate_bytes(SSL_get_rbio(acptr->local->ssl), ssloptions->renegotiate_bytes);
		BIO_set_ssl_renegotiate_bytes(SSL_get_wbio(acptr->local->ssl), ssloptions->renegotiate_bytes);
	}

	if (ssloptions->renegotiate_timeout > 0)
	{
		BIO_set_ssl_renegotiate_timeout(SSL_get_rbio(acptr->local->ssl), ssloptions->renegotiate_timeout);
		BIO_set_ssl_renegotiate_timeout(SSL_get_wbio(acptr->local->ssl), ssloptions->renegotiate_timeout);
	}

	if (acptr->serv && acptr->serv->conf->ciphers)
	{
		if (SSL_set_cipher_list(acptr->local->ssl, acptr->serv->conf->ciphers) == 0)
		{
			/* We abort */
			sendto_realops("Could not set ciphers for link %s (%s)",
				acptr->serv->conf->servername, 
				acptr->serv->conf->ciphers);
			return;
		}
	}

	acptr->flags |= FLAGS_SSL;

	switch (ircd_SSL_connect(acptr, fd))
	{
		case -1:
			fd_close(fd);
			acptr->fd = -1;
			--OpenFiles;
			return;
		case 0: 
			Debug((DEBUG_DEBUG, "SetSSLConnectHandshake(%s)", get_client_name(acptr, TRUE)));
			SetSSLConnectHandshake(acptr);
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

int ircd_SSL_accept(aClient *acptr, int fd) {

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
		if ((n >= 4) && (!strncmp(buf, "USER", 4) || !strncmp(buf, "NICK", 4) || !strncmp(buf, "PASS", 4)))
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
		switch(ssl_err = SSL_get_error(acptr->local->ssl, ssl_err))
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

int ircd_SSL_connect(aClient *acptr, int fd) {

    int ssl_err;
    if((ssl_err = SSL_connect(acptr->local->ssl)) <= 0)
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

int SSL_smart_shutdown(SSL *ssl) {
    char i;
    int rc;
    rc = 0;
    for(i = 0; i < 4; i++) {
	if((rc = SSL_shutdown(ssl)))
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

    if (IsDead(sptr))
    {
#ifdef DEBUGMODE
        /* This is quite possible I guess.. especially if we don't pay attention upstream :p */
        ircd_log(LOG_ERROR, "Warning: fatal_ssl_error() called for already-dead-socket (%d/%s)",
            sptr->fd, sptr->name);
#endif
        return -1;
    }

    switch(where) {
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

    ssl_errstr = ssl_error_str(ssl_error, my_errno);

    /* if we reply() something here, we might just trigger another
     * fatal_ssl_error() call and loop until a stack overflow... 
     * the client won`t get the ERROR : ... string, but this is
     * the only way to do it.
     * IRC protocol wasn`t SSL enabled .. --vejeta
     */
    sptr->flags |= FLAGS_DEADSOCKET;
    sendto_snomask(SNO_JUNK, "Exiting ssl client %s: %s: %s",
    	get_client_name(sptr, TRUE), ssl_func, ssl_errstr);
	
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
		sendto_umode(UMODE_OPER, "Lost connection to %s: %s: %s%s",
			get_client_name(sptr, FALSE), ssl_func, ssl_errstr, extra);
                /* This is a connect() that fails, we don't broadcast that for non-SSL either (noisy) */
	} else
	if ((IsServer(sptr) || (sptr->serv && sptr->serv->conf)) && (where != SAFE_SSL_WRITE))
	{
		/* if server (either judged by IsServer() or clearly an outgoing connect),
		 * and not writing (since otherwise deliver_it will take care of the error), THEN
		 * send a closing link error...
		 */
		sendto_umode_global(UMODE_OPER, "Lost connection to %s: %s: %d (%s)", get_client_name(sptr, FALSE), ssl_func, ssl_error, ssl_errstr);
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

	acptr->flags |= FLAGS_SSL;

	SSL_set_fd(acptr->local->ssl, acptr->fd);
	SSL_set_nonblocking(acptr->local->ssl);

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
	sendto_one(acptr, err_str(ERR_STARTTLS), me.name, !BadPtr(acptr->name) ? acptr->name : "*", "STARTTLS failed");
	acptr->local->ssl = NULL;
	acptr->flags &= ~FLAGS_SSL;
	SetUnknown(acptr);
	return 0; /* hm. we allow to continue anyway. not sure if we want that. */
}
