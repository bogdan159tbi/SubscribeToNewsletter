#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <math.h>

void usage(char *file)
{
	fprintf(stdout, "Usage: %s server_port\n", file);
	exit(0);
}
//verifica daca clientul cu id-ul = id2 
//este deja conectat
struct client * id_exists(char id2[10],struct client **clients, int nr){
	for(int i = 0; i < nr; i++){
		if(strcmp(clients[i]->id,id2) == 0){
			return clients[i];
		}
	}
	return NULL;
}

//daca nu a mai fost conectat clientul
//creeaza un nou client
//daca a mai fost conectat,doar seteaza online pe 1
struct client * create_new_client( int sockfd, struct sockaddr_in cli_addr , char *id){

	struct client *clnt = calloc(sizeof(struct client), 1);
	DIE(clnt== NULL, "client failed to create");
	clnt->sockfd = sockfd;
	clnt->topics = calloc(sizeof(struct newsletter),MAX_TOPICS);
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
void client_left(int sockfd, struct client ***cls, int nr){
	// conexiunea s-a inchis
	
	for(int i = 0 ;i < nr; i++){
		if ((*cls)[i]->sockfd == sockfd){
			printf("Client %s disconnected.\n",(*cls)[i]->id);
			(*cls)[i]->online = 0;
			
			struct client *c = (*cls)[i];	
			//daca da segfault e de la free d aici
			if (c->nr_topics == 0){
				//atunci nu mai am de ce sa retin clientul
				free(c->topics);
				free(c);
				for(int j = i ; j < nr - 1;j++){
					(*cls)[j] = (*cls)[j+1];
				}
			}		
			break;
		}
	}
}

//pentru debug sau extinderea functionalitatii 
//afisez clientii existenti ,precum in lab 8
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
/*
return 1 daca clientul c e abonat la topicul dat
*/
int check_client_subscribed(char *topic , struct client *c){
	for(int i = 0 ; i < c->nr_topics; i++){
		if (!strcmp(topic, c->topics[i].topic)){
			return 1;
		}
	}
	return 0;
}

void subscribe(struct client ***clients, int nr_clients, struct client_tcp msg_from_tcp, int sockfd){
	struct client *c = map_client_sockfd(sockfd, *clients, nr_clients);

	c->topics[c->nr_topics].topic = calloc( TOPIC_LEN,sizeof(char));
	DIE(!c->topics[c->nr_topics].topic,"client subsc calloc");
	
	strcpy(c->topics[c->nr_topics].topic, msg_from_tcp.topic);
	c->topics[c->nr_topics++].sf_active = msg_from_tcp.store;
}
int unsubscribe(struct client ***clients, int nr_clients, struct client_tcp msg_from_tcp, int sockfd,char *topic_name){
	struct client *c;
	for(int i = 0 ;i < nr_clients; i++){
		if((*clients)[i]->sockfd == sockfd) {
			c = (*clients)[i];
		}
	}
	//remove topic with name x from list of topics
	int topic_index = -1;
	for(int i = 0 ; i < c->nr_topics; i++){
		if (!strcmp(topic_name, c->topics[i].topic)){
			topic_index = i;
		}
	}
	if(topic_index < 0){
		return topic_index; //nu exista topicul in lista de subscribtii
	}
	for(int i = topic_index;i < c->nr_topics - 1; i++){
		strcpy(c->topics[i].topic, c->topics[i+1].topic);
	}
	//anulam ultimul element din lista de topicuri
	c->nr_topics--;
	return 0;
}

int parse_udp_msg(struct datagram m, struct server_tcp *msg_from_server, struct sockaddr_in cli_addr_udp){
	if(m.type == 0){
		//int
		strcpy(msg_from_server->type, "INT");
		//semn
		int sign = m.payload[0];
		if(sign > 1){
			//printf("ignoring message.sign  incorrect from udp\n");
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
		double value;//modulul nr * 100
		value = ntohs(*(uint16_t*)(m.payload));
		value = value / 100;
		sprintf(msg_from_server->payload,"%.2f", value);

	} else if(m.type == 2){
		//float
		strcpy(msg_from_server->type, "FLOAT");
		int sign = m.payload[0];
		if(sign > 1){
			//printf("ignoring message.sign  incorrect from udp\n");
			return -1;
		}
		double value = ntohl(*(uint32_t*)(m.payload + 1));
		uint8_t abs_pow = m.payload[5];
		double val;
		if(!abs_pow)
			val = value ; 
		else
			val = value /  pow(10,abs_pow); 
		if (sign == 1)
			val *= (-1);
		sprintf(msg_from_server->payload,"%lf",val);

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
void show_server_to_tcp(struct server_tcp msg_from_server){
	printf("%s:%d topic= %s type= %s value= %s\n",
		   msg_from_server.ip, msg_from_server.port, msg_from_server.topic, msg_from_server.type, msg_from_server.payload);
}
int was_connected_before(struct client ***clients, int nr,char *id, struct sockaddr_in cli_addr){
						
	struct client *check_online = id_exists(id , *clients, nr);//+1 pt ca se trimite c1 iar id e 1 
	if (check_online ) {
		if(check_online->online == 1){
			printf("Client %s already connected.\n",id);
			return 0;
		} else if(check_online->online == 0){
			//a mai fost conectat inainte
			printf("New client %s connected from %s:%d.\n",
					check_online->id,inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
			check_online->online = 1;
			return 1;
		}
	}
	return -1;//nu exista id
}
//(c->topics, c->nr_topics, msg_from_server.topic)
int is_SF_active(struct newsletter *topics, int nr_topics, char *topic){
	for(int i = 0; i < nr_topics; i++){
		if(!strcmp(topics[i].topic, topic) && topics[i].sf_active){
			return 1;
		}
	}
	return 0;
}
/**
 * 1 daca e deja in lista celor cu buffer
 * 0 altfel
 */
struct buffer_tcp * was_already_buffered(char *id, int size_tcp_buffer, struct buffer_tcp **tcp_buff){
	for(int i = 0; i < size_tcp_buffer; i++){
		if (!strcmp(tcp_buff[i]->id, id))
			return tcp_buff[i];
	}
	return NULL;
}
struct buffer_tcp *store_msg_udp(struct buffer_tcp **buff, int buff_size, int sockfd, char *id, struct server_tcp msg_from_server){
	struct buffer_tcp *msg = was_already_buffered(id, buff_size, buff);
	if (msg == NULL){
		msg = calloc(MAX_BUFFER_TCP,sizeof(struct buffer_tcp*));
		msg->nr_msg_from_server = 0;
		msg->messages = calloc(1,sizeof(struct server_tcp));
		DIE(msg == NULL, "store msg\n");
		DIE(msg->messages == NULL, "store msg\n");
	}
	msg->sockfd = sockfd;
	strcpy(msg->id,id);
	msg->messages[msg->nr_msg_from_server] = calloc(1,sizeof(struct server_tcp));
	DIE(msg->messages[msg->nr_msg_from_server] == NULL, "server tcp failed calloc");
	memcpy(msg->messages[msg->nr_msg_from_server++] ,&msg_from_server, sizeof(struct server_tcp));//sau size de msg_from_server??
	return msg;
}
/**
 * 0 if stored msg sent successfully
 * -1 otherwise
*/
int send_buffered_msg(char *id, struct buffer_tcp ***buffered,int *size_local, struct client **clients, int nr_clients) {
	if ((*buffered)[0] == NULL){
		//nu exista mesaje in bufferul local
		return -1;
	}
	struct buffer_tcp *buffered_msg;
	int ret;
	int index = -1 ; //
	for(int i = 0 ;i < *size_local; i++){
		buffered_msg = (*buffered)[i];
		if (!strcmp(id, buffered_msg->id)){
			//send stored messages for client with id = id
			index = i;
			for(int j = 0 ;j < buffered_msg->nr_msg_from_server; j++){
				ret = send(buffered_msg->sockfd, buffered_msg->messages[j], sizeof(struct server_tcp), 0);//daca nu da bine aloc pe stiva messages din struct buffer_tcp
				DIE(ret < 0,"sending buffered failed");
				//remove buffered message
			}
			break;//am gasit deja clientul si tre sa elimin din buffer
		}
	}
	if (index > -1){
		//daca am gasit client cu id = id
		//elibereaza bufferul local pentru el
		buffered_msg = (*buffered)[index];
		for(int i = index; i < *size_local - 1; i++){
			(*buffered)[i] = (*buffered)[i+1];
		}
		*size_local = (*size_local) - 1;
		for(int i = 0 ; i < buffered_msg->nr_msg_from_server; i++){
			free(buffered_msg->messages[i]);
		}
		free(buffered_msg->messages);
		buffered_msg->nr_msg_from_server = 0;
		free(buffered_msg);

	}

	return 0;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd_tcp, newsockfd_tcp, portno;
    int sockfd_udp;
	char buffer[1552];//inaitne era BUFLEN
	struct sockaddr_in serv_addr, cli_addr, cli_addr_udp;
	int n, i, ret;
	socklen_t clilen;
	struct client_tcp msg_from_tcp;
	struct server_tcp msg_from_server;
	struct buffer_tcp **local_buffer = calloc(MAX_BUFFER_TCP, sizeof(struct buffer_tcp*));
	DIE(local_buffer == NULL,"buffer_tcp calloc\n");
	int size_local_buffer = 0;
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
					DIE(newsockfd_tcp < 0, "accept\n");
					int flag = 1;
					setsockopt(newsockfd_tcp, IPPROTO_TCP, TCP_NODELAY, (char*)&flag,sizeof(int));
					char id_client[11];
					ret  = recv(newsockfd_tcp, id_client, ID_LEN, 0);
					DIE(ret < 0,"recv id failed\n");
					
					ret = was_connected_before(&clients, nr_clients, id_client, cli_addr); 
					if ( ret < 0){
						clients[nr_clients] = create_new_client( newsockfd_tcp, cli_addr, id_client);//prea id si socketul corespunzator clientului
						if (clients[nr_clients] == NULL){
							close(newsockfd_tcp);
							break;
						} else {
							clients[nr_clients]->online = 1;
							nr_clients++;
						}
					} else if(ret == 0){
						close(newsockfd_tcp);
						continue; 
					} else {
						//daca a mai fost conectat , trimite-i daca are mesaje in buffer
						
						ret = send_buffered_msg(id_client, &local_buffer, &size_local_buffer,clients, nr_clients);
					}
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
					memset(buffer,0, sizeof(buffer));
                    ret = recvfrom(sockfd_udp, buffer, 1552,0, (struct sockaddr*)&cli_addr_udp, &udp_len);//1552??
                    DIE(ret < 0, "recvfrom udp\n");

					m = *(struct datagram *)buffer;
					//trimite la tcp doar la cei care au subscribe la topicul respectiv
					//converteste datagrama pentru a trimite un mesaj de la server la client tcp
					ret = parse_udp_msg(m, &msg_from_server,cli_addr_udp);
					if (ret == -1){
						continue;
					}
					//verific daca pentru topicul primit exista subscriberi
					//se poate ca un subscriber sa fie offline
					for(int subscribed = 0 ; subscribed <= fdmax; subscribed++){
						if (FD_ISSET(subscribed, &read_fds) && is_tcp_client(subscribed,sockfd_tcp, sockfd_udp) ){
							struct client *c = map_client_sockfd(subscribed, clients, nr_clients);
							if (check_client_subscribed(msg_from_server.topic, c) && c->online == 1){//trimit doar la subscriberi online
								ret = send(subscribed, &msg_from_server, sizeof(struct server_tcp),0);
								DIE(ret < 0 ,"sending to subscribers failed\n");
							} 
						}
					}
					for(int offlines = 0 ; offlines < nr_clients; offlines++){
						if(check_client_subscribed(msg_from_server.topic, clients[offlines]) && clients[offlines]->online == 0 && is_SF_active(clients[offlines]->topics, clients[offlines]->nr_topics, msg_from_server.topic)) {
								//pentru subscribe offline adaug in buffer_local si cand se conecteaza unul la accept verific daca a mai fost conectat inainte
								// apoi ii trimit din buffer_local daca are vreun mesaj
								local_buffer[size_local_buffer++] = store_msg_udp( local_buffer, size_local_buffer,clients[offlines]->sockfd, clients[offlines]->id, msg_from_server);//in loc de c->sockfd sa pun subscribed ??
							} 
					}
					
                } else if (i == STDIN_FILENO){//cand primeste comanda exit de la tastatura
					//closing all sockets
					close_sockets(sockfd_tcp, sockfd_udp, &read_fds, fdmax);
					return 0;
				}
                else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer,0 ,sizeof(buffer));
					n = recv(i, &msg_from_tcp, sizeof(struct client_tcp), 0);
					DIE(n < 0, "recv\n");
					//printf("unsubscribe %d %s\n",msg_from_tcp.action, msg_from_tcp.topic);
					if (n == 0) {
						client_left(i, &clients, nr_clients);
						//dupa ce se deconecteaza ,caut topicurile unde ar sf_active = 1 si retin in buffer_local
						close(i);//se inchide socketul clientului deconectat
						FD_CLR(i, &read_fds);// se scoate din multimea de citire socketul inchis 
					} else {
                        //verific daca mesajul e corect la client		
						if (msg_from_tcp.action){//subscribe case
							subscribe(&clients, nr_clients, msg_from_tcp, i);
						} else {// verifica daca era subscribed inainte
							//printf("client unsubscribed to %s %d\n", msg_from_tcp.topic, msg_from_tcp.store);
							unsubscribe(&clients, nr_clients, msg_from_tcp, i, msg_from_tcp.topic);
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
