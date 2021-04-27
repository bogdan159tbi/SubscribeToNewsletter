#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	5	// numarul maxim de clienti in asteptare
#define TOPIC_LEN 51
#define  CONTENT 1501
#define ID_LEN 11 //10 + '\0;
#endif

//udp msg to server
struct datagram {
	char topic[TOPIC_LEN];
	uint8_t type;
	char payload[CONTENT]; // s ar putea sa fie 1501

};

//mesajele primite de clientul tcp de la server
//reprezentant continutul pentru topicul la care este abonat
struct server_tcp {
	char ip[50];
	int port;
	char topic[TOPIC_LEN];
	char type[11];
	char payload[CONTENT];
};

//tcp client trimite la server
struct client_tcp {
	int store; //1 => store , 0 => no store
	int action; // 1 = subscribe 0 = unsubscribe
	char topic[TOPIC_LEN];
};
struct client {
	char id[ID_LEN];
	int sockfd; //as putea crea un map<sockfd, client>
	int sf_active; //0 = nu primeste nimic la reconectare ,1 altfel
	struct datagram *messages_offline;//retine mesajele pentru id-ul clientului deconectat si le trimite  daca sf _active = 1 
	char *topics; //server retine topicurile la care este abonat clientul cu id-ul respectiv
};

