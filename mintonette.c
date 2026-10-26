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

void show_court(void)
{
    printf("+-----------------+\n");
    printf("|  1  |  2  |  3  |\n");
    printf("|-----------------|\n");
    printf("|  4  |  5  |  6  |\n");
    printf("+-----------------+\n");
}

int main() 
{
    srand((unsigned)time(NULL));
    int faults = 0;
    int i =0;
    int counts[7] = {0};
    for (i=0; i<1000; i++) {
        int zone = pick_landing_zone(SERVE_JUMP);
        counts[zone]++;
    }

    for (i = 1; i <= 6; i++) {
        printf("zone %d: %d\n", i, counts[i]);
    }

    int zone = read_int("Choose a zone (1-6)", 1, 6);
    printf("You picked %d. \n", zone);
    show_court();
    return 0;
}