CFLAGS=-Wall -Werror -Wpedantic -ggdb -std=c99
LDFLAGS=
CC=gcc

## --- Commands ---

# --- Targets ---

all: inet unix

inet: inet_server_test inet_client_test

inet_server_test: inet_server.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o inet_server_test inet_server.c

inet_client_test: inet_client.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o inet_client_test inet_client.c

unix: unix_server_test unix_client_test

unix_server_test: unix_server.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o unix_server_test unix_server.c

unix_client_test: unix_client.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o unix_client_test unix_client.c

clean:
	rm *_test 2>/dev/null || :
