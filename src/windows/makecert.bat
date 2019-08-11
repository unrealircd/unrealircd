@title Certificate Generation
SET OPENSSL_CONF=ssl.cnf
openssl ecparam -out server.key.pem -name secp384r1 -genkey
openssl req -new -config ssl.cnf -out conf/ssl/server.req.pem -key conf/ssl/server.key.pem -nodes
openssl req -x509 -config ssl.cnf -days 3650 -sha256 -in conf/ssl/server.req.pem -key conf/ssl/server.key.pem -out conf/ssl/server.cert.pem

