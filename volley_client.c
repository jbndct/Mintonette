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

// Minimal ANSI colors for dark terminals
#define CLR_RESET  "\033[0m"
#define CLR_BOLD   "\033[1m"
#define CLR_RED    "\033[91m"
#define CLR_GREEN  "\033[92m"
#define CLR_YELLOW "\033[93m"
#define CLR_CYAN   "\033[96m"
#define CLR_GRAY   "\033[90m"

void die_with_error(const char *error_msg)
{
    fprintf(stderr, CLR_RED "%s" CLR_RESET "\n", error_msg);
    exit(EXIT_FAILURE);
}

const char *p_color(int player_id)
{
    return (player_id == 1) ? CLR_CYAN : CLR_YELLOW;
}

void wait_for_enter(int client_sock)
{
    char line[64];
    printf("\n" CLR_GRAY "Press ENTER to continue..." CLR_RESET);
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

        printf(CLR_RED "Invalid input. Enter a number between %d to %d: " CLR_RESET, min, max);
        fflush(stdout);
    }
}

int ask_skill(int first, int last, int banned, int client_sock)
{
    int choice, id;

    printf("\n" CLR_BOLD "Choose your technique:" CLR_RESET "\n");
    for (int i = first; i <= last; i++)
    {
        if (i == banned)
        {
            printf(CLR_GRAY "[%d] %s (Used last turn - Banned)" CLR_RESET "\n",
                   i - first + 1, skills[i].name);
        }
        else
        {
            printf(CLR_CYAN "[%d]" CLR_RESET " %s\n", i - first + 1, skills[i].name);
        }
    }

    while (1)
    {
        choice = read_int("Select option", 1, last - first + 1, client_sock);
        id = first + (choice - 1);
        if (id != banned)
        {
            return id;
        }
        printf(CLR_RED "You can't use the same skill twice in a row. Choose another.\n" CLR_RESET);
    }
}

int ask_zone(SkillId id, int client_sock)
{
    printf("\n" CLR_BOLD "Choose target zone:" CLR_RESET "\n");
    for (int j = 0; j < skills[id].target_count; j++)
    {
        printf(CLR_CYAN "[%d]" CLR_RESET " Zone %d\n", j + 1, skills[id].targets[j]);
    }

    int choice = read_int("Aim at option", 1, skills[id].target_count, client_sock);
    return skills[id].targets[choice - 1];
}

int ask_defense_zone(SkillId id, int client_sock)
{
    printf("\n" CLR_BOLD "Choose a defensive zone to protect:" CLR_RESET "\n");
    for (int j = 0; j < skills[id].target_count; j++)
    {
        printf(CLR_YELLOW "[%d]" CLR_RESET " Zone %d\n", j + 1, skills[id].targets[j]);
    }

    int choice = read_int("Protect option", 1, skills[id].target_count, client_sock);
    return skills[id].targets[choice - 1];
}

void print_mintonette_text(void)
{
    printf(CLR_CYAN CLR_BOLD);
    printf(" __  __ ___ _   _ _____ ___  _   _ _____ _____ _____ _____ \n");
    printf("|  \\/  |_ _| \\ | |_   _/ _ \\| \\ | | ____|_   _|_   _| ____|\n");
    printf("| |\\/| || ||  \\| | | || | | |  \\| |  _|   | |   | | |  _|  \n");
    printf("| |  | || || |\\  | | || |_| | |\\  | |___  | |   | | | |___ \n");
    printf("|_|  |_|___|_| \\_| |_| \\___/|_| \\_|_____| |_|   |_| |_____|\n");
    printf(CLR_RESET);
}

void print_mintonette_ball(void)
{
    printf(CLR_YELLOW);
    printf("                        _______\n");
    printf("                     .-'       '-.\n");
    printf("                   .'   \\  |  /   '.\n");
    printf("                  /      \\ | /      \\\n");
    printf("                 |________\\|/________|\n");
    printf("                 |        /|\\        |\n");
    printf("                  \\      / | \\      /\n");
    printf("                   '.   /  |  \\   .'\n");
    printf("                     '-._______.-'\n");
    printf(CLR_RESET);
}

