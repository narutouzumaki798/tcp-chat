#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <ncurses.h>
#include <sys/mman.h>
#include <semaphore.h>


#include "util.h"

// network
char user_name[50];
char buffer[1024];
int sockfd, portno, n;
struct sockaddr_in serv_addr;
struct hostent *server;

void setup_connection(int argc, char* argv[])
{
    const char* target_serv_addr = "192.168.0.104";
    const char* target_portno = "8001";

    if (argc >= 2)
	target_serv_addr = argv[1];
    if (argc >= 3)
	target_portno = argv[2];

    portno = atoi(target_portno);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(target_serv_addr);
    if (server == NULL) {
        error("ERROR, no such host\n");
        exit(0);
    }

    fill_zero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
}




int main(int argc, char* argv[])
{
    setup_connection(argc, argv);
    copy2(buffer, "aaa");
    while(1)
    {
	sleep(1);
	write(sockfd, buffer, strlen(buffer));
    }
    return 0;
}


