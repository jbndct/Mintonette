#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

#define MAX_TARGETS 4

int roll_percent(void) {
    return rand() % 100 + 1;
}

int fault(int fault_chance) {
    int roll = roll_percent();
    if (roll <= fault_chance) {
        return 1;
    }
    else {
        return 0;
    }
}

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
int pick_landing_zone(SkillId id) {
    int index = rand() % skills[id].target_count;
    return skills[id].targets[index];
}

int is_caught(SkillId id, int landing, int guess) {
    if (landing == guess) {
        return 1;
    }
    else if (skills[id].catch_rule == NEIGHBOR) {
        int row_a = (guess - 1) / 3;
        int col_a = (guess - 1) % 3;
        int row_b = (landing - 1) / 3;
        int col_b = (landing - 1) % 3;

        int row_gap = abs(row_a - row_b);
        int col_gap = abs(col_a - col_b);

        if (row_gap + col_gap == 1) {
            return 1;            /* side by side or front/back */
        }
    }
    return 0;
}

SkillId ask_skill(int first, int last, int banned)
{
    int i;
    int choice;
    SkillId id;

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
void show_court(void)
{
    printf("+---------------------------+\n");
    
    printf("     +-----------------+     \n");
    printf("     |  1  |  2  |  3  |     \n");
    printf("     |-----------------|     \n");
    printf("     |  4  |  5  |  6  |     \n");
    printf("     +-----------------+     \n");
}

int main(void) 
{
    int attacker = 0;
    int defender = 1;
    int last_skill[2] = { -1, -1 };
    int rally_count = 0;
    int rally_over = 0;
    int point_winner = -1;
    int first;
    int last;
    int temp;
    SkillId id;
    int landing;
    int guess;
    int j;
    
    srand((unsigned)time(NULL));

    print_mintonette_text();
    print_mintonette_ball();

    while(!rally_over) {
        show_court();
        printf("Player %d attacks\n", attacker+1);
        if (rally_count == 0) {
             first = SERVE_UNDERHAND;
             last = SERVE_JUMP;
        }
        else {
             first = COUNTER_DEEP;
             last = COUNTER_CUT;
        }

        id = ask_skill(first, last, last_skill[attacker]); 
        last_skill[attacker] = id;

        if (fault(skills[id].fault_chance)) {
            point_winner = defender;
            rally_over = 1;
        }
        else {
            landing = pick_landing_zone(id);
            printf("Incoming: %s! Zones:", skills[id].name);
            for (int j=0; j<skills[id].target_count; j++) {
                printf(" %d", skills[id].targets[j]);
            }
            printf("\n");

            int guess = read_int("Zone to protect", 1, 6);
            int caught = is_caught(id, landing, guess); 

            if (caught) {
                printf ("Caught!\n");
                int temp = defender;
                defender = attacker;
                attacker = temp;
                rally_count++;
            }
            else {
                printf("Missed! It landed in zone %d. Attacker wins the point \n", landing);
                point_winner = attacker;
                rally_over = 1;
            }
        }
        printf("Player %d wins the point!", point_winner + 1);
    }
    
    return 0;
}