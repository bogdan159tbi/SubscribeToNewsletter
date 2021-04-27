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
	fprintf(stderr, "Usage: %s id_client server_address server_port\n", file); // asta e pt udp sau si pt tcp?
	exit(0);
}

void send_id_to_server(char *id, int sockfd){

	int ret = send(sockfd, id, strlen(id) + 1 , 0);
	DIE(ret < 0,"sending id failed");
}

int parse_msg(struct client_tcp *msg, char *buf){
	//buf este de forma subscribe topic sf
	char *tok;
	tok = strtok(buf, " ");
	if (!strcmp("subscribe", tok))
		msg->action = 1;
	else if(!strcmp("unsubscribe", tok))
		msg->action = 0;
	else
		return 0;
	tok = strtok(NULL, " ");
	strcpy(msg->topic, tok);

	tok = strtok(NULL, " ");
	msg->store = atoi(tok);
	return 1;
	
}
int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {//am pus eu 4 in loc de 3 ca am adaugat id_client
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
				printf("leaving...\n");//nu e necesar
				return 0;
			}
			else if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}
			//se verifica corectitudinea mesajului la client
			//nu mai e nevoie la server 
			// se trimite mesaj de tip tcp_client la server
			n = parse_msg(&msg, buffer);//sper ca am parsat bine 
			if (n == 0){
				printf("message ignored.format incorect\n");
			}else {
				n = send(sockfd, &msg, sizeof(struct client_tcp), 0);
				DIE(n < 0, "send");
				if (msg.action == 1)
					printf("Client subscribed to topic\n");
				else
					printf("Client unsubscribed to topic\n");
			}
		} else if(FD_ISSET(sockfd, &tmp_set)){
			//primim de pe sockfd
			//memset(buffer, 0, BUFLEN);
			n = recv(sockfd, &msg_from_server, sizeof(struct server_tcp), 0);
			DIE(n < 0, "recv failed");
			
			if (n == 0){
				fprintf(stdout,"server closed\n");
				break;
			} else{//trebuie tratat in functie de type
				fprintf(stdout, "%s:%d %s %s\n",
						msg_from_server.ip, msg_from_server.port,
						msg_from_server.topic, msg_from_server.payload);
			}
		}
	}

	close(sockfd);

	return 0;
}
