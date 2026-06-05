@title Certificate Generation
SET OPENSSL_CONF=tls.cnf
openssl ecparam -out ../conf/tls/server.key.pem -name secp384r1 -genkey
openssl req -new -x509 -config tls.cnf -key ../conf/tls/server.key.pem -days 3650 -sha256 -out ../conf/tls/server.cert.pem

