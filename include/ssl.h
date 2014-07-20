extern MODVAR SSL_CTX *ctx;
extern MODVAR SSL_CTX *ctx_server;
extern MODVAR SSL_CTX *ctx_client;

extern   SSL_METHOD *meth;
extern   int init_ssl();
extern   int ssl_handshake(aClient *);   /* Handshake the accpeted con.*/
extern   int ssl_client_handshake(aClient *, ConfigItem_link *); /* and the initiated con.*/
extern	 int ircd_SSL_accept(aClient *acptr, int fd);
extern	 int ircd_SSL_connect(aClient *acptr, int fd);
extern	 int SSL_smart_shutdown(SSL *ssl);
extern	 void ircd_SSL_client_handshake(int, int, void *);
extern   void SSL_set_nonblocking(SSL *s);
