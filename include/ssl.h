extern MODVAR SSL_CTX *ctx;
extern MODVAR SSL_CTX *ctx_server;
extern MODVAR SSL_CTX *ctx_client;

extern   SSL_METHOD *meth;
extern   int early_init_ssl();
extern   int init_ssl();
extern   int ssl_handshake(aClient *);   /* Handshake the accpeted con.*/
extern   int ssl_client_handshake(aClient *, ConfigItem_link *); /* and the initiated con.*/
extern	 int ircd_SSL_accept(aClient *acptr, int fd);
extern	 int ircd_SSL_connect(aClient *acptr, int fd);
extern	 int SSL_smart_shutdown(SSL *ssl);
extern	 void ircd_SSL_client_handshake(int, int, void *);
extern   void SSL_set_nonblocking(SSL *s);
extern   SSL_CTX *init_ctx(SSLOptions *ssloptions, int server);

#define SSL_PROTOCOL_TLSV1		0x0001
#define SSL_PROTOCOL_TLSV1_1	0x0002
#define SSL_PROTOCOL_TLSV1_2	0x0004
#define SSL_PROTOCOL_TLSV1_3	0x0008

#define SSL_PROTOCOL_ALL		0xffff
