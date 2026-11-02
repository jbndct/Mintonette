/*
 * VOLLEYBALL - TERMINAL CLIENT
 * ---------------------------------------------------------------
 * Build : gcc -Wall -o volley_client volley_client.c
 * Run   : ./volley_client <server-ip> <port>
 *
 * The client is "dumb" on purpose: it only (1) shows what the server
 * tells it and (2) sends the player's choices back. All game logic
 * (fault chances, scoring, who serves next, win at 10) lives on the server.
 *
 * PROTOCOL (TCP, text, one message per line, '\n' terminated)
 * ---------------------------------------------------------------
 * SERVER -> CLIENT
 *   HELLO <id>              you are player 1 or 2
 *   SCORE <you> <opp>       current score
 *   MENU <n> <banned> <name>  one skill entry (n = 1-4, banned = 1 if it was
 *                           used last turn and can't be picked); the server
 *                           sends 4 of these right before TURN SERVE/RECEIVE
 *   TURN SERVE <secs>       your turn: choose a serve skill   (1-4)
 *   TURN RECEIVE <secs>     your turn: choose a receive skill (1-4)
 *   TURN DEFEND <secs>      your turn: choose a zone to protect (1-6)
 *   OPP <secs>              opponent is acting (you wait)
 *   CLUE <text>             text clue shown to the defender
 *                           e.g. "Uyy, jump serve, likod!"
 *   ZONES <z1> <z2> ...     zones to highlight on your court
 *                           ("ZONES" alone clears highlights)
 *   RESULT <text>           message about what just happened
 *   GAME_OVER WIN|LOSE      match finished
 *
 * CLIENT -> SERVER
 *   SKILL <1-4>             chosen serve / receive skill
 *   ZONE <1-6>              chosen zone to protect
 *   TIMEOUT                 player ran out of time
 *   QUIT                    player left
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUF_SIZE   2048
#define TIME_BAR   20
#define WIN_SCORE  10

/* ANSI helpers */
#define CLR   "\033[2J\033[H"
#define BOLD  "\033[1;33m"
#define DIM   "\033[2m"
#define GREEN "\033[1;32m"
#define RED   "\033[1;31m"
#define CYAN  "\033[1;36m"
#define RST   "\033[0m"

typedef enum {
    M_WAIT,       /* waiting for server                */
    M_SERVE,      /* choose serve skill (1-4)          */
    M_RECEIVE,    /* choose receive skill (1-4)        */
    M_DEFEND,     /* choose zone to protect (1-6)      */
    M_OVER        /* game finished                     */
} Mode;

typedef struct {
    int  id;                 /* 1 or 2 */
    int  my_score, opp_score;
    Mode mode;
    int  total_secs;         /* length of the current timer */
    time_t deadline;         /* when the timer ends         */
    int  timer_active;
    int  zones[7];           /* zones[1..6] highlighted     */
    char clue[160];
    char result[160];
    char menu_name[4][48];   /* skill menu sent by the server */
    int  menu_banned[4];
    int  won;                /* valid when M_OVER           */
} Game;

static Game g;
static int sock = -1;
static struct termios old_tio;
static int tio_saved = 0;

/* ------------------------------------------------------------------ */
/* Skill tables (from the game spec)                                  */
/* ------------------------------------------------------------------ */
/* (no hardcoded skills: the server sends the menu with MENU messages) */

/* ------------------------------------------------------------------ */
/* Terminal handling                                                  */
/* ------------------------------------------------------------------ */
static void restore_terminal(void)
{
    if (tio_saved) tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    printf(RST "\n");
}

static void raw_terminal(void)
{
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &old_tio) == 0) {
        tio_saved = 1;
        t = old_tio;
        t.c_lflag &= ~(ICANON | ECHO);   /* single key presses, no echo */
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
    atexit(restore_terminal);
}

