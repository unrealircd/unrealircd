/* 
  This was originally done by the hq.alert.sk implementation
  Modified by Stskeeps
*/
#include "config.h"
#ifdef USE_SSL

#include "struct.h"

/* The SSL structures */
SSL_CTX *ctx_server;
SSL_CTX *ctx_client;

#define CHK_SSL(err) if ((err)==-1) { ERR_print_errors_fp(stderr); }

void init_ctx_server(void)
{
	ctx_server = SSL_CTX_new(SSLv23_server_method());
	if (!ctx_server)
	{
		ircd_log("Failed to do SSL CTX new");
		exit(2);
	}

	if (SSL_CTX_use_certificate_file(ctx_server, CERTF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log("Failed to load SSL certificate %s", CERTF);
		exit(3);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx_server, KEYF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log("Failed to load SSL private key %s", KEYF);
		exit(4);
	}

	if (!SSL_CTX_check_private_key(ctx_server))
	{
		ircd_log("Failed to check SSL private key");
		exit(5);
	}
}

void init_ctx_client(void)
{
	ctx_client = SSL_CTX_new(SSLv3_client_method());
	if (!ctx_client)
	{
		ircd_log("Failed to do SSL CTX new client");
		exit(2);
	}

	if (SSL_CTX_use_certificate_file(ctx_client, CERTF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log("Failed to load SSL certificate %s (client)", "client.pem");
		exit(3);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx_client, CERTF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log("Failed to load SSL private key %s (client)", KEYF);
		exit(4);
	}

	if (!SSL_CTX_check_private_key(ctx_client))
	{
		ircd_log("Failed to check SSL private key (client)");
		exit(5);
	}
}

void init_ssl()
{
	/* SSL preliminaries. We keep the certificate and key with the context. */

	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();
	init_ctx_server();
	init_ctx_client();
}

#define CHK_NULL(x) if ((x)==NULL) {\
        sendto_umode(UMODE_JUNK, "Lost connection to %s:Error in SSL", \
                     get_client_name(cptr, TRUE)); \
	return 0;\
	}

int  ssl_handshake(aClient *cptr)
{
	char *str;
	int  err;

	cptr->ssl = (struct SSL *)SSL_new(ctx_server);
	CHK_NULL(cptr->ssl);
	SSL_set_fd((SSL *) cptr->ssl, cptr->fd);

	err = SSL_accept((SSL *) cptr->ssl);
	if ((err) == -1)
	{
		if (IsHandshake(cptr))
			sendto_ops("Lost connection to %s:Error in SSL_accept()",
			    get_client_name(cptr, TRUE));
		return 0;
	}

	/* Get the cipher - opt */

/*	ircd_log("SSL connection using %s\n",
	    SSL_get_cipher((SSL *) cptr->ssl));
*/
	/* Get client's certificate (note: beware of dynamic
	 * allocation) - opt */

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
	return 0;

}
/* 
   ssl_client_handshake
   
   Return values:
      -1  = Could not SSL_new
      -2  = Error doing SSL_connect
      -3  = Try again 
*/
int  ssl_client_handshake(aClient *cptr)
{
	cptr->ssl = (struct SSL *) SSL_new((SSL_CTX *)ctx_client);
	if (!cptr->ssl)
	{
		sendto_realops("Couldn't SSL_new(ctx_client)");
		return;
	}
	SSL_set_fd((SSL *)cptr->ssl, cptr->fd);
	SSL_set_connect_state((SSL *)cptr->ssl);
	if (SSL_connect((SSL *)cptr->ssl) <= 0)
	{
		sendto_realops("Couldn't SSL_connect");
		return -2;
	}
	set_non_blocking(cptr);
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
	strcat(buf, my_itoa(bits));
	strcat(buf, "bits");
	return (buf);
}


#endif
