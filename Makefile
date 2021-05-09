# Protocoale de comunicatii:
# Laborator 8: Multiplexare
# Makefile

CFLAGS = -Wall -g -O2 

all: server subscriber client_udp

# Compileaza server.c
server: 
	gcc  $(CFLAGS) server.c -o $@ -lm
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
