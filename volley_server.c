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

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) 
       die_with_error("Error: socket() Failed.");

    bzero((char *) &server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(port_no); 
    
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
       die_with_error("Error: bind() Failed.");
       
    listen(server_sock, 1);
    printf("Server listening to port %d ...\n", port_no);
    
    printf("Waiting for Player 2 to connect...\n");

    client_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_size);
    if (client_sock < 0) 
        die_with_error("Error: accept() Failed.");
    printf("Player 2 succesfully connected! ...\n");  
    
    GameState state;
    memset(&state, 0, sizeof(GameState));

    PlayerInput atk_input, def_input;
    state.attacker = 0;
    state.defender = 1;

    while(true){
        printf("\n--- SCORES | P1 (Server): %d  P2 (Client): %d ---\n", state.scores[0], state.scores[1]);

        if (state.attacker == 0) {
            // === SERVER (P1) ATTACKS, CLIENT (P2) DEFENDS ===
            printf("\n[YOU ARE ATTACKING]\n");
            printf("Pick Skill (1-8): ");
            scanf("%d", &atk_input.skill_id);
            printf("Aim Zone (1-6): ");
            scanf("%d", &atk_input.target_zone);

            // Send state ONLY ONCE, after P1 makes their turn choices
            send(client_sock, &state, sizeof(GameState), 0);
            printf("Waiting for Player 2 to pick defense zone...\n");

            n = recv(client_sock, &def_input, sizeof(PlayerInput), 0);
            if (n <= 0) break;

            printf("[P2 - CLIENT] Guessed Zone: %d\n", def_input.guess_zone);

            if (atk_input.target_zone == def_input.guess_zone) {
                printf("--> Caught by Player 2! Roles switch.\n");
                state.attacker = 1;
                state.defender = 0;
            } else {
                printf("--> Point for Player 1!\n");
                state.scores[0]++;
            }
        } 
        else {
            // === CLIENT (P2) ATTACKS, SERVER (P1) DEFENDS ===
            // Notify client that it's their turn to attack
            send(client_sock, &state, sizeof(GameState), 0);
            printf("\n[PLAYER 2 IS ATTACKING] Waiting for client move...\n");
            
            n = recv(client_sock, &atk_input, sizeof(PlayerInput), 0);
            if (n <= 0) break;

            printf("[P2 - CLIENT] Executed Skill %d aiming Zone %d\n", atk_input.skill_id, atk_input.target_zone);

            printf("[YOU ARE DEFENDING] Pick Zone to Protect (1-6): ");
            scanf("%d", &def_input.guess_zone);

            if (atk_input.target_zone == def_input.guess_zone) {
                printf("--> Caught by Player 1! Roles switch.\n");
                state.attacker = 0;
                state.defender = 1;
            } else {
                printf("--> Point for Player 2!\n");
                state.scores[1]++;
            }
        }
    }

    printf("Closing connection ...\n");
    close(client_sock);
    close(server_sock);
    
    return 0; 
}