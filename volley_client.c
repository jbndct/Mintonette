#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include "protocol.h" // references protocol.h header file

// prints error messages
void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

//READ INPUT
int read_int (const char *prompt, int min, int max) {
    char line[64];
    int value;
    printf("%s: ", prompt);
    while(1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n No more input. \n");
            exit(1);
        }

        if(sscanf(line, "%d", &value) == 1 && value >= min && value <= max) {
            return value;
        }

        printf("Invalid, enter a number between %d to %d: ", min, max);
    }
    
}

//ASK SKILLS
int ask_skill(int first, int last, int banned)
{
    int i;
    int choice;
    int id;

    for (i = first; i <= last; i++) {
        printf("%d. %s", i - first + 1, skills[i].name);
        if (i == banned) {
            printf(" (used last turn)");
        }
        printf("\n");
    }

    while (1) {
        choice = read_int("Pick a skill", 1, last - first + 1);
        id = first + (choice - 1);             
        if (id != banned) {            
            return id;
        }
        printf("You can't use the same skill twice in a row.\n");
    }
}

//CHOOSE WHICH ZONE TO ATTACK/DEFEND
int zone_in_skill(SkillId id, int zone) {
    int i;

    for(i=0; i<skills[id].target_count; i++) {
        if(skills[id].targets[i] == zone) {return 1;}
    }
    return 0;
}

//ASK ZONES
int ask_zone(SkillId id) {
    int zone;
    int j;
 
    while (1) {
        printf("Aim at one of:");
        for(j=0; j<skills[id].target_count; j++) {
            printf(" %d", skills[id].targets[j]);
        }
        printf("\n");

        zone = read_int("Aim at zone ", 1, 6);
        bool in_zone = zone_in_skill(id,zone);
        if (in_zone) {
            return zone;
        }
        else {
            printf("That skill can't reach that zone.\n");
        }
    }

}

//DISPLAY
void print_mintonette_text() {
    printf(" __  __ ___ _   _ _____ ___  _   _ _____ _____ _____ _____ \n");
    printf("|  \\/  |_ _| \\ | |_   _/ _ \\| \\ | | ____|_   _|_   _| ____|\n");
    printf("| |\\/| || ||  \\| | | || | | |  \\| |  _|   | |   | | |  _|  \n");
    printf("| |  | || || |\\  | | || |_| | |\\  | |___  | |   | | | |___ \n");
    printf("|_|  |_|___|_| \\_| |_| \\___/|_| \\_|_____| |_|   |_| |_____|\n");
}

void print_mintonette_ball() {
    printf("                     _______\n");
    printf("                  .-'       '-.\n");
    printf("                .'   \\  |  /   '.\n");
    printf("               /      \\ | /      \\\n");
    printf("              |________\\|/________|\n");
    printf("              |        /|\\        |\n");
    printf("               \\      / | \\      /\n");
    printf("                '.   /  |  \\   .'\n");
    printf("                  '-._______.-'\n");
}

void show_court(void)
{
    printf("+---------------------------+\n");
    
    printf("     +-----------------+     \n");
    printf("     |  1  |  2  |  3  |     \n");
    printf("     |-----------------|     \n");
    printf("     |  4  |  5  |  6  |     \n");
    printf("     +-----------------+     \n");
}

void print_scoreboard(int scores[2], int server)
{
    printf("\n+---------------------------+\n");
    printf("|     VOLLEYBALL MATCH      |\n");
    printf("+---------------------------+\n");
    printf("|   PLAYER 1   |  PLAYER 2  |\n");
    printf("|      %02d      |     %02d     |\n", scores[0], scores[1]);
    printf("+---------------------------+\n");
    printf("| SERVER: PLAYER %d          |\n", server + 1);
    printf("| SET: 1  |  IN PROGRESS    |\n");
    printf("+---------------------------+\n\n");
}


