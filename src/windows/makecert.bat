@title Certificate Generation
SET OPENSSL_CONF=ssl.cnf
openssl ecparam -out server.key.pem -name secp384r1 -genkey
openssl req -new -config ssl.cnf -out conf/tls/server.req.pem -key conf/tls/server.key.pem -nodes
openssl req -x509 -config ssl.cnf -days 3650 -sha256 -in conf/tls/server.req.pem -key conf/tls/server.key.pem -out conf/tls/server.cert.pem

