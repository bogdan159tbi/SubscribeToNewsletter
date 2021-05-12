/*
*  	Protocoale de comunicatii: 
*  	Laborator 6: UDP
*	mini-server de backup fisiere
*/

#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "helpers.h"


void usage(char*file)
{
	fprintf(stderr,"Usage: %s server_port \n",file);
	exit(0);
}

/*
*	Utilizare: ./server server_port nume_fisier
*/
void create_file_bk(char *filename){
        char *tok = strtok(filename,".");
        strcat(tok,".bk");
        strcpy(filename,tok);
}
void receive_fcontent(struct sockaddr_in from_station, int sockfd){
        int fd;
	char buf[BUFLEN];
        socklen_t adr_len = sizeof(from_station);
        DIE(recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&from_station, &adr_len) <= 0,"read file name failed");
        create_file_bk(buf);
        DIE((fd=open(buf,O_WRONLY|O_CREAT|O_TRUNC,0666))==-1,"open file");
        size_t read_bytes;
        memset(buf,0,sizeof(buf));
        while ((read_bytes = 
                recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&from_station, &adr_len)) > 0) {
            DIE(write(fd, buf, read_bytes) <= 0, "write done/fail");
        }
        close(fd);

}
void parse_udp_msg(struct datagram *m ,char*buf){
        char *tok = strtok(buf," ");
        strcpy(m->topic, tok);
        tok  = strtok(NULL," " );
        strcpy(m->payload,tok);
}
int main(int argc,char**argv)
{
    if (argc <  2)
    	usage(argv[0]);
    
    struct sockaddr_in my_sockaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[1])),
        .sin_addr.s_addr = INADDR_ANY
    }  ;
    char buf[BUFLEN];
    /*Deschidere socket*/
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockfd < 0, "socket open fail");	
    struct datagram m;
    m.type = 1;
    strcpy(m.topic,"temperature");
    strcpy(m.payload,"e mare");
    while(1){
        socklen_t adr_len = sizeof(my_sockaddr);
        memset(buf,0, sizeof(buf));
        printf("ce conent sa trimiti la server: \n");
        read(STDIN_FILENO,buf, sizeof(buf));
        parse_udp_msg(&m, buf);
        int ret = sendto(sockfd, &m ,sizeof(struct datagram),0, (struct sockaddr*)&my_sockaddr, adr_len);
        DIE(ret <= 0, "failed");
        fprintf(stdout,"content sent: %s\n",m.payload);
    }
	/*Inchidere socket*/	
        close(sockfd);
	
	/*Inchidere fisier*/
        //close(fd);
	return 0;
}

