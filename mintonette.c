#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define MAX_TARGETS 4

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

int main() 
{
    for(int i=0; i<SKILL_COUNT; i++) {
        printf("%d. %s | Targets: ", i+1, skills[i].name);
        for(int j=0; j<skills[i].target_count; j++) {
            printf("%d ", skills[i].targets[j]);
        }
        printf("| Catch: ");
        if (skills[i].catch_rule==EXACT)  {
            printf("EXACT ");
        }
        else if (skills[i].catch_rule==NEIGHBOR) {
             printf("NEIGHBOR ");
        }
        printf("| Fault Chance: %d%% \n", skills[i].fault_chance);

    }

    return 0;
}