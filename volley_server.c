/*
 * MINTONETTE - NETWORKED SERVER (based on mintonette.c)
 * Build: gcc -Wall -o mintonette_server mintonette_server.c
 * Run  : ./mintonette_server <port>      (then start two volley_client)
 * Protocol: see the header of volley_client.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#define MAX_TARGETS 4
#define TURN_SECS   15
#define WIN_SCORE   10

typedef enum {
    SERVE_UNDERHAND, SERVE_SHORT, SERVE_OVERHAND, SERVE_JUMP,
    COUNTER_DEEP, COUNTER_DINK, COUNTER_SPIKE, COUNTER_CUT, SKILL_COUNT
} SkillId;

typedef enum { EXACT, NEIGHBOR } CatchRule;

typedef struct {
    const char *name;
    int targets[MAX_TARGETS];
    int target_count;
    CatchRule catch_rule;
    int fault_chance;
    const char *clue;          /* text clue shown to the defender */
} Skill;

static Skill skills[SKILL_COUNT] = {
    {"Underhand Serve", {3,4},     2, NEIGHBOR,  5, "Underhand serve, dahan-dahan lang!"},
    {"Short Serve",     {2,3},     2, EXACT,     5, "Short! Harap!"},
    {"Overhand Serve",  {1,3,5},   3, EXACT,    20, "Overhand serve, abangan!"},
    {"Jump Serve",      {1,3,4,6}, 4, EXACT,    30, "Uyy, jump serve, likod!"},
    {"Deep Return",     {1,3,4,6}, 4, NEIGHBOR,  5, "Malalim na bola!"},
    {"Dink",            {1,2,3},   3, EXACT,    20, "Dink! Sa harap, dahan-dahan!"},
    {"Spike",           {2,4,5},   3, EXACT,    25, "SPIKE! Ingat!"},
    {"Cut Shot",        {3,5,6},   3, EXACT,    20, "Cut shot, patagilid!"}
};

/* ---------------- game logic (same as mintonette.c) ---------------- */
static int fault(int chance) { return (rand() % 100 + 1) <= chance; }

static int pick_landing_zone(SkillId id)
{
    return skills[id].targets[rand() % skills[id].target_count];
}

static int is_caught(SkillId id, int landing, int guess)
{
    if (landing == guess) return 1;
    if (skills[id].catch_rule == NEIGHBOR) {
        int ra = (guess - 1) / 3, ca = (guess - 1) % 3;
        int rb = (landing - 1) / 3, cb = (landing - 1) % 3;
        if (abs(ra - rb) + abs(ca - cb) == 1) return 1;
    }
    return 0;
}

/* ---------------- networking ---------------- */
static int cfd[2];
static char rbuf[2][1024];
static size_t rlen[2];

static void vsend(int p, const char *fmt, va_list ap)
{
    char buf[512];
    int n = vsnprintf(buf, sizeof buf - 1, fmt, ap);
    if (n < 0) return;
    if (n > (int)sizeof buf - 2) n = sizeof buf - 2;
    buf[n++] = '\n';
    send(cfd[p], buf, n, 0);
}
static void send_to(int p, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vsend(p, fmt, ap); va_end(ap); }
static void send_both(const char *fmt, ...)
{
    for (int p = 0; p < 2; p++) {
        va_list ap; va_start(ap, fmt); vsend(p, fmt, ap); va_end(ap);
    }
}

/* 1 = got a line, 0 = timeout, -1 = disconnected */
static int read_line(int p, time_t deadline, char *out, size_t outsz)
{
    for (;;) {
        char *nl = memchr(rbuf[p], '\n', rlen[p]);
        if (nl) {
            size_t n = (size_t)(nl - rbuf[p]);
            size_t c = n < outsz - 1 ? n : outsz - 1;
            memcpy(out, rbuf[p], c);
            out[c] = '\0';
            if (c > 0 && out[c - 1] == '\r') out[c - 1] = '\0';
            rlen[p] -= n + 1;
            memmove(rbuf[p], nl + 1, rlen[p]);
            return 1;
        }
        long left = (long)(deadline - time(NULL));
        if (left <= 0) return 0;
        fd_set rf; FD_ZERO(&rf); FD_SET(cfd[p], &rf);
        struct timeval tv = { left, 0 };
        int r = select(cfd[p] + 1, &rf, NULL, NULL, &tv);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return 0;
        if (rlen[p] >= sizeof rbuf[p]) rlen[p] = 0;      /* overflow guard */
        ssize_t n = recv(cfd[p], rbuf[p] + rlen[p], sizeof rbuf[p] - rlen[p], 0);
        if (n <= 0) return -1;
        rlen[p] += (size_t)n;
    }
}

/* Waits for "<tag> <n>" with min<=n<=max. banned = menu index that's not allowed. */
static int get_choice(int p, const char *tag, int min, int max, int banned, int *val)
{
    time_t deadline = time(NULL) + TURN_SECS;
    char line[128];
    for (;;) {
        int r = read_line(p, deadline, line, sizeof line);
        if (r <= 0) return r;
        if (!strcmp(line, "TIMEOUT")) return 0;
        if (!strcmp(line, "QUIT"))    return -1;
        char t[16]; int v;
        if (sscanf(line, "%15s %d", t, &v) == 2 && !strcmp(t, tag) && v >= min && v <= max) {
            if (v == banned) {
                send_to(p, "RESULT You can't use the same skill twice in a row.");
                continue;
            }
            *val = v;
            return 1;
        }
    }
}

