#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include "protocol.h" // references protocol.h header file

// prints error messages
void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

int main(int argc, char *argv[]){
    int server_sock, port_no, n;
    int client_socks[2];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size;

    // check if correct arguments are ran (./volley_server <Port>)
    if (argc < 2) {
        printf("Usage: %s port_no\n", argv[0]);
        exit(1);
    }

    port_no = atoi(argv[1]);

    printf("Server starting ...\n");

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) 
       die_with_error("Error: socket() Failed.\n");

    bzero((char *) &server_addr, sizeof(server_addr));

    int yes = 1; // Allow reuse of the address
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); //changes the server socket to allow reuse of the address

    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(port_no); 
    
    // bind socket to port number
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
       die_with_error("Error: bind() Failed.\n");
    
    // start listening to connections from 2 clients
    listen(server_sock, 2);
    printf("Server listening to port %d ...\n", port_no);
    
    // accept two players, first is Player 1, then Player 2
    for (int i = 0; i < 2; i++) {
        printf("Waiting for Player %d to connect...\n", i + 1);
        client_size = sizeof(client_addr);
        client_socks[i] = accept(server_sock, (struct sockaddr *) &client_addr, &client_size);
        if (client_socks[i] < 0) {
            die_with_error("Error: accept() Failed.\n");
        }
        printf("Player %d connected!\n", i + 1);
    }

    // initialize game state with zeroes, tell which player is which
    GameState state;
    memset(&state, 0, sizeof(GameState));
    state.attacker = 1;
    state.defender = 2;
    state.rally_count = 0;
    state.rally_count = 0;
    state.last_skill[0] = -1; //no skill used yet
    state.last_skill[1] = -1;

    for (int i = 0; i < 2; i++) {
        state.player_id = i + 1;
        state.state_type = (i==0) ? STATE_PICK_SKILL : STATE_WAIT;
        send(client_socks[i], &state, sizeof(GameState), 0);
    }
    printf("Both players connected. States sent.\n");

    PlayerInput p1_input;
    n = recv(client_socks[0], &p1_input, sizeof(PlayerInput), 0);
    if (n <= 0 || p1_input.skill_id < 0 || p1_input.skill_id >= SKILL_COUNT) {
        printf("Missing input from Player 1.\n");
        return 1;
    }

    int id = p1_input.skill_id;
    printf("Player %d used: %s\n", state.attacker, skills[id].name);
    printf("Landing Zone: %d\n", p1_input.target_zone);

    state.last_action_skill = id;
    for (int i = 0; i < 2; i++) {
        state.player_id = i + 1;
        state.state_type = (i == 1) ? STATE_PICK_DEFENSE : STATE_WAIT;
        send(client_socks[i], &state, sizeof(GameState), 0);
    }

    PlayerInput p2_input;                                   // NEW
    n = recv(client_socks[1], &p2_input, sizeof(PlayerInput), 0);   // NEW
    if (n > 0) {
        printf("[P2] guess_zone=%d\n", p2_input.guess_zone);
        /* ...the rest of your if-block stays exactly as it is... */
    }

    printf("Closing connection ...\n");
    close(client_socks[0]);
    close(client_socks[1]);
    close(server_sock);
    
    return 0; 
}