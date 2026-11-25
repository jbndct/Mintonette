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

void die_with_error(const char *error_msg)
{
    fprintf(stderr, "%s\n", error_msg);
    exit(EXIT_FAILURE);
}

void wait_for_enter(int client_sock)
{
    char line[64];
    printf("\nPress ENTER to continue...");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
    {
        printf("\nInput stream closed. Disconnecting...\n");
        close(client_sock);
        exit(EXIT_SUCCESS);
    }
}

int read_int(const char *prompt, int min, int max, int client_sock)
{
    char line[64];
    int value;
    printf("%s: ", prompt);
    fflush(stdout);

    while (1)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("\nInput stream closed. Disconnecting...\n");
            close(client_sock);
            exit(EXIT_SUCCESS);
        }

        if (sscanf(line, "%d", &value) == 1 && value >= min && value <= max)
        {
            return value;
        }

        printf("Invalid input. Enter a number between %d to %d: ", min, max);
        fflush(stdout);
    }
}

int ask_skill(int first, int last, int banned, int client_sock)
{
    int choice, id;

    printf("\nChoose your technique:\n");
    for (int i = first; i <= last; i++)
    {
        printf("[%d] %s", i - first + 1, skills[i].name);
        if (i == banned)
        {
            printf(" (Used last turn - Banned)");
        }
        printf("\n");
    }

    while (1)
    {
        choice = read_int("Select option", 1, last - first + 1, client_sock);
        id = first + (choice - 1);
        if (id != banned)
        {
            return id;
        }
        printf("You can't use the same skill twice in a row. Choose another.\n");
    }
}

int ask_zone(SkillId id, int client_sock)
{
    printf("\nChoose target zone:\n");
    for (int j = 0; j < skills[id].target_count; j++)
    {
        printf("[%d] Zone %d\n", j + 1, skills[id].targets[j]);
    }

    int choice = read_int("Aim at option", 1, skills[id].target_count, client_sock);
    return skills[id].targets[choice - 1];
}

int ask_defense_zone(SkillId id, int client_sock)
{
    printf("\nChoose a defensive zone to protect:\n");
    for (int j = 0; j < skills[id].target_count; j++)
    {
        printf("[%d] Zone %d\n", j + 1, skills[id].targets[j]);
    }

    int choice = read_int("Protect option", 1, skills[id].target_count, client_sock);
    return skills[id].targets[choice - 1];
}

void print_mintonette_text(void)
{
    printf(" __  __ ___ _   _ _____ ___  _   _ _____ _____ _____ _____ \n");
    printf("|  \\/  |_ _| \\ | |_   _/ _ \\| \\ | | ____|_   _|_   _| ____|\n");
    printf("| |\\/| || ||  \\| | | || | | |  \\| |  _|   | |   | | |  _|  \n");
    printf("| |  | || || |\\  | | || |_| | |\\  | |___  | |   | | | |___ \n");
    printf("|_|  |_|___|_| \\_| |_| \\___/|_| \\_|_____| |_|   |_| |_____|\n");
}

void print_mintonette_ball(void)
{
    printf("                        _______\n");
    printf("                     .-'       '-.\n");
    printf("                   .'   \\  |  /   '.\n");
    printf("                  /      \\ | /      \\\n");
    printf("                 |________\\|/________|\n");
    printf("                 |        /|\\        |\n");
    printf("                  \\      / | \\      /\n");
    printf("                   '.   /  |  \\   .'\n");
    printf("                     '-._______.-'\n");
}

