#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/msg.h>
#include <errno.h>
#include "arena.h"

/*
XoXoXoXoXoXoXoXo
Global Variables
XoXoXoXoXoXoXoXo
*/
int mq_id;
int logged_in = 0;
int player_idx = -1;
char my_username[MAX_NAME];

int current_gold = 0;
int current_lvl = 0;
int current_xp = 0;
int current_weapon = -1;

// Raw terminal manipulation for realtime battle
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/*
XoXoXoXoXoXoXoXo
Helper: Clear screen
XoXoXoXoXoXoXoXo
*/
void clear_screen() {
    printf("\033[H\033[J");
}

/*
XoXoXoXoXoXoXoXo
Helper: Send MQ request
XoXoXoXoXoXoXoXo
*/
void send_request(int action, const char *data) {
    MQMessage msg;
    msg.mtype = 1; // Server listens on mtype 1
    msg.action = action;
    msg.sender_pid = getpid();
    memset(msg.data, 0, MSG_DATA_SIZE);
    if (data) strncpy(msg.data, data, MSG_DATA_SIZE - 1);
    msgsnd(mq_id, &msg, sizeof(MQMessage) - sizeof(long), 0);
}

/*
XoXoXoXoXoXoXoXo
Helper: Receive MQ response
XoXoXoXoXoXoXoXo
*/
int receive_response(MQMessage *resp, int timeout) {
    if (timeout) {
        // Simple polling with timeout
        for (int i = 0; i < timeout * 10; i++) {
            if (msgrcv(mq_id, resp, sizeof(MQMessage) - sizeof(long), getpid(), IPC_NOWAIT) != -1) {
                return 1;
            }
            usleep(100000); // 100ms
        }
        return 0; // Timeout
    } else {
        // Block
        return msgrcv(mq_id, resp, sizeof(MQMessage) - sizeof(long), getpid(), 0) != -1;
    }
}

/*
XoXoXoXoXoXoXoXo
UI: Draw ASCII Art
XoXoXoXoXoXoXoXo
*/
void print_banner() {
    printf("\n");
    printf(" | __ ) / \\| |   _   | |   ____| / _ \\| \\ | | \n");
    printf(" |  _ \\/ _ \\ |  | | |  | |  |  _|  | | | | \\| | \n");
    printf(" | |_) / ___ \\ |__| |__| |__| |___| |_| | |\\  | \n");
    printf(" |____/_/   \\_\\_____|_____|_____|_____|\\___/|_| \\_| \n");
    printf(" | ____| |   | ____|  _ \\|_ _|/ _ \\| \\ | | \n");
    printf(" |  _| | |   |  _| | |_) || || | | |  \\| | \n");
    printf(" | |___| |___| |___|  _ < | || |_| | |\\  | \n");
    printf(" |_____|_____|_____|_| \\_\\___|\\___/|_| \\_| \n\n");
}

/*
XoXoXoXoXoXoXoXo
Menu Login/Register
XoXoXoXoXoXoXoXo
*/
void show_main_menu() {
    clear_screen();
    print_banner();
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Exit\n");
    printf("Choice: ");
}

/*
XoXoXoXoXoXoXoXo
Menu Utama Game
XoXoXoXoXoXoXoXo
*/
void show_game_menu() {
    clear_screen();
    print_banner();
    printf("====== PROFILE ======\n");
    printf("Name  : %-15s Lvl : %d\n", my_username, current_lvl);
    printf("Gold  : %-15d XP  : %d\n", current_gold, current_xp);
    printf("Weapon: %s\n", current_weapon >= 0 ? WEAPON_LIST[current_weapon].name : "None");
    printf("=====================\n\n");
    printf("1. Battle\n");
    printf("2. Armory\n");
    printf("3. History\n");
    printf("4. Logout\n");
    printf("Choice: ");
}

