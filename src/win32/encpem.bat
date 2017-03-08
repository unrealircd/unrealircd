@title Encrypting server private key
SET OPENSSL_CONF=ssl.cnf
openssl rsa -in conf/ssl/server.key.pem -out conf/ssl/server.key.c.pem -des3
copy conf/ssl/server.key.c.pem conf/ssl/server.key.pem
