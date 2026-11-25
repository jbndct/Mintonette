#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include "protocol.h"

void die_with_error(const char *error_msg)
{
    perror(error_msg);
    exit(EXIT_FAILURE);
}

int roll_percent(void)
{
    return rand() % 100 + 1;
}

int fault(int fault_chance)
{
    int roll = roll_percent();
    return (roll <= fault_chance) ? 1 : 0;
}

int is_caught(SkillId id, int landing, int guess)
{
    if (landing == guess)
    {
        return 1;
    }
    else if (skills[id].catch_rule == NEIGHBOR)
    {
        int row_a = (guess - 1) / 3;
        int col_a = (guess - 1) % 3;
        int row_b = (landing - 1) / 3;
        int col_b = (landing - 1) % 3;

        int row_gap = abs(row_a - row_b);
        int col_gap = abs(col_a - col_b);

        if (row_gap + col_gap == 1)
        {
            return 1; // side by side or front/back
        }
    }
    return 0;
}

void notify_disconnect(int surviving_sock, GameState *state, int surviving_id)
{
    state->player_id = surviving_id;
    state->state_type = STATE_OPPONENT_LEFT;
    send_all(surviving_sock, state, sizeof(GameState));
}

int main(int argc, char *argv[])
{
    int server_sock, port_no;
    int client_socks[2] = {-1, -1};
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size;

    // Prevent server crash if a client closes socket unexpectedly
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2)
    {
        printf("Usage: %s port_no\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port_no = atoi(argv[1]);
    if (port_no <= 0 || port_no > 65535)
    {
        fprintf(stderr, "Error: Invalid port number.\n");
        exit(EXIT_FAILURE);
    }

    printf("Server starting ...\n");

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
        die_with_error("Error: socket() Failed");

    memset(&server_addr, 0, sizeof(server_addr));

    int yes = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_no);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        die_with_error("Error: bind() Failed");

    if (listen(server_sock, 2) < 0)
        die_with_error("Error: listen() Failed");

    printf("Server listening to port %d ...\n", port_no);

    for (int i = 0; i < 2; i++)
    {
        printf("Waiting for Player %d to connect...\n", i + 1);
        client_size = sizeof(client_addr);
        client_socks[i] = accept(server_sock, (struct sockaddr *)&client_addr, &client_size);
        if (client_socks[i] < 0)
        {
            die_with_error("Error: accept() Failed");
        }
        printf("Player %d connected!\n", i + 1);
    }

    srand((unsigned)time(NULL));

    GameState state;
    memset(&state, 0, sizeof(GameState));
    state.server = 1;
    state.last_serve[0] = -1;
    state.last_serve[1] = -1;
    state.last_counter[0] = -1;
    state.last_counter[1] = -1;

    PlayerInput atk_in, def_in;
    int quit = 0;

    while (!quit && state.scores[0] < WIN_SCORE && state.scores[1] < WIN_SCORE)
    {
        state.attacker = state.server;
        state.defender = 3 - state.server;
        state.rally_count = 0;
        // Reset rally counter bans at start of a new point, but keep last_serve across points
        state.last_counter[0] = -1;
        state.last_counter[1] = -1;
        state.point_winner = 0;
        int rally_over = 0;

        while (!rally_over && !quit)
        {
            int a = state.attacker - 1;
            int d = state.defender - 1;
            int swap_roles = 0;

            // Reset per-turn outcome fields
            state.was_caught = 0;
            state.fault_occurred = 0;
            state.landing_zone = 0;
            state.defender_guess = 0;

            // 1. ATTACKER picks skill & zone, DEFENDER waits
            for (int i = 0; i < 2; i++)
            {
                state.player_id = i + 1;
                state.state_type = (i == a) ? STATE_PICK_SKILL : STATE_WAIT;
                if (send_all(client_socks[i], &state, sizeof(GameState)) < 0)
                {
                    printf("Player %d disconnected during state broadcast.\n", i + 1);
                    notify_disconnect(client_socks[1 - i], &state, (1 - i) + 1);
                    quit = 1;
                    break;
                }
            }
            if (quit) break;

            if (recv_all(client_socks[a], &atk_in, sizeof(PlayerInput)) < 0)
            {
                printf("Player %d disconnected.\n", a + 1);
                notify_disconnect(client_socks[d], &state, d + 1);
                quit = 1;
                break;
            }

            // Validate attacker input
            int id = atk_in.skill_id;
            int is_serve = (state.rally_count == 0);
            int first = is_serve ? SERVE_UNDERHAND : COUNTER_DEEP;
            int last  = is_serve ? SERVE_JUMP : COUNTER_CUT;
            int banned = is_serve ? state.last_serve[a] : state.last_counter[a];

            int zone_ok = 0;
            if (id >= first && id <= last)
            {
                for (int j = 0; j < skills[id].target_count; j++)
                {
                    if (skills[id].targets[j] == atk_in.target_zone)
                        zone_ok = 1;
                }
            }
            if (!zone_ok || id == banned)
            {
                printf("Invalid input from Player %d.\n", a + 1);
                notify_disconnect(client_socks[d], &state, d + 1);
                quit = 1;
                break;
            }

            if (is_serve)
                state.last_serve[a] = id;
            else
                state.last_counter[a] = id;

            state.last_action_skill = id;
            printf("Player %d used: %s (landing %d)\n", a + 1, skills[id].name, atk_in.target_zone);

            // 2. Determine fault chance
            if (fault(skills[id].fault_chance))
            {
                state.fault_occurred = 1;
                state.point_winner = state.defender;
                rally_over = 1;
            }
            else
            {
                // 3. DEFENDER picks zone, ATTACKER waits
                for (int i = 0; i < 2; i++)
                {
                    state.player_id = i + 1;
                    state.state_type = (i == d) ? STATE_PICK_DEFENSE : STATE_WAIT;
                    if (send_all(client_socks[i], &state, sizeof(GameState)) < 0)
                    {
                        printf("Player %d disconnected during defense broadcast.\n", i + 1);
                        notify_disconnect(client_socks[1 - i], &state, (1 - i) + 1);
                        quit = 1;
                        break;
                    }
                }
                if (quit) break;

                if (recv_all(client_socks[d], &def_in, sizeof(PlayerInput)) < 0)
                {
                    printf("Player %d disconnected.\n", d + 1);
                    notify_disconnect(client_socks[a], &state, a + 1);
                    quit = 1;
                    break;
                }

                // Validate defender guess
                int guess_ok = 0;
                for (int j = 0; j < skills[id].target_count; j++)
                {
                    if (skills[id].targets[j] == def_in.guess_zone)
                        guess_ok = 1;
                }
                if (!guess_ok)
                {
                    printf("Invalid defense input from Player %d.\n", d + 1);
                    notify_disconnect(client_socks[a], &state, a + 1);
                    quit = 1;
                    break;
                }

                // 4. Evaluate catch/miss
                int landing = atk_in.target_zone;
                int guess = def_in.guess_zone;
                int caught = is_caught(id, landing, guess);

                state.landing_zone = landing;
                state.defender_guess = guess;
                state.was_caught = caught;

                if (caught)
                {
                    swap_roles = 1;
                }
                else
                {
                    state.point_winner = state.attacker;
                    rally_over = 1;
                }
            }

            // 5. Update score & server, send result to both
            if (state.point_winner != 0)
            {
                state.scores[state.point_winner - 1]++;
                state.server = state.point_winner;
            }

            for (int i = 0; i < 2; i++)
            {
                state.player_id = i + 1;
                state.state_type = STATE_RESULT;
                if (send_all(client_socks[i], &state, sizeof(GameState)) < 0)
                {
                    printf("Player %d disconnected during result broadcast.\n", i + 1);
                    notify_disconnect(client_socks[1 - i], &state, (1 - i) + 1);
                    quit = 1;
                    break;
                }
            }

            // 6. Swap roles if rally continues
            if (swap_roles)
            {
                int t = state.attacker;
                state.attacker = state.defender;
                state.defender = t;
                state.rally_count++;
            }
        }
    }

    // End game broadcast
    if (!quit)
    {
        state.point_winner = (state.scores[0] >= WIN_SCORE) ? 1 : 2;
        for (int i = 0; i < 2; i++)
        {
            state.player_id = i + 1;
            state.state_type = STATE_GAME_OVER;
            send_all(client_socks[i], &state, sizeof(GameState));
        }
    }

    printf("Closing connection ...\n");
    if (client_socks[0] >= 0) close(client_socks[0]);
    if (client_socks[1] >= 0) close(client_socks[1]);
    close(server_sock);

    return 0;
}