static void player_left(int p)
{
    printf("Player %d disconnected.\n", p + 1);
    send_to(1 - p, "RESULT Opponent disconnected.");
    send_to(1 - p, "GAME_OVER WIN");
    exit(0);
}

static void send_scores(const int sc[2])
{
    send_to(0, "SCORE %d %d", sc[0], sc[1]);
    send_to(1, "SCORE %d %d", sc[1], sc[0]);
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: %s <port>\n", argv[0]); return 1; }
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)time(NULL));

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)atoi(argv[1]));
    if (bind(ls, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(ls, 2) < 0) {
        perror("bind/listen");
        return 1;
    }
    for (int p = 0; p < 2; p++) {
        printf("Waiting for player %d...\n", p + 1);
        cfd[p] = accept(ls, NULL, NULL);
        if (cfd[p] < 0) { perror("accept"); return 1; }
        send_to(p, "HELLO %d", p + 1);
        send_to(p, "RESULT Connected as player %d. Waiting for opponent...", p + 1);
    }
    printf("Both players connected. Game start!\n");

    int scores[2] = {0, 0};
    int server = 0;
    send_both("RESULT Game start! First to %d wins.", WIN_SCORE);
    send_scores(scores);

    while (scores[0] < WIN_SCORE && scores[1] < WIN_SCORE) {
        int attacker = server, defender = 1 - server;
        int rally_count = 0, point_winner = -1;
        int last_skill[2] = { -1, -1 };

        while (point_winner < 0) {
            int first = rally_count == 0 ? SERVE_UNDERHAND : COUNTER_DEEP;
            int last  = rally_count == 0 ? SERVE_JUMP      : COUNTER_CUT;
            int banned = (last_skill[attacker] >= first && last_skill[attacker] <= last)
                         ? last_skill[attacker] - first + 1 : 0;

            /* clear both screens, show the menu to the attacker */
            send_both("ZONES");
            send_both("CLUE");
            for (int i = first; i <= last; i++)
                send_to(attacker, "MENU %d %d %s", i - first + 1, i - first + 1 == banned, skills[i].name);
            send_to(attacker, "TURN %s %d", rally_count == 0 ? "SERVE" : "RECEIVE", TURN_SECS);
            send_to(defender, "OPP %d", TURN_SECS);
            send_to(defender, "RESULT Player %d is choosing a skill...", attacker + 1);

            int choice, r = get_choice(attacker, "SKILL", 1, 4, banned, &choice);
            if (r < 0) player_left(attacker);
            if (r == 0) {
                send_both("RESULT Player %d ran out of time!", attacker + 1);
                point_winner = defender;
                break;
            }
            SkillId id = (SkillId)(first + choice - 1);
            last_skill[attacker] = id;

            if (fault(skills[id].fault_chance)) {
                send_both("RESULT FAULT! Player %d messed up the %s.", attacker + 1, skills[id].name);
                point_winner = defender;
                break;
            }

            int landing = pick_landing_zone(id);
            char zl[64] = "ZONES";
            for (int j = 0; j < skills[id].target_count; j++) {
                char t[8];
                snprintf(t, sizeof t, " %d", skills[id].targets[j]);
                strcat(zl, t);
            }
            send_to(defender, "CLUE %s", skills[id].clue);
            send_to(defender, "%s", zl);
            send_to(defender, "RESULT Incoming: %s!", skills[id].name);
            send_to(defender, "TURN DEFEND %d", TURN_SECS);
            send_to(attacker, "OPP %d", TURN_SECS);
            send_to(attacker, "RESULT The ball is on its way...");

            int guess;
            r = get_choice(defender, "ZONE", 1, 6, 0, &guess);
            if (r < 0) player_left(defender);
            if (r == 0) {
                send_both("RESULT Player %d ran out of time!", defender + 1);
                point_winner = attacker;
                break;
            }

            if (is_caught(id, landing, guess)) {
                send_both("RESULT Caught! Player %d takes over.", defender + 1);
                int tmp = defender; defender = attacker; attacker = tmp;
                rally_count++;
            } else {
                send_both("ZONES %d", landing);
                send_both("RESULT Missed! It landed in zone %d.", landing);
                point_winner = attacker;
            }
        }

        scores[point_winner]++;
        server = point_winner;
        printf("Point to player %d  (%d - %d)\n", point_winner + 1, scores[0], scores[1]);
        send_scores(scores);
        send_both("RESULT Player %d wins the point!", point_winner + 1);
        sleep(3);
    }

    int w = scores[0] >= WIN_SCORE ? 0 : 1;
    send_to(w, "GAME_OVER WIN");
    send_to(1 - w, "GAME_OVER LOSE");
    printf("Player %d wins the game.\n", w + 1);
    sleep(1);
    return 0;
}