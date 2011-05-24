@title Encrypting server private key
openssl rsa -in server.key.pem -out server.key.c.pem -des3
copy server.key.c.pem server.key.pem