extern   SSL_CTX * ctx;
extern	 SSL_CTX *ctx_server;
extern	 SSL_CTX *ctx_client;

extern   SSL_METHOD *meth;
extern   void init_ssl();
extern   int ssl_handshake(aClient *);   /* Handshake the accpeted con.*/
extern   int ssl_client_handshake(aClient *, ConfigItem_link *); /* and the initiated con.*/
extern	 int ircd_SSL_read(aClient *acptr, void *buf, int sz);
extern	 int ircd_SSL_write(aClient *acptr, const void *buf, int sz);
extern	 int ircd_SSL_accept(aClient *acptr, int fd);
extern	 int ircd_SSL_connect(aClient *acptr);
extern	 int SSL_smart_shutdown(SSL *ssl);
extern	 int ircd_SSL_client_handshake(aClient *acptr);
extern   void SSL_set_nonblocking(SSL *s);
