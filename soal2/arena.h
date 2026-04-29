#ifndef ARENA_H
#define ARENA_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <pthread.h>

/*
XoXoXoXoXoXoXoXo
IPC Keys
XoXoXoXoXoXoXoXo
*/
#define SHM_KEY  0x00001234   // Shared memory key
#define MQ_KEY   0x00005678   // Message queue key
#define SEM_KEY  0x00009012   // Semaphore key

/*
XoXoXoXoXoXoXoXo
Konfigurasi Game
XoXoXoXoXoXoXoXo
*/
#define MAX_PLAYERS     20
#define MAX_NAME        32
#define MAX_PASS        32
#define MAX_HISTORY     50
#define MAX_LOG         5
#define LOG_LEN         64

#define BASE_DAMAGE     10
#define BASE_HEALTH     100
#define DEFAULT_GOLD    150
#define DEFAULT_LVL     1
#define DEFAULT_XP      0

#define MATCHMAKING_TIME 35
#define ATK_COOLDOWN     1.0
#define ULT_COOLDOWN     3.0

#define XP_WIN          50
#define XP_LOSS         15
#define GOLD_WIN        120
#define GOLD_LOSS       30

/*
XoXoXoXoXoXoXoXo
Senjata / Weapons
XoXoXoXoXoXoXoXo
*/
#define NUM_WEAPONS 5

typedef struct {
    char name[20];
    int price;
    int bonus_dmg;
} Weapon;

// Daftar senjata — dipakai oleh server dan client
static const Weapon WEAPON_LIST[NUM_WEAPONS] = {
    {"Wood Sword",   100,   5},
    {"Iron Sword",   300,  15},
    {"Steel Axe",    600,  30},
    {"Demon Blade", 1500,  60},
    {"God Slayer",  5000, 150}
};

/*
XoXoXoXoXoXoXoXo
Match History Entry
XoXoXoXoXoXoXoXo
*/
typedef struct {
    char time_str[16];    // "HH:MM"
    char opponent[MAX_NAME];
    int  won;             // 1 = WIN, 0 = LOSS
    int  xp_gained;
} MatchRecord;

/*
XoXoXoXoXoXoXoXo
Player Data (Persistent di Shared Memory)
XoXoXoXoXoXoXoXo
*/
typedef struct {
    char username[MAX_NAME];
    char password[MAX_PASS];
    int  gold;
    int  lvl;
    int  xp;
    int  weapon_id;       // -1 = belum punya, 0-4 = index di WEAPON_LIST
    int  active;          // 1 = akun ada, 0 = slot kosong
    int  logged_in;       // 1 = sedang login
    int  in_battle;       // 1 = sedang dalam pertempuran
    pid_t client_pid;     // PID dari eternal yang login

    // Match history
    MatchRecord history[MAX_HISTORY];
    int history_count;
} Player;

/*
XoXoXoXoXoXoXoXo
Battle Arena (di Shared Memory)
XoXoXoXoXoXoXoXo
*/
typedef struct {
    int active;               // 1 = pertempuran sedang berlangsung
    int player_idx[2];        // index ke array player (-1 = bot)
    char name[2][MAX_NAME];   // nama pemain
    int  lvl[2];
    int  max_hp[2];
    int  hp[2];
    int  damage[2];           // total damage per hit
    int  weapon_id[2];        // weapon yang dipakai (-1 = none)

    // Combat log — 5 baris teratas
    char log[MAX_LOG][LOG_LEN];
    int  log_count;

    // Cooldown timestamps (epoch seconds as double)
    double last_atk[2];
    double last_ult[2];

    int  winner;              // -1 = belum selesai, 0 atau 1
    int  finished;            // 1 = pertempuran selesai
} BattleArena;

/*
XoXoXoXoXoXoXoXo
Shared Memory Layout
XoXoXoXoXoXoXoXo
*/
typedef struct {
    Player players[MAX_PLAYERS];
    int player_count;

    // Matchmaking queue — simpan index player yang sedang cari lawan
    int  mm_queue[MAX_PLAYERS];
    int  mm_count;

    // Battle arenas
    BattleArena arenas[MAX_PLAYERS / 2];
    int arena_count;

    int server_running;
} GameState;

/*
XoXoXoXoXoXoXoXo
Message Queue Protocol
XoXoXoXoXoXoXoXo
*/
// mtype harus > 0 untuk System V message queue
// Kita pakai mtype = destination PID, supaya bisa routing

enum msg_action {
    ACT_REGISTER = 1,
    ACT_LOGIN,
    ACT_LOGOUT,
    ACT_BATTLE,
    ACT_ARMORY_BUY,
    ACT_HISTORY,
    ACT_MATCHMAKING_STATUS,
    ACT_BATTLE_ATTACK,
    ACT_BATTLE_ULTIMATE,
    ACT_BATTLE_STATUS,
    ACT_PING,             // cek server hidup

    // Response types
    ACT_RESP_OK = 100,
    ACT_RESP_FAIL,
    ACT_RESP_PROFILE,
    ACT_RESP_BATTLE_START,
    ACT_RESP_BATTLE_UPDATE,
    ACT_RESP_BATTLE_END,
    ACT_RESP_MATCHMAKING,
    ACT_RESP_HISTORY,
    ACT_RESP_ARMORY,
    ACT_RESP_PONG
};

#define MSG_DATA_SIZE 512

typedef struct {
    long mtype;            // destination PID (atau server PID)
    int  action;           // enum msg_action
    pid_t sender_pid;      // PID pengirim
    char data[MSG_DATA_SIZE];  // data payload (format tergantung action)
} MQMessage;

/*
XoXoXoXoXoXoXoXo
Semaphore Operations Helper
XoXoXoXoXoXoXoXo
*/
static inline void sem_lock(int sem_id)
{
    struct sembuf op = {0, -1, 0};
    semop(sem_id, &op, 1);
}

static inline void sem_unlock(int sem_id)
{
    struct sembuf op = {0, 1, 0};
    semop(sem_id, &op, 1);
}

#endif