int main(int argc,  char *argv[]){
    // variable for socket connection, port number, error tracker
    int client_sock,  port_no, n;
    struct sockaddr_in server_addr; 
    struct hostent *server;

    // check if correct arguments are ran (./volley_client <IP> <Port>)
    if (argc < 3) {
        printf("Usage: %s hostname port_no\n",  argv[0]);
        exit(1);
    }
    
    port_no = (argc>=3) ? atoi(argv[2]) : PORT; // Default port if not provided

    print_mintonette_text();
    print_mintonette_ball();
    printf("\n\n\n\n");

    printf("Client starting ...\n");
    // create new TCP network socket
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

    // connect client to running server
    printf("Connecting to server at port %d...\n", port_no);
    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof server_addr) < 0)
        die_with_error("Error: connect() Failed.\n");
    printf("Connection successful! Waiting for game to start... \n");

    GameState state;
    PlayerInput my_input;
    int game_over = 0;

    while (!game_over) {
        //RECEIVE FROM SERVER
        n = recv(client_sock, &state, sizeof(GameState), 0);
        if (n <= 0) {
            printf("Server disconnected.\n");
            break;
        }

        memset(&my_input, 0, sizeof(PlayerInput)); //clear memory for memory input for current turn
        my_input.player_id = state.player_id; //turn the input to client (client is player 2 with player_id 1)

        switch (state.state_type) {
            case STATE_WAIT:
                print_scoreboard(state.scores, state.attacker-1);
                show_court();
                printf("You are Player %d.\n", state.player_id);
                if (state.attacker == state.player_id) {
                    printf("Waiting for the server...\n");
                } else {
                    printf("Player 1 is attacking...\n");
                }
                break;

            case STATE_PICK_SKILL: 
                int first, last;
                print_scoreboard(state.scores, state.attacker-1);
                show_court();
                if (state.rally_count == 0) {
                        printf("[YOUR TURN TO SERVE].\n");
                        first = SERVE_UNDERHAND;
                        last = SERVE_JUMP;
                } else {
                        printf("[YOUR TURN TO ATTACK].\n");
                        first = COUNTER_DEEP;
                        last = COUNTER_CUT;
                }

            my_input.skill_id = ask_skill(first, last, state.last_skill[state.player_id-1]);
            my_input.target_zone = ask_zone(my_input.skill_id);
            my_input.guess_zone = 0;

             //Send PlayerInput back to Server
            n = send(client_sock, &my_input, sizeof(PlayerInput), 0);
            if (n < 0) die_with_error("Error: send() Failed.");
            printf("Move sent! Waiting for server...\n");
            break;

            case STATE_PICK_DEFENSE:
            
            case STATE_RESULT:
            
            case STATE_GAME_OVER:



        }
    }    
    close (client_sock);
    return 0;
}


// show_court();
//         printf("\n--- SCORES | P1: %d  P2 (You): %d ---\n", state.scores[0], state.scores[1]);
//         printf("Player 1 has taken their move.\n");

//         //IF GAME OVER
//         if ((state.scores[0] < 10) && (state.scores[1] < 10)){
//             if (state.scores[0] == 10) {
//                 printf("Player %d wins \n", 1);
//             }
//             else if (state.scores[1] == 10) {
//                 printf("Player %d wins \n", 2);
//             }
//         }

//        
            
//         } else if (state.defender == 1) {
//             printf("[YOUR TURN TO DEFEND].\n");
//             printf("Opponent attacked! Guess zone to protect (1-6):\n");
//             my_input.guess_zone = read_int("Zone to protect", 1, 6);
//             my_input.skill_id = 0;
//             my_input.target_zone = 0;

//             if (send(client_sock, &my_input, sizeof(PlayerInput), 0) < 0) {
//                 die_with_error("Error: send() Failed.");
//             }
//             printf("Defense choice sent! Waiting for server resolution...\n");


//         } else {
//             printf("Waiting for Player 1 to finish their turn...\n");
//         }
//     }
//     close(client_sock);