/* Make these what you want for cert & key files */
#define CERTF  "server.cert.pem"
#define KEYF  "server.key.pem"


extern   SSL_CTX * ctx;
extern   SSL_METHOD *meth;
extern   void init_ssl();
extern   int ssl_handshake(aClient *);   /* Handshake the accpeted con.*/
extern   int ssl_client_handshake(aClient *, ConfigItem_link *); /* and the initiated con.*/
extern	 int ircd_SSL_read(aClient *acptr, void *buf, int sz);
extern	 int ircd_SSL_write(aClient *acptr, const void *buf, int sz);