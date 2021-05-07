# Protocoale de comunicatii:
# Laborator 8: Multiplexare
# Makefile

CFLAGS = -Wall -g -O2

# Portul pe care asculta serverul (de completat)
PORT = 

# Adresa IP a serverului (de completat)
IP_SERVER = 

all: server subscriber client_udp

# Compileaza server.c
server: server.c

# Compileaza client.c
subcriber: subscriber.c

client_udp: client_udp.c
.PHONY: clean run_server run_client

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul
run_client:
	./subscriber ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
