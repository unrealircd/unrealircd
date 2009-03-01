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

#include "config.h"
#ifdef USE_SSL
#include "struct.h"
#include "common.h"
#include "h.h"
#include "proto.h"
#include "sys.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>

#define IDC_PASS                        1166
extern HINSTANCE hInst;
extern HWND hwIRCDWnd;
#endif

#define SAFE_SSL_READ 1
#define SAFE_SSL_WRITE 2
#define SAFE_SSL_ACCEPT 3
#define SAFE_SSL_CONNECT 4

static int fatal_ssl_error(int ssl_error, int where, aClient *sptr);

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

char *ssl_error_str(int err)
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
	    ssl_errstr = "OpenSSL requested a X509 lookup which didn`t arrive";
	    break;
	case SSL_ERROR_SYSCALL:
		snprintf(ssl_errbuf, sizeof(ssl_errbuf), "Underlying syscall error [%s]", STRERROR(ERRNO));
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
		strncpyzt(buf, (char *)beforebuf, size);
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
		strncpyzt(buf, (char *)pass, size);
		strncpyzt(beforebuf, (char *)pass, sizeof(beforebuf));
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
	if (iConf.ssl_options & SSLFLAG_VERIFYCERT)
	{
		if (verify_err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
			if (!(iConf.ssl_options & SSLFLAG_DONOTACCEPTSELFSIGNED))
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
	ircvsprintf(buf, fmt, vl);
	va_end(vl);
	sendto_realops("[SSL rehash] %s", buf);
	ircd_log(LOG_ERROR, "%s", buf);
}

SSL_CTX *init_ctx_server(void)
{
SSL_CTX *ctx_server;

	ctx_server = SSL_CTX_new(SSLv23_server_method());
	if (!ctx_server)
	{
		mylog("Failed to do SSL CTX new");
		return NULL;
	}
	SSL_CTX_set_default_passwd_cb(ctx_server, ssl_pem_passwd_cb);
	SSL_CTX_set_options(ctx_server, SSL_OP_NO_SSLv2);
	SSL_CTX_set_verify(ctx_server, SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE
			| (iConf.ssl_options & SSLFLAG_FAILIFNOCERT ? SSL_VERIFY_FAIL_IF_NO_PEER_CERT : 0), ssl_verify_callback);
	SSL_CTX_set_session_cache_mode(ctx_server, SSL_SESS_CACHE_OFF);

	if (SSL_CTX_use_certificate_chain_file(ctx_server, SSL_SERVER_CERT_PEM) <= 0)
	{
		mylog("Failed to load SSL certificate %s", SSL_SERVER_CERT_PEM);
		goto fail;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx_server, SSL_SERVER_KEY_PEM, SSL_FILETYPE_PEM) <= 0)
	{
		mylog("Failed to load SSL private key %s", SSL_SERVER_KEY_PEM);
		goto fail;
	}

	if (!SSL_CTX_check_private_key(ctx_server))
	{
		mylog("Failed to check SSL private key");
		goto fail;
	}
	if (iConf.x_server_cipher_list)
	{
                if (SSL_CTX_set_cipher_list(ctx_server, iConf.x_server_cipher_list) == 0)
                {
                    mylog("Failed to set SSL cipher list for clients");
                    goto fail;
                }
	}
	if (iConf.trusted_ca_file)
	{
		if (!SSL_CTX_load_verify_locations(ctx_server, iConf.trusted_ca_file, NULL))
		{
			mylog("Failed to load Trusted CA's from %s", iConf.trusted_ca_file);
			goto fail;
		}
	}
	return ctx_server;
fail:
	SSL_CTX_free(ctx_server);
	return NULL;
}


SSL_CTX *init_ctx_client(void)
{
SSL_CTX *ctx_client;

	ctx_client = SSL_CTX_new(SSLv3_client_method());
	if (!ctx_client)
	{
		mylog("Failed to do SSL CTX new client");
		return NULL;
	}
	SSL_CTX_set_default_passwd_cb(ctx_client, ssl_pem_passwd_cb);
	SSL_CTX_set_session_cache_mode(ctx_client, SSL_SESS_CACHE_OFF);
	if (SSL_CTX_use_certificate_file(ctx_client, SSL_SERVER_CERT_PEM, SSL_FILETYPE_PEM) <= 0)
	{
		mylog("Failed to load SSL certificate %s (client)", SSL_SERVER_CERT_PEM);
		goto fail;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx_client, SSL_SERVER_KEY_PEM, SSL_FILETYPE_PEM) <= 0)
	{
		mylog("Failed to load SSL private key %s (client)", SSL_SERVER_KEY_PEM);
		goto fail;
	}

	if (!SSL_CTX_check_private_key(ctx_client))
	{
		mylog("Failed to check SSL private key (client)");
		goto fail;
	}
	return ctx_client;
