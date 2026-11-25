#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

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
    {"Cut Shot", {3, 5, 6}, 3, EXACT, 20, "Abang! Drop ball yan sa harap!"}
};

typedef enum {
    STATE_WAIT,          // waiting
    STATE_PICK_SKILL,    // player attacking
    STATE_PICK_DEFENSE,  // player defending
    STATE_RESULT,        // result of the turn/point
    STATE_GAME_OVER,
    STATE_OPPONENT_LEFT  // opponent disconnected mid-match
} StateType;

// Fixed-width int32_t fields have natural 4-byte alignment (no padding needed)
typedef struct {
    int32_t player_id;
    int32_t skill_id;
    int32_t target_zone;
    int32_t guess_zone;
} PlayerInput;

typedef struct {
    int32_t player_id;
    int32_t scores[2];
    int32_t attacker;
    int32_t defender;
    int32_t server;
    int32_t rally_count;
    int32_t last_serve[2];
    int32_t last_counter[2];

    int32_t state_type;
    int32_t last_action_skill;
    int32_t landing_zone;
    int32_t defender_guess;
    int32_t was_caught;
    int32_t fault_occurred;
    int32_t point_winner;
} GameState;

_Static_assert(sizeof(PlayerInput) == 16, "Unexpected PlayerInput padding");
_Static_assert(sizeof(GameState) == 72, "Unexpected GameState padding");

// Reliable TCP send/recv helpers to prevent partial packet reads/writes
static inline int send_all(int sock, const void *buf, size_t len)
{
    size_t total = 0;
    const char *p = (const char *)buf;
    while (total < len) {
        ssize_t n = send(sock, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

static inline int recv_all(int sock, void *buf, size_t len)
{
    size_t total = 0;
    char *p = (char *)buf;
    while (total < len) {
        ssize_t n = recv(sock, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

#endif