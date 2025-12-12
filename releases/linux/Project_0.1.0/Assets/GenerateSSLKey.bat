REM Generate certificate in Config dir
openssl genrsa -out Config\key.pem 4096
openssl req -new -x509 -key Config\key.pem -out Config\cert.pem -days 365 -subj "/CN=localhost" -config DebugSSLConfig.cnf

REM Import to Windows trust store
certutil -addstore -user Root Config\cert.pem

echo Generating done.
echo Press any key.

PAUSE