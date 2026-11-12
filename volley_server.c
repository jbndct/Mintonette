#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
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
    state.last_skill[0] = -1; //no skill used yet
    state.last_skill[1] = -1;

    for (int i = 0; i < 2; i++) {
        state.player_id = i + 1;
        state.state_type = (i==0) ? STATE_PICK_SKILL : STATE_WAIT;
        send(client_socks[i], &state, sizeof(GameState), 0);
    }
    printf("Both players connected. States sent.\n");


    srand((unsigned)time(NULL));//seed once (fault rolls)
 
    GameState state;
    bzero(&state, sizeof(GameState));          
    state.server = 1;                            //P1 serves first by default
    PlayerInput atk_in, def_in;                  
    int quit = 0;                                //allow players to quit if one disconnects

    while (!quit && state.scores[0] < WIN_SCORE && state.scores[1] < WIN_SCORE) {   
        state.attacker = state.server;
        state.defender = 3 - state.server;
        state.rally_count = 0;
        state.last_skill[0] = -1;
        state.last_skill[1] = -1;
        state.point_winner = 0;
        int rally_over = 0;
 
        while (!rally_over && !quit) {           
            int a = state.attacker - 1;          
            int d = state.defender - 1;          
            int swap_roles = 0;
 
            // 1. ATTACKER, defender waits
            state.was_caught = 0;
            state.fault_occurred = 0;
            for (int i = 0; i < 2; i++) {
                state.player_id = i + 1;
                state.state_type = (i == a) ? STATE_PICK_SKILL : STATE_WAIT;
                send(client_socks[i], &state, sizeof(GameState), 0);
            }
 
            n = recv(client_socks[a], &atk_in, sizeof(PlayerInput), 0);
            if (n <= 0) {                                  // CHANGED: was goto done
                printf("Player %d disconnected.\n", a + 1);
                quit = 1;
                break;
            }
 
            //to check if the input was valid, if not, quit the game and notify the other player
            int id = atk_in.skill_id;
            int first = (state.rally_count == 0) ? SERVE_UNDERHAND : COUNTER_DEEP;
            int last  = (state.rally_count == 0) ? SERVE_JUMP : COUNTER_CUT;
            int zone_ok = 0;
            if (id >= first && id <= last)
                for (int j = 0; j < skills[id].target_count; j++)
                    if (skills[id].targets[j] == atk_in.target_zone) zone_ok = 1;
            if (!zone_ok || id == state.last_skill[a]) {
                printf("Invalid input from Player %d.\n", a + 1);
                quit = 1;                                  
                break;
            }
 
            state.last_skill[a] = id;
            state.last_action_skill = id;
            printf("Player %d used: %s (landing %d)\n", a + 1, skills[id].name, atk_in.target_zone);
 
            // 2. for determining fault chance based on the skill 
            if (rand() % 100 + 1 <= skills[id].fault_chance) {
                state.fault_occurred = 1;
                state.point_winner = state.defender;
                rally_over = 1;
            } else {
                // 3. DEFENDER, attacker waits
                for (int i = 0; i < 2; i++) {
                    state.player_id = i + 1;
                    state.state_type = (i == d) ? STATE_PICK_DEFENSE : STATE_WAIT;
                    send(client_socks[i], &state, sizeof(GameState), 0);
                }
                n = recv(client_socks[d], &def_in, sizeof(PlayerInput), 0);
                if (n <= 0) {                              
                    printf("Player %d disconnected.\n", d + 1);
                    quit = 1;
                    break;
                }
 
                // 4. if CAUGHT/MISSED
                int landing = atk_in.target_zone;
                int guess = def_in.guess_zone;
                int caught = (landing == guess);
                if (!caught && skills[id].catch_rule == NEIGHBOR && guess >= 1 && guess <= 6) {
                    int row_gap = abs((guess - 1) / 3 - (landing - 1) / 3);
                    int col_gap = abs((guess - 1) % 3 - (landing - 1) % 3);
                    if (row_gap + col_gap == 1) caught = 1;
                }
 
                state.landing_zone = landing;
                state.defender_guess = guess;
                state.was_caught = caught;
 
                if (caught) {
                    swap_roles = 1;              // take turns AFTER the result is sent, 
                } else {
                    state.point_winner = state.attacker;
                    rally_over = 1;
                }
            }
 
            // 5. score, update server, send result to both
            if (state.point_winner != 0) {
                state.scores[state.point_winner - 1]++;
                state.server = state.point_winner;   // point winner serves next
            }
 
            for (int i = 0; i < 2; i++) {
                state.player_id = i + 1;
                state.state_type = STATE_RESULT;
                send(client_socks[i], &state, sizeof(GameState), 0);
            }
 
            // 6. For rally
            if (swap_roles) {
                int t = state.attacker;
                state.attacker = state.defender;
                state.defender = t;
                state.rally_count++;
            }
        }
    }
 
    // for ending game
    if (!quit) {                                           
        state.point_winner = (state.scores[0] >= WIN_SCORE) ? 1 : 2;
        for (int i = 0; i < 2; i++) {
            state.player_id = i + 1;
            state.state_type = STATE_GAME_OVER;
            send(client_socks[i], &state, sizeof(GameState), 0);
        }
    }

    printf("Closing connection ...\n");
    close(client_socks[0]);
    close(client_socks[1]);
    close(server_sock);
    
    return 0; 
}