void print_skill_guide(void)
{
    printf(CLR_GRAY "\n==================================================\n" CLR_RESET);
    printf(CLR_BOLD "                SKILL REFERENCE GUIDE             \n" CLR_RESET);
    printf(CLR_GRAY "==================================================\n" CLR_RESET);
    printf(CLR_CYAN "--- SERVE SKILLS (Opening Attack) ---" CLR_RESET "\n");
    printf("1. Underhand Serve | Fault: " CLR_GREEN " 5%%" CLR_RESET " | Target: Zones 3, 4\n");
    printf("   Hint: " CLR_YELLOW "\"Free ball yan oh!\"" CLR_RESET " (Forgiving defense)\n\n");
    printf("2. Short Serve     | Fault: " CLR_GREEN " 5%%" CLR_RESET " | Target: Zones 2, 3\n");
    printf("   Hint: " CLR_YELLOW "\"Yung unahan bantayan mo!\"" CLR_RESET "\n\n");
    printf("3. Overhand Serve  | Fault: " CLR_YELLOW "20%%" CLR_RESET " | Target: Zones 1, 3, 5\n");
    printf("   Hint: " CLR_YELLOW "\"Handa… Awit! Sa gilid yan o gitna!\"" CLR_RESET "\n\n");
    printf("4. Jump Serve      | Fault: " CLR_RED "30%%" CLR_RESET " | Target: Zones 1, 3, 4, 6\n");
    printf("   Hint: " CLR_YELLOW "\"Bantayan niyo mga sulok!\"" CLR_RESET " (High risk/reward)\n\n");

    printf(CLR_YELLOW "--- COUNTER / ATTACK SKILLS (Rally Phase) ---" CLR_RESET "\n");
    printf("5. Deep Return     | Fault: " CLR_GREEN " 5%%" CLR_RESET " | Target: Zones 1, 3, 4, 6\n");
    printf("   Hint: " CLR_YELLOW "\"Habol sa likod!\"" CLR_RESET " (Forgiving defense)\n\n");
    printf("6. Dink            | Fault: " CLR_YELLOW "20%%" CLR_RESET " | Target: Zones 1, 2, 3\n");
    printf("   Hint: " CLR_YELLOW "\"Hulog sa unahan!\"" CLR_RESET "\n\n");
    printf("7. Spike           | Fault: " CLR_RED "25%%" CLR_RESET " | Target: Zones 2, 4, 5\n");
    printf("   Hint: " CLR_YELLOW "\"Aray mo!!! Spike sa mukha!\"" CLR_RESET "\n\n");
    printf("8. Cut Shot        | Fault: " CLR_YELLOW "20%%" CLR_RESET " | Target: Zones 3, 5, 6\n");
    printf("   Hint: " CLR_YELLOW "\"Abang! Drop ball yan sa harap!\"" CLR_RESET "\n");
    printf(CLR_GRAY "==================================================\n" CLR_RESET);
}

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
    printf("\n     %sPLAYER %d" CLR_RESET " | " CLR_RED "ATTACK" CLR_RESET "\n", p_color(attacker), attacker);
    printf(CLR_GRAY "   +-------+-------+-------+\n");
    printf("   |   " CLR_RESET "1" CLR_GRAY "   |   " CLR_RESET "2" CLR_GRAY "   |   " CLR_RESET "3" CLR_GRAY "   |\n");
    printf("   +-------+-------+-------+\n");
    printf("   |   " CLR_RESET "4" CLR_GRAY "   |   " CLR_RESET "5" CLR_GRAY "   |   " CLR_RESET "6" CLR_GRAY "   |\n");
    printf("   +-------+-------+-------+\n" CLR_RESET);
    printf("     %sPLAYER %d" CLR_RESET " | " CLR_CYAN "DEFENSE" CLR_RESET "\n\n", p_color(defender), defender);
}

