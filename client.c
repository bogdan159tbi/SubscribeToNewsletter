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
	fprintf(stderr, "Usage: %s id_client server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd_tcp, sockfd_udp, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {//am pus eu 4 in loc de 3 ca am adaugat id_client
		usage(argv[0]);
	}

	sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd_tcp < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd_tcp, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	fd_set read_set;
	FD_ZERO(&read_set);
	FD_SET(STDIN_FILENO, &read_set);
	FD_SET(sockfd_tcp, &read_set);
	int fd_max = STDIN_FILENO > sockfd_tcp ? STDIN_FILENO : sockfd_tcp;

	while (1) {
		fd_set tmp_set = read_set;
		select(fd_max + 1, &tmp_set, NULL, NULL, NULL);
		if(FD_ISSET(STDIN_FILENO, &tmp_set)) {
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}

			// se trimite mesaj la server
			n = send(sockfd_tcp, buffer, strlen(buffer), 0);
			DIE(n < 0, "send");
		} else if(FD_ISSET(sockfd_tcp, &tmp_set)){
			//primim de pe sockfd
			//memset(buffer, 0, BUFLEN);
			n = recv(sockfd_tcp, buffer, sizeof(buffer) - 1, 0);
			buffer[n] = '\0';// in loc de memset 
			DIE(n < 0, "recv failed");
			if (n == 0){
				fprintf(stdout,"server closed\n");
				break;
			} else
				fprintf(stdout, "received %s\n",buffer);
		}
	}

	close(sockfd_tcp);
    //close(sockfd_udp);

	return 0;
}
