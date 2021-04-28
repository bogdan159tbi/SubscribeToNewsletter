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
int id_exists(char id2[10],struct client **clients, int nr){
	for(int i = 0; i < nr; i++){
		if(strcmp(clients[i]->id,id2) == 0){
			return 1;
		}
	}
	return 0;
}

struct client * create_new_client(struct client **cl, int nr, int sockfd, struct sockaddr_in cli_addr){
	char id[11];
	int ret  = recv(sockfd, id, ID_LEN, 0);
	DIE(ret < 0,"recv id failed");
	if (id_exists(id, cl, nr)) {
		printf("Client %s already connected.\n",id);
		return NULL;
	}
	struct client *clnt = calloc(sizeof(struct client), 1);
	DIE(clnt== NULL, "client fialed to create");
	clnt->sockfd = sockfd;
	clnt->topics = calloc(sizeof(char*),MAX_TOPICS);
	DIE(clnt->topics == NULL, "clnt topics failed");
	clnt->nr_topics = 0;
	strcpy(clnt->id, id);
	printf("New client %s connected from %s:%d.\n",
			clnt->id,inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	return clnt;
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
int is_tcp_client(int subscribed, int sockfd_tcp, int sockfd_udp){
	if((subscribed != STDIN_FILENO) && (subscribed != sockfd_udp) && (subscribed != sockfd_tcp))
		return 1;
	return 0;
}

//return -1 daca n a gasit niciun client pentru socketul dat
struct client * map_client_sockfd(int sockfd, struct client **cls ,int nr){
	for(int i = 0 ; i < nr ;i++){
		if(cls[i]->sockfd == sockfd)
			return cls[i];
	}
	return NULL;
}
void subscribe(struct client **clients, int nr_clients, struct client_tcp msg_from_tcp, int sockfd){
	struct client *c = map_client_sockfd(sockfd, clients, nr_clients);
	c->topics[c->nr_topics] = calloc( TOPIC_LEN,sizeof(char));
	strcpy(c->topics[c->nr_topics++], msg_from_tcp.topic);
	c->sf_active = msg_from_tcp.store;
	printf("client %s subscribed to %s %d\n", c->id,c->topics[c->nr_topics - 1], c->sf_active);
}

//return 1 daca clientul c e abonat la topicul dat
int check_client_subscribed(char *topic , struct client *c){
	for(int i = 0 ; i < c->nr_topics; i++){
		if (!strcmp(topic, c->topics[i]))
			return 1;
	}
	return 0;
}
//stream reprezinta mesajul de la server la clientul tcp
int my_pow(int base, int exp){
	for(int i = 1; i < exp;i++){
		base *= base;
	}
	return base;
}
int parse_udp_msg(struct datagram m, struct server_tcp *msg_from_server, struct sockaddr_in cli_addr_udp){
	//sa fac vreo verificare?
	if(m.type == 0){
		//int
		strcpy(msg_from_server->type, "INT");
		//semn
		int sign = m.payload[0];
		if(sign > 1){
			printf("ignoring message.sign  incorrect from udp\n");
			return -1;
		}
		//nr de tip uin32_t in network byte order 
		uint32_t value = ntohl(*(uint32_t*)(m.payload  + 1));
		if(sign == 1)
			value *= (-1);
		//strcpy(msg_from_server->payload, itoa(value));
		sprintf(msg_from_server->payload,"%d", value);//sa gasesc alta varianta mai originala

	} else if(m.type == 1){
		//short real
		strcpy(msg_from_server->type, "SHORT_REAL");
		uint16_t value;//modulul nr * 100
		value = ntohs(*(uint16_t*)(m.payload));
		double val = value / 100;
		sprintf(msg_from_server->payload,"%.2f", val);

	} else if(m.type == 2){
		//float
		strcpy(msg_from_server->type, "FLOAT");
		int sign = m.payload[0];
		if(sign > 1){
			printf("ignoring message.sign  incorrect from udp\n");
			return -1;
		}
		uint32_t value = ntohs(*(uint32_t*)(m.payload + 1));
		uint8_t abs_pow = m.payload[5];
		float val = value / my_pow(10, abs_pow); // nu stiu daca folosec bine my_pow
		if (sign == 1)
			val *= (-1);
		sprintf(msg_from_server->payload,"%f",val);

	} else if(m.type == 3) {
		//string
		strcpy(msg_from_server->type, "STRING");
		memcpy(msg_from_server->payload,m.payload,CONTENT ); //sau content - 1?? ca sa ia doar 1500
	} 
	msg_from_server->port = ntohs(cli_addr_udp.sin_port);
	strcpy(msg_from_server->ip, inet_ntoa(cli_addr_udp.sin_addr));
	strcpy(msg_from_server->topic, m.topic);
	return 0;
}

int main(int argc, char *argv[])
{
	int sockfd_tcp, newsockfd_tcp, portno;
    int sockfd_udp;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr, cli_addr_udp;
	int n, i, ret;
	socklen_t clilen;
	struct client_tcp msg_from_tcp;
	struct server_tcp msg_from_server;

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

					clients[nr_clients] = create_new_client( clients,nr_clients, newsockfd_tcp, cli_addr);//prea id si socketul corespunzator clientului
					if (clients[nr_clients] == NULL){
						close(newsockfd_tcp);
						break;
					}
					nr_clients++;
					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd_tcp, &read_fds);
					//sa mai adaug dupa asta si tmp_fds = read_fds; ??
					if (newsockfd_tcp > fdmax) { 
						fdmax = newsockfd_tcp;
					}

				} else if (i == sockfd_udp){
                    //s-au primit date pe unul din socketii de client udp
                    struct datagram m;
					socklen_t udp_len = sizeof(struct sockaddr);
					//WARNING: in loc de 1552 sa pun 1501 ??
					// in udp_client.py trimit max 1553 dar in enunt e 1501
                    ret = recvfrom(sockfd_udp, buffer, 1552,0, (struct sockaddr*)&cli_addr_udp, &udp_len);
                    DIE(ret < 0, "recvfrom udp");
					m = *(struct datagram *)buffer;
					//printf("datagram topic= %s type= %d value= %s\n",m.topic, m.type, m.payload);
					//trimite la tcp doar la cei care au subscribe la topicul respectiv
					//converteste datagrama pentru a trimite un mesaj 
					//de la server la client tcp
					ret = parse_udp_msg(m, &msg_from_server,cli_addr_udp);
					if (ret == -1){
						continue;
					}
					printf("%s:%d topic= %s type= %s value= %s\n",
						  msg_from_server.ip, msg_from_server.port, msg_from_server.topic, msg_from_server.type, msg_from_server.payload);
					
					for(int subscribed = 0 ; subscribed <= fdmax; subscribed++){
						if (FD_ISSET(subscribed, &read_fds) && is_tcp_client(subscribed,sockfd_tcp, sockfd_udp) ){
							struct client *c = map_client_sockfd(subscribed, clients, nr_clients);
							if (check_client_subscribed(m.topic,c)){
								ret = send(subscribed, &msg_from_server, sizeof(struct server_tcp),0);
								DIE(ret < 0 ,"sending to subscribers failed");
							}
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
					n = recv(i, &msg_from_tcp, sizeof(struct client_tcp), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						client_left(i, clients, nr_clients);
						FD_CLR(i, &read_fds);
					} else {
                        //verific daca mesajul e corect la client		
						if (msg_from_tcp.action){//subscribe case
							subscribe(clients, nr_clients, msg_from_tcp, i);
						} else {// verifica daca era subscribed inainte
							printf("client unsubscribed to %s %d\n", msg_from_tcp.topic, msg_from_tcp.store);
							//TODO : scade client->nr_topics la unsubscribe
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