/*
XoXoXoXoXoXoXoXo
Action: Register
XoXoXoXoXoXoXoXo
*/
void do_register() {
    char user[MAX_NAME], pass[MAX_PASS];
    printf("\nCREATE ACCOUNT\n");
    printf("Username: ");
    scanf("%s", user);
    printf("Password: ");
    scanf("%s", pass);

    char data[MSG_DATA_SIZE];
    snprintf(data, sizeof(data), "%s\n%s", user, pass);
    send_request(ACT_REGISTER, data);

    MQMessage resp;
    receive_response(&resp, 0);
    if (resp.action == ACT_RESP_OK) {
        printf("\nAccount created!\n");
    } else {
        printf("\nError: %s\n", resp.data);
    }
    printf("Press any key to continue...\n");
    getchar(); getchar();
}

/*
XoXoXoXoXoXoXoXo
Action: Login
XoXoXoXoXoXoXoXo
*/
void do_login() {
    char user[MAX_NAME], pass[MAX_PASS];
    printf("\nLOGIN\n");
    printf("Username: ");
    scanf("%s", user);
    printf("Password: ");
    scanf("%s", pass);

    char data[MSG_DATA_SIZE];
    snprintf(data, sizeof(data), "%s\n%s", user, pass);
    send_request(ACT_LOGIN, data);

    MQMessage resp;
    receive_response(&resp, 0);
    if (resp.action == ACT_RESP_PROFILE) {
        sscanf(resp.data, "%d\n%[^\n]\n%d\n%d\n%d\n%d", 
               &player_idx, my_username, &current_gold, &current_lvl, &current_xp, &current_weapon);
        logged_in = 1;
        printf("\nWelcome!\n");
    } else {
        printf("\nError: %s\n", resp.data);
    }
    printf("Press any key to continue...\n");
    getchar(); getchar();
}

/*
XoXoXoXoXoXoXoXo
Action: Battle Mode
XoXoXoXoXoXoXoXo
*/
void draw_battle_ui(const char* data, int arena_idx) {
    clear_screen();
    // Parse data from server. 
    // Format is complex, so simpler approach: rely on the server to update via shared memory 
    // But we are required to use IPC MQ for communication. 
    // Let's attach to SHM only for reading the arena state to draw it properly since it's realtime.
    // That's the most efficient way to do realtime.
    
    int shm_id = shmget(SHM_KEY, sizeof(GameState), 0666);
    if (shm_id < 0) return;
    GameState *gs = (GameState *)shmat(shm_id, NULL, 0);
    if (gs == (void *)-1) return;

    BattleArena *a = &gs->arenas[arena_idx];
    
    int side = (a->player_idx[0] == player_idx) ? 0 : 1;
    int target = 1 - side;

    printf("\n==== ARENA ====\n\n");
    
    // Draw Enemy
    printf("%-20s Lvl %d\n", a->name[target], a->lvl[target]);
    printf("[");
    int hp_bars = (a->hp[target] * 20) / a->max_hp[target];
    for (int i=0; i<20; i++) {
        if (i < hp_bars) printf("#"); else printf("-");
    }
    printf("] %d/%d\n\n", a->hp[target], a->max_hp[target]);

    printf("      VS      \n\n");

    // Draw Player
    printf("%-20s Lvl %d | Weapon: %s\n", a->name[side], a->lvl[side], 
        (a->weapon_id[side] >= 0) ? WEAPON_LIST[a->weapon_id[side]].name : "None");
    printf("[");
    hp_bars = (a->hp[side] * 20) / a->max_hp[side];
    for (int i=0; i<20; i++) {
        if (i < hp_bars) printf("#"); else printf("-");
    }
    printf("] %d/%d\n\n", a->hp[side], a->max_hp[side]);

    // Draw Combat Log
    printf("Combat Log:\n");
    for (int i=0; i<MAX_LOG; i++) {
        if (strlen(a->log[i]) > 0) {
            printf("> %s\n", a->log[i]);
        } else {
            printf(">\n");
        }
    }
    printf("\nPress 'a' to Attack | 'u' for Ultimate\n");

    shmdt(gs);
}

