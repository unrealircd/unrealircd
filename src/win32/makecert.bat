@title Certificate Generation
openssl req -new -config ssl.cnf -out server.req.pem -keyout server.key.pem -nodes
openssl req -x509 -config ssl.cnf -days 365 -in server.req.pem -key server.key.pem -out server.cert.pem