fail:
	SSL_CTX_free(ctx_client);
	return NULL;
}

void init_ssl(void)
{
	/* SSL preliminaries. We keep the certificate and key with the context. */

	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();
	if (USE_EGD) {
#if OPENSSL_VERSION_NUMBER >= 0x000907000
		if (!EGD_PATH)
			RAND_status();
		else

#else
		if (EGD_PATH) 
#endif
			RAND_egd(EGD_PATH);		
	}
	ctx_server = init_ctx_server();
	if (!ctx_server)
		exit(7);
	ctx_client = init_ctx_client();
	if (!ctx_client)
		exit(8);
}

void reinit_ssl(aClient *acptr)
{
SSL_CTX *tmp;

	if (IsPerson(acptr))
		mylog("%s (%s@%s) requested a reload of all SSL related data (/rehash -ssl)",
			acptr->name, acptr->user->username, acptr->user->realhost);
	else
		mylog("%s requested a reload of all SSL related data (/rehash -ssl)",
			acptr->name);

	tmp = init_ctx_server();
	if (!tmp)
	{
		mylog("SSL Reload failed.");
		return;
	}
	/* free and do it for real */
	SSL_CTX_free(tmp);
	SSL_CTX_free(ctx_server);
	ctx_server = init_ctx_server();
	
	tmp = init_ctx_client();
	if (!tmp)
	{
		mylog("SSL Reload partially failed. Server context is reloaded, client context failed");
		return;
	}
	/* free and do it for real */
	SSL_CTX_free(tmp);
	SSL_CTX_free(ctx_client);
	ctx_client = init_ctx_client();
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

	cptr->ssl = SSL_new(ctx_server);
	CHK_NULL(cptr->ssl);
	SSL_set_fd((SSL *) cptr->ssl, cptr->fd);
	set_non_blocking(cptr->fd, cptr);
	/* 
	 *  if necessary, SSL_write() will negotiate a TLS/SSL session, if not already explicitly
	 *  performed by SSL_connect() or SSL_accept(). If the peer requests a
	 *  re-negotiation, it will be performed transparently during the SSL_write() operation.
	 *    The behaviour of SSL_write() depends on the underlying BIO. 
	 *   
	 */
	if (!ircd_SSL_accept(cptr, cptr->fd)) {
		SSL_set_shutdown((SSL *)cptr->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown((SSL *)cptr->ssl);
		SSL_free((SSL *)cptr->ssl);
		cptr->ssl = NULL;
		return -1;
	}
	return 0;

}
/* 
   ssl_client_handshake
        This will initiate a client SSL_connect
        
        -Stskeeps 
   
   Return values:
      -1  = Could not SSL_new
      -2  = Error doing SSL_connect
      -3  = Try again 
*/
int  ssl_client_handshake(aClient *cptr, ConfigItem_link *l)
{
	cptr->ssl = SSL_new((SSL_CTX *)ctx_client);
	if (!cptr->ssl)
	{
		sendto_realops("Couldn't SSL_new(ctx_client) on %s",
			get_client_name(cptr, FALSE));
		return -1;
	}
/*	set_blocking(cptr->fd); */
	SSL_set_fd((SSL *)cptr->ssl, cptr->fd);
	SSL_set_connect_state((SSL *)cptr->ssl);
	if (l && l->ciphers)
	{
		if (SSL_set_cipher_list((SSL *)cptr->ssl, 
			l->ciphers) == 0)
		{
			/* We abort */
			sendto_realops("SSL cipher selecting for %s was unsuccesful (%s)",
				l->servername, l->ciphers);
			return -2;
		}
	}
	if (SSL_connect((SSL *)cptr->ssl) <= 0)
	{
#if 0
		sendto_realops("Couldn't SSL_connect");
		return -2;
#endif
	}
	set_non_blocking(cptr->fd, cptr);
	cptr->flags |= FLAGS_SSL;
	return 1;
}

/* This is a bit homemade to fix IRCd's cleaning madness -- Stskeeps */
int	SSL_change_fd(SSL *s, int fd)
{
	BIO_set_fd(SSL_get_rbio(s), fd, BIO_NOCLOSE);
	BIO_set_fd(SSL_get_wbio(s), fd, BIO_NOCLOSE);
	return 1;
}

void	SSL_set_nonblocking(SSL *s)
{
	BIO_set_nbio(SSL_get_rbio(s),1);  
	BIO_set_nbio(SSL_get_wbio(s),1);  
}

char	*ssl_get_cipher(SSL *ssl)
{
	static char buf[400];
	int bits;
	SSL_CIPHER *c; 
	
	buf[0] = '\0';
	strcpy(buf, SSL_get_version(ssl));
	strcat(buf, "-");
	strcat(buf, SSL_get_cipher(ssl));
	c = SSL_get_current_cipher(ssl);
	SSL_CIPHER_get_bits(c, &bits);
	strcat(buf, "-");
	strcat(buf, (char *)my_itoa(bits));
	strcat(buf, "bits");
	return (buf);
}

int ircd_SSL_read(aClient *acptr, void *buf, int sz)
{
    int len, ssl_err;
    len = SSL_read((SSL *)acptr->ssl, buf, sz);
    if (len <= 0)
    {
       switch(ssl_err = SSL_get_error((SSL *)acptr->ssl, len)) {
           case SSL_ERROR_SYSCALL:
               if (ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN ||
                       ERRNO == P_EINTR) {
           case SSL_ERROR_WANT_READ:
                   SET_ERRNO(P_EWOULDBLOCK);
		   Debug((DEBUG_ERROR, "ircd_SSL_read: returning EWOULDBLOCK and 0 for %s - %s", acptr->name,
			ssl_err == SSL_ERROR_WANT_READ ? "SSL_ERROR_WANT_READ" : "SSL_ERROR_SYSCALL"		   
		   ));
                   return -1;
               }
           case SSL_ERROR_SSL:
               if(ERRNO == EAGAIN)
                   return -1;
           default:
           	Debug((DEBUG_ERROR, "ircd_SSL_read: returning fatal_ssl_error for %s",
           	 acptr->name));
		return fatal_ssl_error(ssl_err, SAFE_SSL_READ, acptr);        
       }
    }
    Debug((DEBUG_ERROR, "ircd_SSL_read for %s (%p, %i): success", acptr->name, buf, sz));
    return len;
}
int ircd_SSL_write(aClient *acptr, const void *buf, int sz)
{
    int len, ssl_err;

    len = SSL_write((SSL *)acptr->ssl, buf, sz);
    if (len <= 0)
    {
       switch(ssl_err = SSL_get_error((SSL *)acptr->ssl, len)) {
           case SSL_ERROR_SYSCALL:
               if (ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN ||
                       ERRNO == P_EINTR)
		{
			SET_ERRNO(P_EWOULDBLOCK);
			return -1;
		}
		return -1;
          case SSL_ERROR_WANT_WRITE:
                   SET_ERRNO(P_EWOULDBLOCK);
                   return -1;
           case SSL_ERROR_SSL:
               if(ERRNO == EAGAIN)
                   return -1;
           default:
		Debug((DEBUG_ERROR, "ircd_SSL_write: returning fatal_ssl_error for %s", acptr->name));
		return fatal_ssl_error(ssl_err, SAFE_SSL_WRITE, acptr);
       }
    }
    Debug((DEBUG_ERROR, "ircd_SSL_write for %s (%p, %i): success", acptr->name, buf, sz));
    return len;
}

int ircd_SSL_client_handshake(aClient *acptr)
{
	acptr->ssl = SSL_new(ctx_client);
	if (!acptr->ssl)
	{
		sendto_realops("Failed to SSL_new(ctx_client)");
		return FALSE;
	}
	SSL_set_fd(acptr->ssl, acptr->fd);
	SSL_set_connect_state(acptr->ssl);
	SSL_set_nonblocking(acptr->ssl);
        if (iConf.ssl_renegotiate_bytes > 0)
	{
          BIO_set_ssl_renegotiate_bytes(SSL_get_rbio(acptr->ssl), iConf.ssl_renegotiate_bytes);
          BIO_set_ssl_renegotiate_bytes(SSL_get_wbio(acptr->ssl), iConf.ssl_renegotiate_bytes);
        }
        if (iConf.ssl_renegotiate_timeout > 0)
        {
          BIO_set_ssl_renegotiate_timeout(SSL_get_rbio(acptr->ssl), iConf.ssl_renegotiate_timeout);
          BIO_set_ssl_renegotiate_timeout(SSL_get_wbio(acptr->ssl), iConf.ssl_renegotiate_timeout);
        }

	if (acptr->serv && acptr->serv->conf->ciphers)
	{
		if (SSL_set_cipher_list((SSL *)acptr->ssl, 
			acptr->serv->conf->ciphers) == 0)
		{
			/* We abort */
			sendto_realops("SSL cipher selecting for %s was unsuccesful (%s)",
				acptr->serv->conf->servername, 
				acptr->serv->conf->ciphers);
			return -2;
		}
	}
	acptr->flags |= FLAGS_SSL;
	switch (ircd_SSL_connect(acptr))
	{
		case -1: 
			return -1;
		case 0: 
			Debug((DEBUG_DEBUG, "SetSSLConnectHandshake(%s)", get_client_name(acptr, TRUE)));
			SetSSLConnectHandshake(acptr);
			return 0;
		case 1: 
			Debug((DEBUG_DEBUG, "SSL_init_finished should finish this job (%s)", get_client_name(acptr, TRUE)));
			/* SSL_init_finished in s_bsd will finish the job */
			return 1;
		default:
			return -1;		
	}

}

int ircd_SSL_accept(aClient *acptr, int fd) {

    int ssl_err;

    if((ssl_err = SSL_accept((SSL *)acptr->ssl)) <= 0) {
	switch(ssl_err = SSL_get_error((SSL *)acptr->ssl, ssl_err)) {
	    case SSL_ERROR_SYSCALL:
		if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK
			|| ERRNO == P_EAGAIN)
	    case SSL_ERROR_WANT_READ:
	    case SSL_ERROR_WANT_WRITE:
		    Debug((DEBUG_DEBUG, "ircd_SSL_accept(%s), - %s", get_client_name(acptr, TRUE), ssl_error_str(ssl_err)));
		    /* handshake will be completed later . . */
		    return 1;
	    default:
		return fatal_ssl_error(ssl_err, SAFE_SSL_ACCEPT, acptr);
		
	}
	/* NOTREACHED */
	return -1;
    }
    return 1;
}

