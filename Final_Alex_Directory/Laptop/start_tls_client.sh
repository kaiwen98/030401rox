# !/bin/sh
echo Launching TLS Client...
cd tls-client-lib
g++ alex-pi_tls.cpp make_tls_client.cpp tls_client_lib.cpp tls_pthread.cpp tls_common_lib.cpp -pthread -lssl -lcrypto -lncurses -o tls-alex-client
# Change IP address if necessary.
./tls-alex-client 192.168.43.186 22 
