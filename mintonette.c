#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_TARGETS 4

void print_mintonette_text()
{
    printf(" __  __ ___ _   _ _____ ___  _   _ _____ _____ _____ _____ \n");
    printf("|  \\/  |_ _| \\ | |_   _/ _ \\| \\ | | ____|_   _|_  _| ____|\n");
    printf("| |\\/| || ||  \\| | | || | | |  \\| |  _|   | |   | | |  _|  \n");
    printf("| |  | || || |\\  | | || |_| | |\\  | |___  | |   | | | |___ \n");
    printf("|_|  |_|___|_| \\_| |_| \\___/|_| \\_|_____| |_|   |_| |_____|\n");
}

void print_mintonette_ball()
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

typedef enum
{
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

typedef enum
{
    EXACT,
    NEIGHBOR
} CatchRule;

typedef struct
{
    const char *name;
    int targets[MAX_TARGETS];
    int target_count;
    CatchRule catch_rule;
    int fault_chance;
    const char *hint; // Added proper Tagalog hints
} Skill;

Skill skills[SKILL_COUNT] = {
    {"Underhand Serve", {3, 4}, 2, NEIGHBOR, 5, "Free ball yan oh!"},
    {"Short Serve", {2, 3}, 2, EXACT, 5, "Yung unahan bantayan mo!"},
    {"Overhand Serve", {1, 3, 5}, 3, EXACT, 20, "Handa… Awit! Sa gilid yan o gitna!"},
    {"Jump Serve", {1, 3, 4, 6}, 4, EXACT, 30, "Bantayan niyo mga sulok!"},
    {"Deep Return", {1, 3, 4, 6}, 4, NEIGHBOR, 5, "Habol sa likod!"},
    {"Dink", {1, 2, 3}, 3, EXACT, 20, "Hulog sa unahan!"},
    {"Spike", {2, 4, 5}, 3, EXACT, 25, "Aray mo!!! Spike sa mukha!"},
    {"Cut Shot", {3, 5, 6}, 3, EXACT, 20, "Abang! Drop ball yan sa harap!"}};

int roll_percent(void)
{
    return rand() % 100 + 1;
}

int fault(int fault_chance)
{
    int roll = roll_percent();
    return (roll <= fault_chance) ? 1 : 0;
}

int read_int(const char *prompt, int min, int max)
{
    char line[64];
    int value;
    printf("%s: ", prompt);
    while (1)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("\nNo more input.\n");
            exit(1);
        }
        if (sscanf(line, "%d", &value) == 1 && value >= min && value <= max)
        {
            return value;
        }
        printf("Invalid input. Enter a number between %d to %d: ", min, max);
    }
}

int zone_in_skill(SkillId id, int zone)
{
    int i;
    for (i = 0; i < skills[id].target_count; i++)
    {
        if (skills[id].targets[i] == zone)
        {
            return 1;
        }
    }
    return 0;
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
            return 1; /* side by side or front/back */
        }
    }
    return 0;
}

void show_court(int attacker, int defender)
{
    printf("\n     PLAYER %d | ATTACK\n", attacker + 1);
    printf("   +-------+-------+-------+\n");
    printf("   |   1   |   2   |   3   |\n");
    printf("   +-------+-------+-------+\n");
    printf("   |   4   |   5   |   6   |\n");
    printf("   +-------+-------+-------+\n");
    printf("     PLAYER %d | DEFENSE\n\n", defender + 1);
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

int ask_skill(int first, int last, int banned)
{
    int i;
    int choice;
    int id;

    printf("\nChoose your technique:\n");
    for (i = first; i <= last; i++)
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
        choice = read_int("Select option", 1, last - first + 1);
        id = first + (choice - 1);
        if (id != banned)
        {
            return id;
        }
        printf("You can't use the same skill twice in a row. Choose another.\n");
    }
}