int ircd_SSL_connect(aClient *acptr) {

    int ssl_err;
    if((ssl_err = SSL_connect((SSL *)acptr->ssl)) <= 0) {
	switch(ssl_err = SSL_get_error((SSL *)acptr->ssl, ssl_err)) {
	    case SSL_ERROR_SYSCALL:
		if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK
			|| ERRNO == P_EAGAIN)
	    case SSL_ERROR_WANT_READ:
	    case SSL_ERROR_WANT_WRITE:
	    	 {
		    Debug((DEBUG_DEBUG, "ircd_SSL_connect(%s), - %s", get_client_name(acptr, TRUE), ssl_error_str(ssl_err)));
		    /* handshake will be completed later . . */
		    return 0;
	         }
	    default:
		return fatal_ssl_error(ssl_err, SAFE_SSL_CONNECT, acptr);
		
	}
	/* NOTREACHED */
	return -1;
    }
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

static int fatal_ssl_error(int ssl_error, int where, aClient *sptr)
{
    /* don`t alter ERRNO */
    int errtmp = ERRNO;
    char *ssl_errstr, *ssl_func;

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

    switch(ssl_error) {
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
	    ssl_errstr = "OpenSSL requested a X509 lookup which didn`t arrive";
	    break;
	case SSL_ERROR_SYSCALL:
	    ssl_errstr = "Underlying syscall error";
	    /* TODO: then show the REAL socktet error instead...
	     * unfortunately that means we need to have the 'untouched errno',
	     * which is not always present since our function is not always
	     * called directly after a failure. Hence, we should add a new
	     * parameter to fatal_ssl_error which is called errno, and use
	     * that here... Something for 3.2.7 ;) -- Syzop
	     */
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
		char *myerr = ssl_errstr;
		if (ssl_error == SSL_ERROR_SYSCALL)
			myerr = STRERROR(errtmp);
		/* sendto_failops_whoare_opers("Closing link: SSL_connect(): %s - %s", myerr, get_client_name(sptr, FALSE)); */
		sendto_umode(UMODE_OPER, "Lost connection to %s: %s: %s",
			get_client_name(sptr, FALSE), ssl_func, myerr);
	} else
	if ((IsServer(sptr) || (sptr->serv && sptr->serv->conf)) && (where != SAFE_SSL_WRITE))
	{
		/* if server (either judged by IsServer() or clearly an outgoing connect),
		 * and not writing (since otherwise deliver_it will take care of the error), THEN
		 * send a closing link error...
		 */
		sendto_umode(UMODE_OPER, "Lost connection to %s: %s: %s",
			get_client_name(sptr, FALSE), ssl_func, ssl_errstr);
		/* sendto_failops_whoare_opers("Closing link: %s: %s - %s", ssl_func, ssl_errstr, get_client_name(sptr, FALSE)); */
	}
	
	if (errtmp)
	{
		SET_ERRNO(errtmp);
		sptr->error_str = strdup(strerror(errtmp));
	} else {
		SET_ERRNO(P_EIO);
		sptr->error_str = strdup(ssl_errstr);
	}
    return -1;
}

#endif
