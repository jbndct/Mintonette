#ifndef PROTOCOL_H
#define PROTOCOL_H 

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_TARGETS 4
#define WIN_SCORE 10

#define PLAYER1_ID 1
#define PLAYER2_ID 2

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
    const char *hint;
} Skill;

static const Skill skills[SKILL_COUNT] = {
    {"Underhand Serve", {3, 4}, 2, NEIGHBOR, 5, "Free ball yan oh!"},
    {"Short Serve", {2, 3}, 2, EXACT, 5, "Yung unahan bantayan mo!"},
    {"Overhand Serve", {1, 3, 5}, 3, EXACT, 20, "Handa… Awit! Sa gilid yan o gitna!"},
    {"Jump Serve", {1, 3, 4, 6}, 4, EXACT, 30, "Bantayan niyo mga sulok!"},
    {"Deep Return", {1, 3, 4, 6}, 4, NEIGHBOR, 5, "Habol sa likod!"},
    {"Dink", {1, 2, 3}, 3, EXACT, 20, "Hulog sa unahan!"},
    {"Spike", {2, 4, 5}, 3, EXACT, 25, "Aray mo!!! Spike sa mukha!"},
    {"Cut Shot", {3, 5, 6}, 3, EXACT, 20, "Abang! Drop ball yan sa harap!"}};

typedef enum {
    STATE_WAIT,          //waiting
    STATE_PICK_SKILL,    //player attacking
    STATE_PICK_DEFENSE,  //player defending
    STATE_RESULT,        //result of the match
    STATE_GAME_OVER
} StateType;

// struct for move choices made by player
typedef struct {
    int player_id;
    int skill_id;
    int target_zone;
    int guess_zone;
} PlayerInput;

// struct for current game state, for synchonization
typedef struct {
    int player_id;
    int scores[2];
    int attacker;
    int defender;
    int server;
    int rally_count;
    int last_skill[2];
    
    int state_type;
    int last_action_skill;
    int landing_zone;
    int defender_guess;
    int was_caught;
    int fault_occurred;
    int point_winner;
} GameState;

#endif