void print_skill_guide(void)
{
    printf("\n==================================================\n");
    printf("                SKILL REFERENCE GUIDE             \n");
    printf("==================================================\n");
    printf("--- SERVE SKILLS (Opening Attack) ---\n");
    printf("1. Underhand Serve | Fault:  5%% | Target: Zones 3, 4\n");
    printf("   Hint: \"Free ball yan oh!\" (Forgiving defense)\n\n");
    printf("2. Short Serve     | Fault:  5%% | Target: Zones 2, 3\n");
    printf("   Hint: \"Yung unahan bantayan mo!\"\n\n");
    printf("3. Overhand Serve  | Fault: 20%% | Target: Zones 1, 3, 5\n");
    printf("   Hint: \"Handa… Awit! Sa gilid yan o gitna!\"\n\n");
    printf("4. Jump Serve      | Fault: 30%% | Target: Zones 1, 3, 4, 6\n");
    printf("   Hint: \"Bantayan niyo mga sulok!\" (High risk/reward)\n\n");

    printf("--- COUNTER / ATTACK SKILLS (Rally Phase) ---\n");
    printf("5. Deep Return     | Fault:  5%% | Target: Zones 1, 3, 4, 6\n");
    printf("   Hint: \"Habol sa likod!\" (Forgiving defense)\n\n");
    printf("6. Dink            | Fault: 20%% | Target: Zones 1, 2, 3\n");
    printf("   Hint: \"Hulog sa unahan!\"\n\n");
    printf("7. Spike           | Fault: 25%% | Target: Zones 2, 4, 5\n");
    printf("   Hint: \"Aray mo!!! Spike sa mukha!\"\n\n");
    printf("8. Cut Shot        | Fault: 20%% | Target: Zones 3, 5, 6\n");
    printf("   Hint: \"Abang! Drop ball yan sa harap!\"\n");
    printf("==================================================\n");
}

// Clears the terminal and keeps the start screen pinned at the top
void reset_turn_screen(void)
{
    printf("\033[2J\033[H");
    print_mintonette_text();
    print_mintonette_ball();
    print_skill_guide();
    printf("\n");
    fflush(stdout);
}

void show_court(int attacker, int defender)
{
    printf("\n     PLAYER %d | ATTACK\n", attacker);
    printf("   +-------+-------+-------+\n");
    printf("   |   1   |   2   |   3   |\n");
    printf("   +-------+-------+-------+\n");
    printf("   |   4   |   5   |   6   |\n");
    printf("   +-------+-------+-------+\n");
    printf("     PLAYER %d | DEFENSE\n\n", defender);
}

void print_scoreboard(int p1_score, int p2_score, int server)
{
    printf("\n+---------------------------+\n");
    printf("|     VOLLEYBALL MATCH      |\n");
    printf("+---------------------------+\n");
    printf("|   PLAYER 1   |  PLAYER 2  |\n");
    printf("|      %02d      |     %02d     |\n", p1_score, p2_score);
    printf("+---------------------------+\n");
    printf("| SERVER: PLAYER %d          |\n", server);
    printf("| SET: 1  |  IN PROGRESS    |\n");
    printf("+---------------------------+\n\n");
}

