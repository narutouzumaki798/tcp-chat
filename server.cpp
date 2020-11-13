#include <stdio.h>  
#include <string.h>
#include <stdlib.h>  
#include <errno.h>  
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros  
#include <unordered_map>
#include <vector>
#include <iostream>
#include <string>

using namespace std;

#include "util.h"
     
#define TRUE   1  
#define FALSE  0  
#define PORT 8001  

int opt = TRUE;   
int master_socket, addrlen, new_socket, client_socket[30],
    max_clients = 30, activity, i, valread, sd;
int max_sd;
struct sockaddr_in address;
char buffer[1025];  //data buffer of 1K
fd_set readfds;   //set of socket descriptors


unordered_map<int, string> sock_map;
vector<int> sock_list;


void setup_connection()
{
    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    //create a master socket  
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)   
    {   
        error("socket failed");   
        exit(EXIT_FAILURE);   
    }   
    //set master socket to allow multiple connections  
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt,  
          sizeof(opt)) < 0 )   
    {   
        error("setsockopt");   
        exit(EXIT_FAILURE);   
    }   
     
    //type of socket created  
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );   
         
    //bind the socket to localhost port 8888  
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)   
    {   
        error("bind failed");   
        exit(EXIT_FAILURE);   
    }   
    //try to specify maximum of 3 pending connections for the master socket  
    if (listen(master_socket, 3) < 0)   
    {   
        error("listen");   
        exit(EXIT_FAILURE);   
    }   
         
    addrlen = sizeof(address);   
}



string char_to_string(char* a)
{
    string ans = ""; int i = 0;
    while(a[i] != '\0') { ans += a[i]; i++; }
    return ans;
}


bool exists(string a) // check if username is already present
{
    if(a == "you") return true; // reserved word
    for(int i=0; i<sock_list.size(); i++)
    {
	int sd = sock_list[i];
	if(sock_map[sd] == a) return true;
    }
    return false;
}

void add_user(int sd)
{
    string uname;
    while(1) // checking for valid user name
    {
	int n = read(sd, buffer, 1024);
	buffer[n] = '\0';  // buffer e age 0 na bhore nile eta korte hobe
	uname = char_to_string(buffer);
	if(exists(uname))
	    write(sd, "no", 3);
	else
	{
	    write(sd, "ok", 3);
	    break;
	}
    }
    usleep(1000000); // wait for client to receive ok

    printf("added user %s\n", uname.c_str());
    sock_map[sd] = uname;
    sock_list.push_back(sd);

    string message;
    for(int i=0; i<sock_list.size(); i++)
    {
	if(sock_list[i] == sd)
	{
	    message = "       you joined as " + uname + "\n\n";
	}
	else
	{
	    message = "       " + uname + " joined\n\n"; 
	}
	write(sock_list[i], message.c_str(), message.size());
    }
}

void remove_user(int sd)
{
    string uname = sock_map[sd];
    vector<int> tmp;
    for(int i=0; i<sock_list.size(); i++)
	if(sock_list[i] != sd) tmp.push_back(sock_list[i]);
    sock_list.clear();
    for(int i=0; i<tmp.size(); i++)
	sock_list.push_back(tmp[i]);

    string message;
    for(int i=0; i<sock_list.size(); i++)
    {
        message = "       " + uname + " disconnected\n\n"; 
	write(sock_list[i], message.c_str(), message.size());
    }
}

void send_all(int sd) // send_all marker
{
    string uname = sock_map[sd];
    string message;
    for(int i=0; i<sock_list.size(); i++)
    {
	if(sock_list[i] == sd)
	    message = "you: " + char_to_string(buffer) + "\n\n";
	else
	    message = uname + ": " + char_to_string(buffer) + "\n\n";
	write(sock_list[i], message.c_str(), message.size());
     }
     printf("server: sending message from %s\n", uname.c_str());
}


void recheck_sockets()
{
        //clear the socket set  
        FD_ZERO(&readfds);   
     
        //add master socket to set  
        FD_SET(master_socket, &readfds);   
        max_sd = master_socket;   
             
        //add child sockets to set  
        for ( i = 0 ; i < max_clients ; i++)   
        {   
            //socket descriptor  
            sd = client_socket[i];   
                 
            //if valid socket descriptor then add to read list  
            if(sd > 0)   
                FD_SET(sd , &readfds);   
                 
            //highest file descriptor number, need it for the select function  
            if(sd > max_sd)   
                max_sd = sd;   
        }   
     
        //wait for an activity on one of the sockets , timeout is NULL ,  
        //so wait indefinitely  
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
       
        /* if ((activity < 0) && (errno!=EINTR))    */
        /* {    */
        /*     printf("select error");    */
        /* }    */
             
        //If something happened on the master socket,
        //then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {   
            if ((new_socket = accept(master_socket,  
	       (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) // accept the new connection
            {   
                error("accept");   
                exit(EXIT_FAILURE);   
            }   
             
            printf("new connection:   socket-fd: %d   ip: %s\n",
		   new_socket , inet_ntoa(address.sin_addr));

	    add_user(new_socket);

            //add new socket to array of sockets  
            for (i = 0; i < max_clients; i++)   
            {   
                //if position is empty  
                if( client_socket[i] == 0 )   
                {   
                    client_socket[i] = new_socket;   
                    break;   
                }   
            }   
        }   
        //else its some IO operation on some other socket 
        for (i = 0; i < max_clients; i++)   
        {   
            sd = client_socket[i];   
            if (FD_ISSET(sd , &readfds))   
            {   
                //check if it was for closing , and also read the  
                //incoming message  
		valread = read(sd, buffer, 1024);
                if (valread == 0)   
                {
                    //some user disconnected 
                    printf("user %s disconnected. socket-fd = %d\n", sock_map[sd].c_str(), sd);
		    remove_user(sd);
                         
                    //close the socket and mark as 0 in list for reuse  
                    close(sd);   
                    client_socket[i] = 0;   
                }   
                else 
                {   
                    //set the string terminating NULL byte on the end  
                    //of the data read  
                    buffer[valread] = '\0';   
		    send_all(sd);
                    // send(sd , buffer , strlen(buffer) , 0 );   
                }   
            }   
        }   
}



int main(int argc , char *argv[])   
{   
    setup_connection();
    while(1)   
    {   
	recheck_sockets();
    }   
    return 0;   
}   