void do_battle() {
    printf("\nJoining Matchmaking...\n");
    char req_data[MSG_DATA_SIZE];
    snprintf(req_data, sizeof(req_data), "%d", player_idx);
    send_request(ACT_BATTLE, req_data);

    MQMessage resp;
    receive_response(&resp, 0);

    if (resp.action == ACT_RESP_MATCHMAKING) {
        printf("Searching for an opponent... [%d s]\n", MATCHMAKING_TIME);
        // Wait for BATTLE_START
        while(1) {
            if (receive_response(&resp, 1)) {
                if (resp.action == ACT_RESP_BATTLE_START) break;
            }
            // Check if server timeout gives us a bot
        }
    } 
    
    if (resp.action != ACT_RESP_BATTLE_START) {
        return;
    }

    int arena_idx = atoi(resp.data);
    
    enable_raw_mode();
    
    // Non-blocking stdin for realtime input
    // The main thread will draw UI and poll input
    int in_battle = 1;
    while(in_battle) {
        draw_battle_ui("", arena_idx);

        char c = '\0';
        // Check keyboard input
        struct timeval tv = {0, 50000}; // 50ms
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
        
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            read(STDIN_FILENO, &c, 1);
            if (c == 'a' || c == 'A') {
                snprintf(req_data, sizeof(req_data), "%d\n%d", arena_idx, player_idx);
                send_request(ACT_BATTLE_ATTACK, req_data);
            } else if (c == 'u' || c == 'U') {
                snprintf(req_data, sizeof(req_data), "%d\n%d", arena_idx, player_idx);
                send_request(ACT_BATTLE_ULTIMATE, req_data);
            }
        }

        // Check message queue for battle end or updates
        if (msgrcv(mq_id, &resp, sizeof(MQMessage) - sizeof(long), getpid(), IPC_NOWAIT) != -1) {
            if (resp.action == ACT_RESP_BATTLE_END) {
                in_battle = 0;
            }
        }
        
        // Also check server status just in case
        int shm_id = shmget(SHM_KEY, sizeof(GameState), 0666);
        if (shm_id >= 0) {
            GameState *gs = (GameState *)shmat(shm_id, NULL, 0);
            if (gs != (void *)-1) {
                if (gs->arenas[arena_idx].finished) in_battle = 0;
                shmdt(gs);
            }
        }
    }

    disable_raw_mode();
    
    // Draw final state
    draw_battle_ui("", arena_idx);

    // Refresh profile to update XP/Gold
    snprintf(req_data, sizeof(req_data), "%s\n", my_username); // dummy pass since we know we're in
    // Read from SHM is faster
    int shm_id = shmget(SHM_KEY, sizeof(GameState), 0666);
    if (shm_id >= 0) {
        GameState *gs = (GameState *)shmat(shm_id, NULL, 0);
        if (gs != (void *)-1) {
            current_gold = gs->players[player_idx].gold;
            current_lvl = gs->players[player_idx].lvl;
            current_xp = gs->players[player_idx].xp;
            
            int won = (gs->arenas[arena_idx].winner >= 0 && gs->arenas[arena_idx].player_idx[gs->arenas[arena_idx].winner] == player_idx);
            
            printf("\n");
            if (won) {
                printf("=== VICTORY ===\n");
            } else {
                printf("=== DEFEAT ===\n");
            }
            printf("Battle ended. Press [ENTER] to continue...\n");
            
            shmdt(gs);
        }
    }

    char dummy[10];
    fgets(dummy, sizeof(dummy), stdin); // clear buffer if any
    getchar(); // wait
}