int main(int argc, char *argv[])
{
    int client_sock, port_no;
    struct sockaddr_in server_addr;
    struct hostent *server;

    if (argc < 3)
    {
        printf("Usage: %s hostname port_no\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port_no = atoi(argv[2]);
    if (port_no <= 0 || port_no > 65535)
    {
        die_with_error("Error: Invalid port number.");
    }

    reset_turn_screen();

    printf("Client starting ...\n");
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0)
        die_with_error("Error: socket() Failed.");

    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        close(client_sock);
        die_with_error("Error: No such host.");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port_no);

    printf("Connecting to server at port %d...\n", port_no);
    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(client_sock);
        die_with_error("Error: connect() Failed.");
    }
    printf("Connection successful! Waiting for game to start...\n");

    GameState state;
    PlayerInput my_input;
    int game_over = 0;

    while (!game_over)
    {
        if (recv_all(client_sock, &state, sizeof(GameState)) < 0)
        {
            printf("\nServer disconnected.\n");
            break;
        }

        // Clear previous turn's output while keeping the start screen at the top
        reset_turn_screen();

        memset(&my_input, 0, sizeof(PlayerInput));
        my_input.player_id = state.player_id;

        switch (state.state_type)
        {
        case STATE_WAIT:
        {
            int defender = (state.attacker == 1) ? 2 : 1;
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            show_court(state.attacker, defender);
            printf("You are Player %d.\n", state.player_id);
            if (state.attacker == state.player_id)
            {
                printf("Waiting for the server...\n");
            }
            else
            {
                printf("Player %d is attacking...\n", state.attacker);
            }
            break;
        }

        case STATE_PICK_SKILL:
        {
            int first, last, banned;
            int defender = (state.attacker == 1) ? 2 : 1;
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            show_court(state.attacker, defender);
            printf("You are Player %d.\n", state.player_id);
            if (state.rally_count == 0)
            {
                printf("Action: SERVE\n");
                first = SERVE_UNDERHAND;
                last = SERVE_JUMP;
                banned = state.last_serve[state.player_id - 1];
            }
            else
            {
                printf("Action: COUNTER / ATTACK\n");
                first = COUNTER_DEEP;
                last = COUNTER_CUT;
                banned = state.last_counter[state.player_id - 1];
            }

            my_input.skill_id = ask_skill(first, last, banned, client_sock);
            my_input.target_zone = ask_zone(my_input.skill_id, client_sock);
            my_input.guess_zone = 0;

            if (send_all(client_sock, &my_input, sizeof(PlayerInput)) < 0)
            {
                close(client_sock);
                die_with_error("Error: send() Failed.");
            }
            printf("Move sent! Waiting for server...\n");
            break;
        }

        case STATE_PICK_DEFENSE:
        {
            SkillId used = state.last_action_skill;
            int defender = (state.attacker == 1) ? 2 : 1;
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            show_court(state.attacker, defender);

            printf("================================\n");
            printf("         PLAYER %d TURN\n", state.player_id);
            printf("================================\n");
            printf("\"%s\"\n\n", skills[used].hint);
            printf("Player %d used: %s\n", state.attacker, skills[used].name);

            printf("Incoming Target Zones:");
            for (int j = 0; j < skills[used].target_count; j++)
            {
                printf(" Zone %d", skills[used].targets[j]);
            }
            printf("\n\n");

            my_input.guess_zone = ask_defense_zone(used, client_sock);

            if (send_all(client_sock, &my_input, sizeof(PlayerInput)) < 0)
            {
                close(client_sock);
                die_with_error("Error: send() Failed.");
            }
            printf("Defense sent. Waiting for server...\n");
            break;
        }

        case STATE_RESULT:
        {
            SkillId used = state.last_action_skill;
            int defender = (state.attacker == 1) ? 2 : 1;
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            show_court(state.attacker, defender);

            if (state.fault_occurred)
            {
                printf(">>> FAULT! Player %d messed up their execution (%s)! <<<\n",
                       state.attacker, skills[used].name);
            }
            else if (state.was_caught)
            {
                printf(">>> CAUGHT! Player %d successfully returned the ball! <<<\n", state.defender);
            }
            else
            {
                printf(">>> MISSED! It landed in Zone %d. Player %d wins the point! <<<\n",
                       state.landing_zone, state.attacker);
            }

            if (state.point_winner != 0)
            {
                printf("\nPlayer %d wins the point!\n", state.point_winner);
            }

            wait_for_enter(client_sock);
            break;
        }

        case STATE_GAME_OVER:
        {
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            printf("\n==================================================\n");
            printf("🏆 PLAYER %d WINS THE VOLLEYBALL MATCH! 🏆\n", state.point_winner);
            if (state.point_winner == state.player_id)
            {
                printf("You won!\n");
            }
            else
            {
                printf("You lost. Better luck next time!\n");
            }
            printf("==================================================\n");
            game_over = 1;
            break;
        }

        case STATE_OPPONENT_LEFT:
        {
            printf("\n==================================================\n");
            printf("Opponent disconnected. You win by forfeit!\n");
            printf("==================================================\n");
            game_over = 1;
            break;
        }
        }
    }

    close(client_sock);
    return 0;
}