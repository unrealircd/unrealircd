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
#include "common.h"
#include "struct.h"
#include "h.h"
#include "sys.h"
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

typedef struct {
	int *size;
	char **buffer;
} StreamIO;

static StreamIO *streamp;
#define CHK_SSL(err) if ((err)==-1) { ERR_print_errors_fp(stderr); }
#ifdef _WIN32
LRESULT SSLPassDLG(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam) {
	StreamIO *stream;
	switch (Message) {
		case WM_INITDIALOG:
			return TRUE;
		case WM_COMMAND:
			stream = (StreamIO *)streamp;
			if (LOWORD(wParam) == IDCANCEL) {
				*stream->buffer = NULL;
				EndDialog(hDlg, TRUE);
			}
			else if (LOWORD(wParam) == IDOK) {
				GetDlgItemText(hDlg, IDC_PASS, *stream->buffer, *stream->size);
				EndDialog(hDlg, TRUE);
			}
			return FALSE;
		case WM_CLOSE:
			if (stream)
				*stream->buffer = NULL;
			EndDialog(hDlg, TRUE);
		default:
			return FALSE;
	}
}
#endif				
				
				
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
		strncpy(buf, (char *)beforebuf, size);
		buf[size - 1] = '\0';
		return (strlen(buf));
	}
#ifndef _WIN32
	pass = getpass("Password for SSL private key: ");
#else
	pass = passbuf;
	stream.buffer = &pass;
	stream.size = &passsize;
	streamp = &stream;
	DialogBoxParam(hInst, "SSLPass", hwIRCDWnd, (DLGPROC)SSLPassDLG, (LPARAM)NULL); 
#endif
	if (pass)
	{
		strncpy(buf, (char *)pass, size);
		strncpy(beforebuf, (char *)pass, sizeof(beforebuf));
		beforebuf[sizeof(beforebuf) - 1] = '\0';
		buf[size - 1] = '\0';
		before = 1;
		return (strlen(buf));
	}
	return 0;
}

void init_ctx_server(void)
{
	ctx_server = SSL_CTX_new(SSLv23_server_method());
	if (!ctx_server)
	{
		ircd_log(LOG_ERROR, "Failed to do SSL CTX new");
		exit(2);
	}
	SSL_CTX_set_default_passwd_cb(ctx_server, ssl_pem_passwd_cb);

	if (SSL_CTX_use_certificate_file(ctx_server, CERTF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log(LOG_ERROR, "Failed to load SSL certificate %s", CERTF);
		exit(3);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx_server, KEYF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log(LOG_ERROR, "Failed to load SSL private key %s", KEYF);
		exit(4);
	}

	if (!SSL_CTX_check_private_key(ctx_server))
	{
		ircd_log(LOG_ERROR, "Failed to check SSL private key");
		exit(5);
	}
}

void init_ctx_client(void)
{
	ctx_client = SSL_CTX_new(SSLv23_client_method());
	if (!ctx_client)
	{
		ircd_log(LOG_ERROR, "Failed to do SSL CTX new client");
		exit(2);
	}
	SSL_CTX_set_default_passwd_cb(ctx_client, ssl_pem_passwd_cb);
	if (SSL_CTX_use_certificate_file(ctx_client, CERTF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log(LOG_ERROR, "Failed to load SSL certificate %s (client)", CERTF);
		exit(3);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx_client, KEYF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log(LOG_ERROR, "Failed to load SSL private key %s (client)", KEYF);
		exit(4);
	}

	if (!SSL_CTX_check_private_key(ctx_client))
	{
		ircd_log(LOG_ERROR, "Failed to check SSL private key (client)");
		exit(5);
	}
}

void init_ssl(void)
{
	/* SSL preliminaries. We keep the certificate and key with the context. */

	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();
	init_ctx_server();
	init_ctx_client();
}

#define CHK_NULL(x) if ((x)==NULL) {\
        sendto_snomask(SNO_JUNK, "Lost connection to %s:Error in SSL", \
                     get_client_name(cptr, TRUE)); \
	return 0;\
	}

int  ssl_handshake(aClient *cptr)
{
	char *str;
	int  err;

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

	/* Get client's certificate (note: beware of dynamic
	 * allocation) - opt */
        /* We do not do this -Stskeeps */

#ifdef NO_CERTCHECKING
	cptr->client_cert =
	    (struct X509 *)SSL_get_peer_certificate((SSL *) cptr->ssl);

	if (cptr->client_cert != NULL)
	{
		// log (L_DEBUG,"Client certificate:\n");

		str =
		    X509_NAME_oneline(X509_get_subject_name((X509 *) cptr->
		    client_cert), 0, 0);
		CHK_NULL(str);
		// log (L_DEBUG, "\t subject: %s\n", str);
		free(str);

		str =
		    X509_NAME_oneline(X509_get_issuer_name((X509 *) cptr->
		    client_cert), 0, 0);
		CHK_NULL(str);
		// log (L_DEBUG, "\t issuer: %s\n", str);
		free(str);

		/* We could do all sorts of certificate
		 * verification stuff here before
		 *        deallocating the certificate. */

		X509_free((X509 *) cptr->client_cert);
	}
	else
	{
		// log (L_DEBUG, "Client does not have certificate.\n");
	}
#endif
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
	switch(ssl->session->ssl_version)
	{
		case SSL2_VERSION:
			strcat(buf, "SSLv2"); break;
		case SSL3_VERSION:
			strcat(buf, "SSLv3"); break;
		case TLS1_VERSION:
			strcat(buf, "TLSv1"); break;
		default:
			strcat(buf, "UNKNOWN");
	}
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
               if (ERRNO == EWOULDBLOCK || ERRNO == EAGAIN ||
                       ERRNO == EINTR)
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
		return fatal_ssl_error(ssl_err, SAFE_SSL_WRITE, acptr);
       }
    }
    return len;
}

int ircd_SSL_client_handshake(aClient *acptr)
{
	int ssl_err;
	
	acptr->ssl = SSL_new(ctx_client);
	if (!acptr->ssl)
	{
		sendto_realops("Failed to SSL_new(ctx_client)");
		return FALSE;
	}
	SSL_set_fd(acptr->ssl, acptr->fd);
	SSL_set_connect_state(acptr->ssl);
	SSL_set_nonblocking(acptr->ssl);
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
			SetSSLConnectHandshake(acptr);
			return 0;
		case 1: 
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
		    /* handshake will be completed later . . */
		    return 0;
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
    char *errstr = (char *)strerror(errtmp);
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
    ircd_log(LOG_ERROR, "exiting ssl client %s: %s: %s",
    	sptr->name, ssl_func, ssl_errstr);
    SET_ERRNO(errtmp ? errtmp : P_EIO); /* Stick a generic I/O error */
    sptr->flags |= FLAGS_DEADSOCKET;
    return -1;
}

#endif