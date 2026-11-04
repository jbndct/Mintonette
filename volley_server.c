#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include "protocol.h"

void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

int main(int argc, char *argv[]){
    int server_sock, client_sock, port_no, n;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size;

    if (argc < 2) {
        printf("Usage: %s port_no", argv[0]);
        exit(1);
    }

    port_no = atoi(argv[1]);

    printf("Server starting ...\n");

    // Create a socket for incoming connections
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) 
       die_with_error("Error: socket() Failed.");
       
    // Bind socket to a port
    bzero((char *) &server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET; // Internet address 
    server_addr.sin_addr.s_addr = INADDR_ANY; // Any incoming interface
    server_addr.sin_port = htons(port_no); // Local port
    
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
       die_with_error("Error: bind() Failed.");
       
    // Mark the socket so it will listen for incoming connections
    listen(server_sock, 1);
    printf("Server listening to port %d ...\n", port_no);
    
    printf("Waiting for Player 2 to connect...\n");

    // Accept new connection
    client_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_size);
    if (client_sock < 0) 
        die_with_error("Error: accept() Failed.");
    printf("Player 2 succesfully connected! ...\n");  
    
    GameState state;
    memset(&state, 0, sizeof(GameState));

    PlayerInput p1_input, p2_input;

    while(true){
        
        printf("\n--- SCORES | P1 (Server): %d  P2 (Client): %d ---\n", state.scores[0], state.scores[1]);
        

        printf("[P1 - YOU] Pick Skill (1-8): ");
        scanf("%d", &p1_input.skill_id);
        printf("[P1 - YOU] Aim Zone (1-6): ");
        scanf("%d", &p1_input.target_zone);

        printf("Waiting for Player 2 to respond...\n");

        send(client_sock, &state, sizeof(GameState), 0);

        n = recv(client_sock, &p2_input, sizeof(PlayerInput), 0);
        if (n <= 0) {
            printf("Player 2 disconnected.\n");
            break;
        }

        printf("[P2 - CLIENT] Guessed Zone: %d\n", p2_input.guess_zone);

        if (p1_input.target_zone == p2_input.guess_zone) {
            printf("--> Caught by Player 2!\n");
        } else {
            printf("--> Point for Player 1!\n");
            state.scores[0]++;
        }
    }

    printf("Closing connection ...\n");
    close(client_sock);
    close(server_sock);
    
    return 0; 
}