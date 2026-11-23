#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

int main(int argc,  char *argv[]){
    
    //DEFINING VARIABLES
    int client_sock,  port_no,  n;
    struct sockaddr_in server_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
        printf("Usage: %s hostname port_no",  argv[0]);
        exit(1);
    }

    //CREATE SOCKET
    client_sock = socket(AF_INET,  SOCK_STREAM,  0);
    if (client_sock < 0) 
        die_with_error("Error: socket() Failed.");

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        die_with_error("Error: No such host.");
    }
    
    //CONNECTION TO SERVER
    port_no = atoi(argv[2]);
    bzero((char *) &server_addr,  sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,  
         (char *)&server_addr.sin_addr.s_addr, 
         server->h_length);
         
    server_addr.sin_port = htons(port_no);

    //GAME LOADING?
    printf("Connecting to server at port %d...\n", port_no);
    if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
        die_with_error("Error: connect() Failed.");

    //GAME MENU STARTS
    printf("Connection successful!\n");

    // COMMUNICATE

    //SHOW DETAILS
    //You are player 2, server is player 1 
    //Start game? (y/n)

    //WHILE LOOP FOR GAMEPLAY
    //WAIT FOR THE ATTACK OF PLAYER 1

    bzero(buffer, 256);
    n = recv(client_sock, buffer, 255, 0);
    if (n < 0) 
         die_with_error("Error: recv() Failed.");

    //SHOW THE ATTACK OF PLAYER 1
    printf("Server says : %s\n", buffer);

    //PLAYER 2 COUNTERATTACK
    printf("Please type a message : ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);

    //SEND THE COUNTERATTACK TO PLAYER 1
    n = send(client_sock, buffer, strlen(buffer), 0);
    if (n < 0) 
         die_with_error("Error: send() Failed.");

    
    //IF A PLAYER WINS 10 POINTS, PROMPT CLIENT & SERVER TO PLAY AGAIN OR QUIT GAME
    
    if (scores[0] == 10) {
        printf("Player %d wins \n", 1);
    }
    else if (scores[1] == 10) {
        printf("Player %d wins \n", 2);
    }

    close(client_sock);
    
    return 0;
}
