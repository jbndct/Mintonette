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
    client_sock = socket(AF_INET,  SOCK_STREAM,  0);
    if (client_sock < 0) 
        die_with_error("Error: socket() Failed.");

    printf("Looking for host '%s'...\n", argv[1]);
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        die_with_error("Error: No such host.");
    }
    printf("Host found!\n");

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

    int my_id;
    n = recv(client_sock, &my_id, sizeof(int), 0);
    if (n <= 0) {
        die_with_error("Failed to receive player ID assignment from server.");
    }

    printf("Connection successful! You are Player %d!\n", my_id + 1);
    
    GameState state;
    memset(&state, 0, sizeof(GameState));
    PlayerInput my_input;

    while (true) {
        n = recv(client_sock, &state, sizeof(GameState), 0);
        if (n <= 0) {
            printf("Server disconnected.\n");
            break;
        }

        my_input.player_id = my_id;

        if (state.state_type == 1) {
            // --- ATTACK PHASE ---
            if (my_id == state.attacker) {
                printf("\n--- SCORES | P1: %d  P2: %d ---\n", state.scores[0], state.scores[1]);
                printf("[YOUR TURN TO ATTACK (Player %d)]\n", my_id + 1);
                printf("Pick Skill (1-8): ");
                scanf("%d", &my_input.skill_id);
                printf("Aim Zone (1-6): ");
                scanf("%d", &my_input.target_zone);

                n = send(client_sock, &my_input, sizeof(PlayerInput), 0);
                if (n < 0) die_with_error("Error: send() Failed.");
                printf("Attack sent! Waiting for opponent to defend...\n");
            } else {
                printf("\nWaiting for Player %d to attack...\n", state.attacker + 1);
            }
        } 
        else if (state.state_type == 2) {
            // --- DEFENSE PHASE ---
            if (my_id == state.defender) {
                printf("\n[YOUR TURN TO DEFEND (Player %d)]\n", my_id + 1);
                printf("Incoming Skill %d! \n", state.last_action_skill);
                printf("Pick Zone to Protect (1-6): ");
                scanf("%d", &my_input.guess_zone);

                n = send(client_sock, &my_input, sizeof(PlayerInput), 0);
                if (n < 0) die_with_error("Error: send() Failed.");
                printf("Defense choice sent! Waiting for turn result...\n");
            } else {
                printf("Waiting for Player %d to defend...\n", state.defender + 1);
            }
        }
        else if (state.state_type == 3) {
            // --- TURN RESULT PHASE ---
            printf("\n=== RALLY RESULT ===\n");
            if (state.was_caught) {
                printf("CAUGHT! The defender guessed correctly. Roles switch!\n");
            } else {
                printf("MISSED! Landed in zone %d.\n", state.landing_zone);
                printf("Player %d wins the point!\n", state.point_winner + 1);
            }
            printf("CURRENT SCORES | Player 1: %d  Player 2: %d\n", state.scores[0], state.scores[1]);
            printf("====================\n");
        }
    }
    close(client_sock);
    
    return 0;
}