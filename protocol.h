#ifndef PROTOCOL_H
#define PROTOCOL_H 

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    int player_id;
    int skill_id;
    int target_zone;
    int guess_zone;
} PlayerInput;

typedef struct {
    int scores[2];
    int attacker;
    int defender;
    int rally_count;
    int last_skill[2];
    
    int state_type;
    int last_action_skill;
    int landing_zone;
    int defender_guess;
    int was_caught;
    int fault_occured;
    int point_winner;
} GameState;

#endif