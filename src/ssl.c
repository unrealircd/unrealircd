/* 
  This was originally done by the hq.alert.sk implementation
  Modified by Stskeeps
*/
#include "config.h"
#ifdef USE_SSL

#include "struct.h"

/* The SSL structures */
SSL_CTX *ctx;
SSL_METHOD *meth;

void init_ssl()
{
	/* SSL preliminaries. We keep the certificate and key with the context. */

	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();
	meth = SSLv23_server_method();
	ctx = SSL_CTX_new(meth);
	if (!ctx)
	{
		ircd_log("Failed to do SSL CTX new");
		exit(2);
	}

	if (SSL_CTX_use_certificate_file(ctx, CERTF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log("Failed to load SSL certificate %s", CERTF);
		exit(3);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, KEYF, SSL_FILETYPE_PEM) <= 0)
	{
		ircd_log("Failed to load SSL private key %s", KEYF);
		exit(4);
	}

	if (!SSL_CTX_check_private_key(ctx))
	{
		ircd_log("Failed to check SSL private key");
		exit(5);
	}

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

	cptr->ssl = (struct SSL *)SSL_new(ctx);
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
		Free(str);

		str =
		    X509_NAME_oneline(X509_get_issuer_name((X509 *) cptr->
		    client_cert), 0, 0);
		CHK_NULL(str);
		// log (L_DEBUG, "\t issuer: %s\n", str);
		Free(str);

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

int  ssl_client_handshake(struct Client *cptr)
{

	char *str;
	int  err;

	cptr->ssl = (struct SSL *)SSL_new(ctx);
	CHK_NULL(cptr->ssl);
	SSL_set_fd((SSL *) cptr->ssl, cptr->fd);
	set_blocking(cptr->fd);
	err = SSL_connect((SSL *) cptr->ssl);
	set_non_blocking(cptr->fd);
	if ((err) == -1)
	{
		ircd_log("Could connect to %s:Error in SSL_connect()",
		    get_client_name(cptr, TRUE));
		return 0;
	}

	/* Following two steps are optional and not required for
	   data exchange to be successful. */

	/* Get the cipher - opt */
//  set_blocking(cptr->fd);
/*	ircd_log("SSL connection using %s\n",
	    SSL_get_cipher((SSL *) cptr->ssl));
*/
	/* Get server's certificate (note: beware of dynamic allocation) - opt */

	cptr->client_cert =
	    (struct X509 *)SSL_get_peer_certificate((SSL *) cptr->ssl);
	set_non_blocking(cptr->fd);
	CHK_NULL(cptr->client_cert);
	// log (L_NOTICE,"Server certificate:\n");

	str =
	    X509_NAME_oneline(X509_get_subject_name((X509 *) cptr->client_cert),
	    0, 0);
	CHK_NULL(str);
//  log (L_NOTICE, "\t subject: %s\n", str);
	Free(str);

	str =
	    X509_NAME_oneline(X509_get_issuer_name((X509 *) cptr->client_cert),
	    0, 0);
	CHK_NULL(str);
//  log (L_NOTICE, "\t issuer: %s\n", str);
	Free(str);

	/* We could do all sorts of certificate verification stuff here before
	   deallocating the certificate. */

	X509_free((X509 *) cptr->client_cert);
	return 1;
}
#endif
