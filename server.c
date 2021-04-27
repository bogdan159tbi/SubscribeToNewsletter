#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}
//verifica daca clientul cu id-ul = id2 
//este deja conectat
int id_exists(char id2[10],struct client *clients, int nr){
	for(int i = 0; i < nr; i++){
		if(strcmp(clients[i].id,id2) == 0){
			return 1;
		}
	}
	return 0;
}

void receive_id_client(struct client **cl, int sockfd, struct sockaddr_in cli_addr){
	int ret  = recv(sockfd, (*cl)->id, ID_LEN, 0);
	DIE(ret < 0,"recv id failed");
	(*cl)->sockfd = sockfd;
	printf("New client %s connected from %s:%d.\n",
			(*cl)->id,inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
}
void close_sockets(int sockfd_tcp, int sockfd_udp, fd_set *read_fds, int fdmax) {
	for (int fds = 0; fds <= fdmax; fds++){
		if(FD_ISSET(fds,read_fds) && fds != sockfd_udp && fds != sockfd_tcp)
			close(fds);
	}
	close(sockfd_tcp);
	close(sockfd_udp);
}
//cauta clientul in lista dupa sockfd si afiseaza
void client_left(int sockfd, struct client **cls, int nr){
	// conexiunea s-a inchis
	for(int i = 0 ;i < nr; i++){
		if (cls[i]->sockfd == sockfd){
			printf("Client %s disconnected.\n",cls[i]->id);
			break;
		}
	}
	close(sockfd);
	// se scoate din multimea de citire socketul inchis 
}

void show_clients(struct client *cls, int nr){
	for(int i = 0 ; i < nr;i++){
		printf("Client %s ", cls[i].id);
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	int sockfd_tcp, newsockfd_tcp, portno;
    int sockfd_udp;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr, cli_addr_udp;
	int n, i, ret;
	socklen_t clilen;
	struct client **clients;//retine lista cu clientii offline / online
	int nr_clients = 0;
	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}
	clients = calloc(sizeof(struct client*), MAX_CLIENTS);
	DIE(clients < 0,"clients failed");
	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd_tcp < 0, "socket");

    sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockfd_udp < 0,"socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sockfd_udp, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr));
    DIE(ret < 0, "bind udp");

    //ret = listen(sockfd_udp, MAX_CLIENTS);
    //DIE(ret < 0, "listen udp");

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sockfd_udp, &read_fds);
    fdmax = sockfd_udp > STDIN_FILENO ? sockfd_udp : STDIN_FILENO;

	ret = bind(sockfd_tcp, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd_tcp, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd_tcp, &read_fds);
	fdmax = sockfd_tcp > fdmax ? sockfd_tcp : fdmax;;
	while (1) {
		tmp_fds = read_fds; 
		//select pastreaza doar descriptorii pe care se primesc data
		//de aia se foloseste o multime temporare pt descriptori
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd_tcp) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd_tcp = accept(sockfd_tcp, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd_tcp < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd_tcp, &read_fds);
					//sa mai adaug dupa asta si tmp_fds = read_fds; ??
					if (newsockfd_tcp > fdmax) { 
						fdmax = newsockfd_tcp;
					}

					clients[nr_clients] = calloc(sizeof(struct client), 1);
					DIE(clients[nr_clients] == NULL, "client fialed to create");

					receive_id_client( (&clients[nr_clients]), newsockfd_tcp, cli_addr);//prea id si socketul corespunzator clientului
					nr_clients++;

				} else if (i == sockfd_udp){
                    //s-au primit date pe unul din socketii de client udp
                    //asa ca trebuie decodificat mesajul dupa cum se cere
                    //memset(buffer, 0, BUFLEN);
                    struct udp_msg m;
					socklen_t udp_len = sizeof(struct sockaddr);
                    ret = recvfrom(sockfd_udp, &m, sizeof(struct udp_msg),0, (struct sockaddr*)&cli_addr_udp, &udp_len);
                    DIE(ret < 0, "recvfrom udp");
					printf("received from udp %s:%d %s\n",inet_ntoa(cli_addr_udp.sin_addr),ntohs(cli_addr_udp.sin_port) , m.payload);
					//trimite la tcp doar la cei care au subscribe la topicul respectiv
					for(int subscribed = 0 ; subscribed <= fdmax; subscribed++){
						if (FD_ISSET(subscribed, &read_fds) && subscribed != sockfd_udp && subscribed != sockfd_tcp){
							ret = send(subscribed, m.payload, strlen(m.payload) + 1,0);
							DIE(ret < 0 ,"sending to subscribers failed");
						}
					}
                } else if (i == STDIN_FILENO){//cand primeste comanda exit de la tastatura
					//close all sockets
					close_sockets(sockfd_tcp, sockfd_udp, &read_fds, fdmax);
					return 0;//tre sa dea return 0 sau mai face altceva dupa
				}
                else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						client_left(i, clients, nr_clients);
						FD_CLR(i, &read_fds);
					} else {
                        //cazul cand tre sa trimit catre client2 ce s a primit de la client 12
						char s1[BUFLEN];
						int nr;
						//pune s1 mesajul in sine si in nr se pune file descriptorul socketului destinatie
						int elem = sscanf(buffer, "%d%[^\n]s", &nr, s1);//tre parsat si id ul cu care se conecteaza clientul
						if(elem == 2){ //daca mesajul este format corect ,adica sockId + msg_content
							printf ("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);
							int res = send(nr, s1, strlen(s1) + 1,0);// + \0
							DIE(res < 0 ,"send failed\n");
						} else{
							fprintf(stdout,"message ignored\n");// in cazul in care mesajul trimis nu are formatul dorit
						}
					}
				}
			}
		}
	}

	close(sockfd_tcp);
    close(sockfd_udp);
	return 0;
}
