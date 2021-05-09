#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <netinet/tcp.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stdout, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN	256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	5	// numarul maxim de clienti in asteptare
#define TOPIC_LEN 51
#define CONTENT 1501
#define ID_LEN 11 //10 + '\0;
#define MAX_TOPICS 50
#define MAX_BUFFER_TCP 100//nr de mesaje tinute in buffer
#endif

//udp msg to server
struct datagram {
	char topic[50];
	uint8_t type;
	char payload[CONTENT]; 

};

//mesaj de la server la client tcp
//reprezentant continutul pentru topicul la care este abonat
struct  server_tcp {
	char ip[50];
	int port;
	char topic[TOPIC_LEN];
	char type[11];
	char payload[CONTENT];
};

//tcp client trimite la server
struct  client_tcp {
	int store; //1 => store , 0 => no store
	int action; // 1 = subscribe 0 = unsubscribe
	char topic[TOPIC_LEN];
};
struct  newsletter {
	char *topic;
	int sf_active; //0 = nu primeste nimic la reconectare ,1 altfel
};
struct  client {
	char id[ID_LEN];
	int sockfd; 
	struct datagram *messages_offline;//retine mesajele pentru id-ul clientului deconectat si le trimite  daca sf _active = 1 
	struct newsletter *topics; //server retine topicurile la care este abonat clientul cu id-ul respectiv
	int nr_topics;
	int online; // 1 daca e conectat sau 0 daca a fost online si s a deconectat
};

//buffer local cand e deconectat
struct  buffer_tcp {
	char id[ID_LEN];
	int sockfd;
	struct server_tcp **messages;//mesaje care trebuie trimise de la server la clientul id tcp
	int nr_msg_from_server;
};

struct topic_buffer {
	char topic_name[TOPIC_LEN];
	struct server_tcp subscribers[MAX_CLIENTS];
};


