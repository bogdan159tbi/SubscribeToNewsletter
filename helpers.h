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
#define TOPIC_LEN 50
#define  CONTENT 1500
#define ID_LEN 11 //10 + '\0;
#endif

struct udp_msg {
	char topic[TOPIC_LEN];
	uint8_t type;
	char payload[CONTENT]; // s ar putea sa fie 1501

};

struct client {
	char id[ID_LEN];
	int sockfd; //as putea crea un map<sockfd, client>
	int sf_active; //0 = nu primeste nimic la reconectare ,1 altfel
	struct udp_msg *messages_offline;//retine mesajele pentru id-ul clientului deconectat si le trimite  daca sf _active = 1 
	char *topics; //server retine topicurile la care este abonat clientul cu id-ul respectiv
};

