#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stdout, "Usage: %s id_client server_address server_port\n", file); // asta e pt udp sau si pt tcp?
	exit(0);
}

void send_id_to_server(char *id, int sockfd){

	int ret = send(sockfd, id, strlen(id) + 1 , 0);
	DIE(ret < 0,"sending id failed");
}

int parse_msg(struct client_tcp *msg, char *buf){
	//buf este de forma subscribe topic sf
	char *tok;
	buf[strlen(buf) - 1] = '\0';
	tok = strtok(buf," ");
	if (tok[0] == 's'){
		msg->action = 1;
	}
	else
		return 0;
	tok = strtok(NULL, " ");
	strcpy(msg->topic, tok);
	tok = strtok(NULL, " ");
	if(tok == NULL)
		return -1;
	msg->store = atoi(tok);

	return 1;
	
}
int parse_unsubscribe_msg(struct client_tcp *msg, char *buf){
	//buf este de forma subscribe topic sf
	char *tok;
	buf[strlen(buf) - 1] = '\0';
	tok = strtok(buf," ");
    if(tok[0] == 'u')
		msg->action = 0;
	else
		return 0;
	tok = strtok(NULL, " ");
	strcpy(msg->topic, tok);

	return 1;
}
int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	//dupa ce s a conectat trebuie sa anuntam server ul ce id are clientul
	send_id_to_server(argv[1], sockfd);
	
	fd_set read_set;
	FD_ZERO(&read_set);
	FD_SET(STDIN_FILENO, &read_set);
	FD_SET(sockfd, &read_set);
	int fd_max = STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd;

	struct client_tcp msg;
	struct server_tcp msg_from_server;
	while (1) {
		fd_set tmp_set = read_set;
		select(fd_max + 1, &tmp_set, NULL, NULL, NULL);
		if(FD_ISSET(STDIN_FILENO, &tmp_set)) {
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);
			if (!strcmp(buffer, "exit")){
				close(sockfd);
				return 0;
			}
			else if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}
			//se verifica corectitudinea mesajului la client
			//nu mai e nevoie la server 
			if(buffer[0] == 's')
				n = parse_msg(&msg, buffer);
			else 
				n = parse_unsubscribe_msg(&msg,buffer);
			if (n == 0){
				//printf("n = 0 message ignored.format incorect\n");
			}else if (n > 0){
				// se trimite mesaj de tip tcp_client la server
				n = send(sockfd, &msg, sizeof(struct client_tcp), 0);
				DIE(n < 0, "send");
				if (msg.action == 1)
					printf("Subscribed to topic.\n");
				else
					printf("Unsubscribed from topic.\n");
			}
		} else if(FD_ISSET(sockfd, &tmp_set)){
			//primim de pe sockfd
			n = recv(sockfd, &msg_from_server, sizeof(struct server_tcp), 0);
			DIE(n < 0, "recv failed");
			if (n == 0){
				fprintf(stdout,"server closed\n");
				break;
			} else{//trebuie tratat in functie de type
				fprintf(stdout, "%s - %s - %s\n",
						//msg_from_server.ip, msg_from_server.port,
						msg_from_server.topic, msg_from_server.type,msg_from_server.payload);
			}
		}
	}

	close(sockfd);

	return 0;
}