/* ------------------------------------------------------------------ */
/* Networking helpers                                                 */
/* ------------------------------------------------------------------ */
static int connect_to(const char *host, const char *port)
{
    struct addrinfo hints, *res, *p;
    int fd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static void send_line(const char *fmt, ...) __attribute__((format(printf,1,2)));
#include <stdarg.h>
static void send_line(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    buf[n++] = '\n';
    send(sock, buf, n, 0);
}

/* ------------------------------------------------------------------ */
/* Rendering                                                          */
/* ------------------------------------------------------------------ */
static int secs_left(void)
{
    if (!g.timer_active) return 0;
    long d = (long)(g.deadline - time(NULL));
    return d < 0 ? 0 : (int)d;
}

static void draw_cell(int z)
{
    if (g.zones[z]) printf(BOLD "[%d]" RST, z);
    else            printf(" %d ", z);
}

static void draw_menu(void)
{
    for (int i = 0; i < 4; i++) {
        if (!g.menu_name[i][0]) continue;
        printf("  " CYAN "[%d]" RST " %-20s", i + 1, g.menu_name[i]);
        if (g.menu_banned[i]) printf(DIM " (used last turn)" RST);
        printf("\n");
    }
}

static void render(void)
{
    printf(CLR);

    /* header + score */
    printf("==============  VOLLEYBALL  ==============\n");
    printf(" You (P%d): " GREEN "%d" RST "   |   Opponent: " RED "%d" RST
           "   (first to %d)\n", g.id, g.my_score, g.opp_score, WIN_SCORE);
    printf("------------------------------------------\n\n");

    /* turn banner */
    switch (g.mode) {
    case M_SERVE:
    case M_RECEIVE: printf(GREEN "[YOUR TURN - ATTACK]" RST "\n"); break;
    case M_DEFEND:  printf(GREEN "[YOUR TURN - DEFEND]" RST "\n"); break;
    case M_WAIT:    printf(RED   "[OPPONENT TURN]" RST "\n");      break;
    case M_OVER:    printf(CYAN  "[GAME OVER]" RST "\n");          break;
    }

    /* clue bubble */
    if (g.clue[0]) {
        int len = (int)strlen(g.clue);
        printf("\n  .");
        for (int i = 0; i < len + 4; i++) putchar('-');
        printf(".\n  |  \"%s\"  |\n  '", g.clue);
        for (int i = 0; i < len + 4; i++) putchar('-');
        printf("'\n");
    } else {
        printf("\n\n\n\n");
    }

    /* court */
    printf("\n  |=========================|\n");
    printf("  |  "); draw_cell(1); printf("  "); draw_cell(2); printf("  "); draw_cell(3); printf("  |\n");
    printf("  |                         |\n");
    printf("  |  "); draw_cell(4); printf("  "); draw_cell(5); printf("  "); draw_cell(6); printf("  |\n");
    printf("  |_________________________|\n\n");

    /* menu / prompt */
    switch (g.mode) {
    case M_SERVE:
        printf("SERVE:\n");   draw_menu();
        printf("\nPress 1-4 to choose.\n");
        break;
    case M_RECEIVE:
        printf("ATTACK:\n");  draw_menu();
        printf("\nPress 1-4 to choose.\n");
        break;
    case M_DEFEND:
        printf("Which zone will you protect?\n");
        printf("\nPress 1-6 to choose.\n");
        break;
    case M_WAIT:
        printf(DIM "Waiting for the opponent..." RST "\n\n\n");
        break;
    case M_OVER:
        if (g.won) printf(GREEN "*** YOU WIN! ***" RST "\n");
        else       printf(RED   "*** YOU LOSE ***" RST "\n");
        printf("Final score: %d - %d\n", g.my_score, g.opp_score);
        break;
    }

    /* last result line */
    printf("\n%s\n", g.result[0] ? g.result : " ");

    /* timer bar */
    if (g.timer_active && g.total_secs > 0) {
        int left   = secs_left();
        int filled = left * TIME_BAR / g.total_secs;
        printf("\nTIME [");
        for (int i = 0; i < TIME_BAR; i++) putchar(i < filled ? '#' : ' ');
        printf("] %2ds\n", left);
    }
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Server message handling                                            */
/* ------------------------------------------------------------------ */
static void start_timer(int secs)
{
    g.total_secs   = secs > 0 ? secs : 1;
    g.deadline     = time(NULL) + secs;
    g.timer_active = 1;
}

static void handle_line(char *line)
{
    char cmd[32] = "", arg[32] = "";
    int n1 = 0, n2 = 0;

    sscanf(line, "%31s%n", cmd, &n1);
    char *rest = line + n1;
    while (*rest == ' ') rest++;

    if (!strcmp(cmd, "HELLO")) {
        g.id = atoi(rest);
    } else if (!strcmp(cmd, "SCORE")) {
        sscanf(rest, "%d %d", &g.my_score, &g.opp_score);
    } else if (!strcmp(cmd, "MENU")) {
        int idx = 0, banned = 0, k = 0;
        if (sscanf(rest, "%d %d%n", &idx, &banned, &k) >= 2 && idx >= 1 && idx <= 4) {
            const char *name = rest + k;
            while (*name == ' ') name++;
            snprintf(g.menu_name[idx - 1], sizeof g.menu_name[0], "%s", name);
            g.menu_banned[idx - 1] = banned;
        }
    } else if (!strcmp(cmd, "TURN")) {
        int secs = 0;
        sscanf(rest, "%31s %d", arg, &secs);
        if      (!strcmp(arg, "SERVE"))   g.mode = M_SERVE;
        else if (!strcmp(arg, "RECEIVE")) g.mode = M_RECEIVE;
        else if (!strcmp(arg, "DEFEND"))  g.mode = M_DEFEND;
        start_timer(secs);
        /* when it is my attack turn the old clue is no longer relevant */
        if (g.mode != M_DEFEND) { g.clue[0] = '\0'; memset(g.zones, 0, sizeof g.zones); }
    } else if (!strcmp(cmd, "OPP")) {
        int secs = atoi(rest);
        g.mode = M_WAIT;
        start_timer(secs);
    } else if (!strcmp(cmd, "CLUE")) {
        snprintf(g.clue, sizeof g.clue, "%s", rest);
    } else if (!strcmp(cmd, "ZONES")) {
        int z;
        memset(g.zones, 0, sizeof g.zones);
        while (sscanf(rest, "%d%n", &z, &n2) == 1) {
            if (z >= 1 && z <= 6) g.zones[z] = 1;
            rest += n2;
        }
    } else if (!strcmp(cmd, "RESULT")) {
        snprintf(g.result, sizeof g.result, "%s", rest);
    } else if (!strcmp(cmd, "GAME_OVER")) {
        g.mode = M_OVER;
        g.timer_active = 0;
        g.won = (strncmp(rest, "WIN", 3) == 0);
    }
    render();
}

/* Reads from the socket, splits into lines. Returns 0 if the server closed. */
static int read_server(void)
{
    static char acc[BUF_SIZE];
    static size_t len = 0;

    ssize_t n = recv(sock, acc + len, sizeof acc - len - 1, 0);
    if (n <= 0) return 0;
    len += (size_t)n;
    acc[len] = '\0';

    char *start = acc, *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        *nl = '\0';
        if (nl > start && nl[-1] == '\r') nl[-1] = '\0';
        if (*start) handle_line(start);
        start = nl + 1;
    }
    len = strlen(start);
    memmove(acc, start, len + 1);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Keyboard handling                                                  */
/* ------------------------------------------------------------------ */
static void handle_key(char c)
{
    if (c == 'q' || c == 'Q') {
        send_line("QUIT");
        exit(0);
    }
    if (c < '1' || c > '6') return;
    int v = c - '0';

    if ((g.mode == M_SERVE || g.mode == M_RECEIVE) && v <= 4) {
        if (g.menu_banned[v - 1]) {
            snprintf(g.result, sizeof g.result,
                     "You can't use the same skill twice in a row.");
            render();
            return;
        }
        send_line("SKILL %d", v);
        snprintf(g.result, sizeof g.result, "You chose: %s", g.menu_name[v - 1]);
        g.mode = M_WAIT;
        g.timer_active = 0;
        render();
    } else if (g.mode == M_DEFEND) {
        send_line("ZONE %d", v);
        snprintf(g.result, sizeof g.result, "You protect zone %d", v);
        g.mode = M_WAIT;
        g.timer_active = 0;
        render();
    }
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-ip> <port>\n", argv[0]);
        return 1;
    }

    sock = connect_to(argv[1], argv[2]);
    if (sock < 0) {
        fprintf(stderr, "Could not connect to %s:%s (%s)\n",
                argv[1], argv[2], strerror(errno));
        return 1;
    }

    memset(&g, 0, sizeof g);
    g.mode = M_WAIT;
    snprintf(g.result, sizeof g.result, "Connected. Waiting for the game to start...");

    raw_terminal();
    render();

    int last_sec = -1;
    for (;;) {
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(sock, &rf);
        FD_SET(STDIN_FILENO, &rf);
        struct timeval tv = { 0, 200000 };   /* wake 5x/sec to update timer */

        int r = select(sock + 1, &rf, NULL, NULL, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(sock, &rf)) {
            if (!read_server()) {
                printf("\n" RED "Disconnected from server." RST "\n");
                break;
            }
        }
        if (FD_ISSET(STDIN_FILENO, &rf)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1) handle_key(c);
        }

        /* timer: redraw each second, report timeout on my turn */
        if (g.timer_active) {
            int left = secs_left();
            if (left != last_sec) {
                last_sec = left;
                render();
            }
            if (left == 0 && (g.mode == M_SERVE || g.mode == M_RECEIVE || g.mode == M_DEFEND)) {
                send_line("TIMEOUT");
                snprintf(g.result, sizeof g.result, "Time's up!");
                g.mode = M_WAIT;
                g.timer_active = 0;
                render();
            }
        }

        if (g.mode == M_OVER) {
            /* let the player read the result, then leave on any key / close */
            if (FD_ISSET(STDIN_FILENO, &rf)) break;
        }
    }

    close(sock);
    return 0;
}