void print_scoreboard(int p1_score, int p2_score, int server)
{
    printf(CLR_GRAY "\n+---------------------------+\n");
    printf("|" CLR_RESET CLR_BOLD "     VOLLEYBALL MATCH      " CLR_RESET CLR_GRAY "|\n");
    printf("+---------------------------+\n");
    printf("|   " CLR_CYAN "PLAYER 1" CLR_GRAY "   |  " CLR_YELLOW "PLAYER 2" CLR_GRAY "  |\n");
    printf("|      " CLR_CYAN CLR_BOLD "%02d" CLR_RESET CLR_GRAY "      |     " CLR_YELLOW CLR_BOLD "%02d" CLR_RESET CLR_GRAY "     |\n", p1_score, p2_score);
    printf("+---------------------------+\n");
    printf("| SERVER: %sPLAYER %d" CLR_GRAY "          |\n", p_color(server), server);
    printf("| SET: 1  |  IN PROGRESS    |\n");
    printf("+---------------------------+\n\n" CLR_RESET);
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
    printf(CLR_GREEN "Connection successful! Waiting for game to start...\n" CLR_RESET);

    GameState state;
    PlayerInput my_input;
    int game_over = 0;

    while (!game_over)
    {
        if (recv_all(client_sock, &state, sizeof(GameState)) < 0)
        {
            printf("\n" CLR_RED "Server disconnected." CLR_RESET "\n");
            break;
        }

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
            printf("You are %sPlayer %d" CLR_RESET ".\n", p_color(state.player_id), state.player_id);
            if (state.attacker == state.player_id)
            {
                printf("Waiting for the server...\n");
            }
            else
            {
                printf("%sPlayer %d" CLR_RESET " is attacking...\n", p_color(state.attacker), state.attacker);
            }
            break;
        }

        case STATE_PICK_SKILL:
        {
            int first, last, banned;
            int defender = (state.attacker == 1) ? 2 : 1;
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            show_court(state.attacker, defender);
            printf("You are %sPlayer %d" CLR_RESET ".\n", p_color(state.player_id), state.player_id);
            if (state.rally_count == 0)
            {
                printf("Action: " CLR_CYAN CLR_BOLD "SERVE" CLR_RESET "\n");
                first = SERVE_UNDERHAND;
                last = SERVE_JUMP;
                banned = state.last_serve[state.player_id - 1];
            }
            else
            {
                printf("Action: " CLR_YELLOW CLR_BOLD "COUNTER / ATTACK" CLR_RESET "\n");
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
            printf(CLR_GREEN "Move sent! Waiting for server...\n" CLR_RESET);
            break;
        }

        case STATE_PICK_DEFENSE:
        {
            SkillId used = state.last_action_skill;
            int defender = (state.attacker == 1) ? 2 : 1;
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            show_court(state.attacker, defender);

            printf(CLR_GRAY "================================\n" CLR_RESET);
            printf(CLR_BOLD "         PLAYER %d TURN\n" CLR_RESET, state.player_id);
            printf(CLR_GRAY "================================\n" CLR_RESET);
            printf(CLR_YELLOW "\"%s\"" CLR_RESET "\n\n", skills[used].hint);
            printf("%sPlayer %d" CLR_RESET " used: " CLR_BOLD "%s" CLR_RESET "\n",
                   p_color(state.attacker), state.attacker, skills[used].name);

            printf("Incoming Target Zones:");
            for (int j = 0; j < skills[used].target_count; j++)
            {
                printf(CLR_YELLOW " Zone %d" CLR_RESET, skills[used].targets[j]);
            }
            printf("\n\n");

            my_input.guess_zone = ask_defense_zone(used, client_sock);

            if (send_all(client_sock, &my_input, sizeof(PlayerInput)) < 0)
            {
                close(client_sock);
                die_with_error("Error: send() Failed.");
            }
            printf(CLR_GREEN "Defense sent. Waiting for server...\n" CLR_RESET);
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
                printf(CLR_RED CLR_BOLD ">>> FAULT! Player %d messed up their execution (%s)! <<<" CLR_RESET "\n",
                       state.attacker, skills[used].name);
            }
            else if (state.was_caught)
            {
                printf(CLR_GREEN CLR_BOLD ">>> CAUGHT! Player %d successfully returned the ball! <<<" CLR_RESET "\n",
                       state.defender);
            }
            else
            {
                printf(CLR_RED CLR_BOLD ">>> MISSED! It landed in Zone %d. Player %d wins the point! <<<" CLR_RESET "\n",
                       state.landing_zone, state.attacker);
            }

            if (state.point_winner != 0)
            {
                printf("\n%s" CLR_BOLD "Player %d wins the point!" CLR_RESET "\n",
                       p_color(state.point_winner), state.point_winner);
            }

            wait_for_enter(client_sock);
            break;
        }

        case STATE_GAME_OVER:
        {
            print_scoreboard(state.scores[0], state.scores[1], state.server);
            printf(CLR_GRAY "\n==================================================\n" CLR_RESET);
            printf(CLR_YELLOW CLR_BOLD "🏆 PLAYER %d WINS THE VOLLEYBALL MATCH! 🏆\n" CLR_RESET, state.point_winner);
            if (state.point_winner == state.player_id)
            {
                printf(CLR_GREEN CLR_BOLD "You won!\n" CLR_RESET);
            }
            else
            {
                printf(CLR_RED "You lost. Better luck next time!\n" CLR_RESET);
            }
            printf(CLR_GRAY "==================================================\n" CLR_RESET);
            game_over = 1;
            break;
        }

        case STATE_OPPONENT_LEFT:
        {
            printf(CLR_GRAY "\n==================================================\n" CLR_RESET);
            printf(CLR_YELLOW CLR_BOLD "Opponent disconnected. You win by forfeit!\n" CLR_RESET);
            printf(CLR_GRAY "==================================================\n" CLR_RESET);
            game_over = 1;
            break;
        }
        }
    }

    close(client_sock);
    return 0;
}