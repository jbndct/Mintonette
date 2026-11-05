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


typedef enum {
    SERVE_UNDERHAND,
    SERVE_SHORT,
    SERVE_OVERHAND,
    SERVE_JUMP,
    COUNTER_DEEP,
    COUNTER_DINK,
    COUNTER_SPIKE,
    COUNTER_CUT,
    SKILL_COUNT
} SkillId;

typedef enum {
    EXACT, 
    NEIGHBOR
} CatchRule;

typedef struct {
    const char *name;
    int targets[MAX_TARGETS];
    int target_count;
    CatchRule catch_rule;
    int fault_chance;
} Skill;

Skill skills[SKILL_COUNT] = {
    {"Underhand Serve", {3,4}, 2, NEIGHBOR, 5},
    {"Short Serve", {2,3}, 2, EXACT, 5},
    {"Overhand Serve", {1,3,5}, 3, EXACT, 20},
    {"Jump Serve", {1,3,4,6}, 4, EXACT, 30},
    {"Deep Return", {1,3,4,6}, 4, NEIGHBOR, 5},
    {"Dink", {1,2,3}, 3, EXACT, 20},
    {"Spike", {2,4,5}, 3, EXACT, 25},
    {"Cut Shot", {3,5,6}, 3, EXACT, 20}
};

void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

//FOR SOCKET
//RECEIVE

//SEND

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

//SCORE/UPDATES

//WINNER


int main(int argc,  char *argv[]){
    int client_sock,  port_no, n;
    struct sockaddr_in server_addr;
    struct hostent *server;

    if (argc < 3) {
        printf("Usage: %s hostname port_no",  argv[0]);
        exit(1);
    }

    print_mintonette_text();
    print_mintonette_ball();
    printf("\n\n\n\n");

    printf("Player 2 (client) starting ...\n");
    // Create a socket using TCP
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

    printf("Connecting to Player 1 at port %d...\n", port_no);
    if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
        die_with_error("Error: connect() Failed.");

    printf("Connection successful! You are Player 2! \n");

    GameState state;
    PlayerInput my_input;

    while (true) {
        //RECEIVE FROM SERVER
        n = recv(client_sock, &state, sizeof(GameState), 0);
        if (n <= 0) {
            printf("Server disconnected.\n");
            break;
        }

        memset(&my_input, 0, sizeof(PlayerInput)); //clear memory for memory input for current turn
        my_input.player_id = 2; //turn the input to client (client is player 2 with player_id 1)

        show_court();
        printf("\n--- SCORES | P1: %d  P2 (You): %d ---\n", state.scores[0], state.scores[1]);
        printf("Player 1 has taken their move.\n");

        //IF GAME OVER
        if ((state.scores[0] < 10) && (state.scores[1] < 10)){
            if (state.scores[0] == 10) {
                printf("Player %d wins \n", 1);
            }
            else if (state.scores[1] == 10) {
                printf("Player %d wins \n", 2);
            }
        }

        //STATE FOR CLIENT
        int first, last;
        if (state.attacker == 1) {
            if (state.rally_count == 0) {
                printf("[YOUR TURN TO SERVE].\n");
                first = SERVE_UNDERHAND;
                last = SERVE_JUMP;
            } else {
            printf("[YOUR TURN TO ATTACK].\n");
            first = COUNTER_DEEP;
            last = COUNTER_CUT;
            }

            my_input.skill_id = ask_skill(first, last, state.last_skill[1]);
            my_input.target_zone = ask_zone(my_input.skill_id);
            my_input.guess_zone = 0;

             // 3. Send Player 2 input back to Server
            n = send(client_sock, &my_input, sizeof(PlayerInput), 0);
            if (n < 0) die_with_error("Error: send() Failed.");
            printf("Move sent! Waiting for Player 1...\n");
            
        } else if (state.defender == 1) {
            printf("[YOUR TURN TO DEFEND].\n");
            printf("Opponent attacked! Guess zone to protect (1-6):\n");
            my_input.guess_zone = read_int("Zone to protect", 1, 6);
            my_input.skill_id = 0;
            my_input.target_zone = 0;

            if (send(client_sock, &my_input, sizeof(PlayerInput), 0) < 0) {
                die_with_error("Error: send() Failed.");
            }
            printf("Defense choice sent! Waiting for server resolution...\n");


        } else {
            printf("Waiting for Player 1 to finish their turn...\n");
        }
    }
    close(client_sock);
    
    return 0;
}
