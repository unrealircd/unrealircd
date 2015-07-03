@title Encrypting server private key
openssl rsa -in conf/ssl/server.key.pem -out conf/ssl/server.key.c.pem -des3
copy conf/ssl/server.key.c.pem conf/ssl/server.key.pem
