#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include "protocol.h"

void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

int main(int argc,  char *argv[]){
    int client_sock,  port_no, n;
    struct sockaddr_in server_addr;
    struct hostent *server;

    if (argc < 3) {
        printf("Usage: %s hostname port_no",  argv[0]);
        exit(1);
    }

    printf("Player 2 (client) starting ...\n");
    // Create a socket using TCP
    client_sock = socket(AF_INET,  SOCK_STREAM,  0);
    if (client_sock < 0) 
        die_with_error("Error: socket() Failed.");

    printf("Looking for host '%s'...\n", argv[1]);
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        die_with_error("Error: No such host.");
    }
    printf("Host found!\n");

    // Establish a connection to server
    port_no = atoi(argv[2]);
    bzero((char *) &server_addr,  sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,  
         (char *)&server_addr.sin_addr.s_addr, 
         server->h_length);
         
    server_addr.sin_port = htons(port_no);

    printf("Connecting to Player 1 at port %d...\n", port_no);
    if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
        die_with_error("Error: connect() Failed.");

    printf("Connection successful! You are Player 2! \n");
    
    
    GameState state;
    PlayerInput my_input;

    while (true) {
        n = recv(client_sock, &state, sizeof(GameState), 0);
        if (n <= 0) {
            printf("Server disconnected.\n");
            break;
        }

        printf("\n--- SCORES | P1: %d  P2 (You): %d ---\n", state.scores[0], state.scores[1]);
        printf("Player 1 has taken their move.\n");

        my_input.player_id = 1; 
        printf("[P2 - YOU] Pick Zone to Protect (1-6): ");
        scanf("%d", &my_input.guess_zone);

        // 3. Send Player 2 input back to Server
        n = send(client_sock, &my_input, sizeof(PlayerInput), 0);
        if (n < 0) die_with_error("Error: send() Failed.");

        printf("Move sent! Waiting for Player 1...\n");
}
    close(client_sock);
    
    return 0;
}