int ask_zone(SkillId id)
{
    int zone;

    printf("\nChoose target zone:\n");
    for (int i = 1; i <= 6; i++)
    {
        printf("[%d] Zone %d\n", i, i);
    }

    while (1)
    {
        zone = read_int("Aim at zone", 1, 6);
        if (zone_in_skill(id, zone))
        {
            return zone;
        }
        else
        {
            printf("That skill can't reach Zone %d. Try a valid zone.\n", zone);
        }
    }
}

int main(void)
{
    int attacker = 0;
    int defender = 1;
    int last_skill[2] = {-1, -1};
    int rally_count = 0;
    int rally_over = 0;
    int point_winner = -1;
    int first;
    int last;
    int temp;
    SkillId id;
    int landing;
    int guess;
    int scores[2] = {0, 0};
    int server = 0;

    srand((unsigned)time(NULL));

    print_mintonette_text();
    print_mintonette_ball();
    printf("\n\n");

    while ((scores[0] < 10) && (scores[1] < 10))
    {
        attacker = server;
        defender = 1 - server;
        rally_count = 0;
        last_skill[0] = -1;
        last_skill[1] = -1;
        rally_over = 0;
        point_winner = -1;

        while (!rally_over)
        {
            // Clear screen feel or separation line
            printf("================================\n");
            printf("         PLAYER %d TURN\n", attacker + 1);
            printf("================================\n");

            show_court(attacker, defender);

            if (rally_count == 0)
            {
                printf("Action: SERVE\n");
                first = SERVE_UNDERHAND;
                last = SERVE_JUMP;
            }
            else
            {
                printf("Action: COUNTER / ATTACK\n");
                first = COUNTER_DEEP;
                last = COUNTER_CUT;
            }

            id = ask_skill(first, last, last_skill[attacker]);
            last_skill[attacker] = id;

            // Check Fault
            if (fault(skills[id].fault_chance))
            {
                printf("\n>>> FAULT! Player %d messed up their execution (%s)! <<<\n", attacker + 1, skills[id].name);
                point_winner = defender;
                rally_over = 1;
                break;
            }

            landing = ask_zone(id);

            printf("\nYour opponent is preparing to receive...\n\n");

            // Defender Turn Screen
            printf("================================\n");
            printf("         PLAYER %d TURN\n", defender + 1);
            printf("================================\n");

            show_court(attacker, defender);

            // Display Tagalog Hint
            printf("\"%s\"\n\n", skills[id].hint);
            printf("Player %d used: %s\n", attacker + 1, skills[id].name);

            // Show targets for information context
            printf("Incoming Target Zones:");
            for (int j = 0; j < skills[id].target_count; j++)
            {
                printf(" Zone %d", skills[id].targets[j]);
            }
            printf("\n\n");

            printf("Choose a defensive zone:\n");
            for (int i = 1; i <= 6; i++)
            {
                printf("[%d] Zone %d\n", i, i);
            }

            guess = read_int("Your decision (Zone to protect)", 1, 6);
            int caught = is_caught(id, landing, guess);

            if (caught)
            {
                printf("\n>>> CAUGHT! Player %d successfully returned the ball! <<<\n\n", defender + 1);
                temp = defender;
                defender = attacker;
                attacker = temp;
                rally_count++;
            }
            else
            {
                printf("\n>>> MISSED! It landed in Zone %d. Player %d wins the point! <<<\n", landing, attacker + 1);
                point_winner = attacker;
                rally_over = 1;
            }
        }

        scores[point_winner]++;
        server = point_winner;

        printf("\nPlayer %d wins the point!\n", point_winner + 1);
        print_scoreboard(scores, server);

        printf("Press Enter to continue to the next rally...");
        getchar();
    }

    printf("\n==================================================\n");
    if (scores[0] == 10)
    {
        printf("PLAYER 1 WINS THE VOLLEYBALL MATCH!\n");
    }
    else
    {
        printf("PLAYER 2 WINS THE VOLLEYBALL MATCH!\n");
    }
    printf("==================================================\n");

    return 0;
}