/*
XoXoXoXoXoXoXoXo
Action: Armory
XoXoXoXoXoXoXoXo
*/
void do_armory() {
    clear_screen();
    printf("=== ARMORY ===\n");
    printf("Gold: %d\n", current_gold);
    for (int i=0; i<NUM_WEAPONS; i++) {
        printf("%d. %-15s | %5d G | +%3d Dmg\n", i+1, WEAPON_LIST[i].name, WEAPON_LIST[i].price, WEAPON_LIST[i].bonus_dmg);
    }
    printf("0. Back | Choice: ");
    
    int choice;
    if (scanf("%d", &choice) == 1 && choice > 0 && choice <= NUM_WEAPONS) {
        char req[MSG_DATA_SIZE];
        snprintf(req, sizeof(req), "%d\n%d", player_idx, choice-1);
        send_request(ACT_ARMORY_BUY, req);
        
        MQMessage resp;
        receive_response(&resp, 0);
        if (resp.action == ACT_RESP_ARMORY) {
            int bonus;
            sscanf(resp.data, "%d\n%d\n%d", &current_gold, &current_weapon, &bonus);
            printf("\nWeapon purchased!\n");
        } else {
            printf("\nError: %s\n", resp.data);
        }
        printf("Press [ENTER] to continue...\n");
        getchar(); getchar();
    }
}

/*
XoXoXoXoXoXoXoXo
Action: History
XoXoXoXoXoXoXoXo
*/
void do_history() {
    clear_screen();
    printf("=== MATCH HISTORY ===\n");
    
    char req[MSG_DATA_SIZE];
    snprintf(req, sizeof(req), "%d", player_idx);
    send_request(ACT_HISTORY, req);
    
    MQMessage resp;
    receive_response(&resp, 0);
    
    if (resp.action == ACT_RESP_HISTORY) {
        int count;
        char *line = strtok(resp.data, "\n");
        if (line) {
            count = atoi(line);
            printf("%-10s | %-15s | %-6s | %s\n", "Time", "Opponent", "Res", "XP");
            printf("--------------------------------------------------\n");
            line = strtok(NULL, "\n");
            while (line) {
                char time_str[16], opp[MAX_NAME], res[10];
                int xp;
                sscanf(line, "%[^|]|%[^|]|%[^|]|%d", time_str, opp, res, &xp);
                printf("%-10s | %-15s | %-6s | +%-4d XP\n", time_str, opp, res, xp);
                line = strtok(NULL, "\n");
            }
        }
    }
    
    printf("\nPress [ENTER] to continue...\n");
    getchar(); getchar();
}

/*
XoXoXoXoXoXoXoXo
Action: Logout
XoXoXoXoXoXoXoXo
*/
void do_logout() {
    char req[MSG_DATA_SIZE];
    snprintf(req, sizeof(req), "%d", player_idx);
    send_request(ACT_LOGOUT, req);
    
    MQMessage resp;
    receive_response(&resp, 0);
    logged_in = 0;
    player_idx = -1;
}

/*
XoXoXoXoXoXoXoXo
SIGINT Handler
XoXoXoXoXoXoXoXo
*/
void sigint_handler(int sig) {
    (void)sig;
    if (logged_in) do_logout();
    printf("\n[System] Disconnecting from Eterion...\n");
    exit(0);
}

/*
XoXoXoXoXoXoXoXo
Main Program
XoXoXoXoXoXoXoXo
*/
int main() {
    signal(SIGINT, sigint_handler);

    mq_id = msgget(MQ_KEY, 0666);
    if (mq_id < 0) {
        printf("Orion are you there?\n");
        exit(1);
    }

    // Ping Orion
    send_request(ACT_PING, "ping");
    MQMessage resp;
    if (!receive_response(&resp, 2)) {
        printf("Orion are you there?\n");
        exit(1);
    }

    while(1) {
        if (!logged_in) {
            show_main_menu();
            int choice = 0;
            if (scanf("%d", &choice) != 1) {
                while(getchar() != '\n');
                continue;
            }
            switch(choice) {
                case 1: do_register(); break;
                case 2: do_login(); break;
                case 3: sigint_handler(0); break;
            }
        } else {
            show_game_menu();
            int choice = 0;
            if (scanf("%d", &choice) != 1) {
                while(getchar() != '\n');
                continue;
            }
            switch(choice) {
                case 1: do_battle(); break;
                case 2: do_armory(); break;
                case 3: do_history(); break;
                case 4: do_logout(); break;
            }
        }
    }

    return 0;
}
