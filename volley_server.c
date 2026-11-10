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
    int server_sock, port_no, n;
    int client_socks[2];
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
       
    listen(server_sock, 2);
    printf("Server listening to port %d ...\n", port_no);
    
    for (int i = 0; i < 2; i++) {
        printf("Waiting for Player %d to connect...\n", i + 1);
        client_size = sizeof(client_addr);
        client_socks[i] = accept(server_sock, (struct sockaddr *) &client_addr, &client_size);
        if (client_socks[i] < 0) {
            die_with_error("Error: accept() Failed.");
        }
        printf("Player %d connected!\n", i + 1);

        int player_id = i;
        send(client_socks[i], &player_id, sizeof(int), 0);
    }

    GameState state;
    memset(&state, 0, sizeof(GameState));

    PlayerInput atk_input, def_input;
    state.attacker = 0;
    state.defender = 1;

    for (int i = 0; i < 2; i++) {
        send(client_socks[i], &state, sizeof(GameState), 0);
    }

    while(true){
        printf("\n--- SCORES | P1: %d  P2: %d ---\n", state.scores[0], state.scores[1]);
        printf("Waiting for Attacker (Player %d) to choose attack...\n", state.attacker + 1);

        // Receive attack input from the attacking client socket
        n = recv(client_socks[state.attacker], &atk_input, sizeof(PlayerInput), 0);
        if (n <= 0) {
            printf("Attacker disconnected.\n");
            break;
        }
        printf("[P%d] Executed Skill %d aiming Zone %d\n", 
            state.attacker + 1, atk_input.skill_id, atk_input.target_zone);

        printf("Waiting for Defender (Player %d) to choose defense...\n", state.defender + 1);

        // Receive defense input from the defending client socket
        n = recv(client_socks[state.defender], &def_input, sizeof(PlayerInput), 0);
        if (n <= 0) {
            printf("Defender disconnected.\n");
            break;
        }
        printf("[P%d] Guessed Zone %d\n", state.defender + 1, def_input.guess_zone);

        // Resolve turn logic
        if (atk_input.target_zone == def_input.guess_zone) {
            printf("--> Caught! Roles switch.\n");
            int temp = state.attacker;
            state.attacker = state.defender;
            state.defender = temp;
        } else {
            printf("--> Point for Player %d!\n", state.attacker + 1);
            state.scores[state.attacker]++;
        }

        // Broadcast updated state to both clients
        for (int i = 0; i < 2; i++) {
            send(client_socks[i], &state, sizeof(GameState), 0);
        }
    }

    printf("Closing connection ...\n");
   close(client_socks[0]);
    close(client_socks[1]);
    close(server_sock);
    
    return 0; 
}