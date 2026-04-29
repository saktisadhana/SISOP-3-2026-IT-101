#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include "arena.h"

/*
XoXoXoXoXoXoXoXo
Global IPC handles
XoXoXoXoXoXoXoXo
*/
int shm_id, mq_id, sem_id;
GameState *gs = NULL;

/*
XoXoXoXoXoXoXoXo
Cleanup saat shutdown
XoXoXoXoXoXoXoXo
*/
void cleanup(int sig)
{
    (void)sig;
    if (gs) {
        gs->server_running = 0;
        shmdt(gs);
    }
    printf("\n> Orion shutting down...\n");
    exit(0);
}

/*
XoXoXoXoXoXoXoXo
Helper: cari player by username
XoXoXoXoXoXoXoXo
*/
int find_player(const char *username)
{
    for (int i = 0; i < gs->player_count; i++)
        if (gs->players[i].active && strcmp(gs->players[i].username, username) == 0)
            return i;
    return -1;
}

/*
XoXoXoXoXoXoXoXo
Helper: hitung damage player
XoXoXoXoXoXoXoXo
*/
int calc_damage(Player *p)
{
    int bonus = 0;
    if (p->weapon_id >= 0 && p->weapon_id < NUM_WEAPONS)
        bonus = WEAPON_LIST[p->weapon_id].bonus_dmg;
    return BASE_DAMAGE + (p->xp / 50) + bonus;
}

int calc_health(Player *p)
{
    return BASE_HEALTH + (p->xp / 10);
}

/*
XoXoXoXoXoXoXoXo
Helper: kirim response via MQ
XoXoXoXoXoXoXoXo
*/
void send_response(pid_t dest, int action, const char *data)
{
    MQMessage msg;
    msg.mtype = dest;
    msg.action = action;
    msg.sender_pid = getpid();
    memset(msg.data, 0, MSG_DATA_SIZE);
    if (data) strncpy(msg.data, data, MSG_DATA_SIZE - 1);
    msgsnd(mq_id, &msg, sizeof(MQMessage) - sizeof(long), 0);
}

/*
XoXoXoXoXoXoXoXo
Handle Register
Format data: "username\npassword"
XoXoXoXoXoXoXoXo
*/
void handle_register(MQMessage *req)
{
    char user[MAX_NAME], pass[MAX_PASS];
    sscanf(req->data, "%[^\n]\n%s", user, pass);

    sem_lock(sem_id);

    if (find_player(user) != -1) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Username already exists!");
        return;
    }

    if (gs->player_count >= MAX_PLAYERS) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Server full!");
        return;
    }

    int idx = gs->player_count++;
    Player *p = &gs->players[idx];
    memset(p, 0, sizeof(Player));
    strncpy(p->username, user, MAX_NAME - 1);
    strncpy(p->password, pass, MAX_PASS - 1);
    p->gold = DEFAULT_GOLD;
    p->lvl = DEFAULT_LVL;
    p->xp = DEFAULT_XP;
    p->weapon_id = -1;
    p->active = 1;
    p->logged_in = 0;
    p->in_battle = 0;
    p->history_count = 0;

    sem_unlock(sem_id);

    printf("> Account created: %s\n", user);
    send_response(req->sender_pid, ACT_RESP_OK, "Account created!");
}

/*
XoXoXoXoXoXoXoXo
Handle Login
XoXoXoXoXoXoXoXo
*/
void handle_login(MQMessage *req)
{
    char user[MAX_NAME], pass[MAX_PASS];
    sscanf(req->data, "%[^\n]\n%s", user, pass);

    sem_lock(sem_id);
    int idx = find_player(user);

    if (idx == -1) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "User not found!");
        return;
    }

    Player *p = &gs->players[idx];
    if (strcmp(p->password, pass) != 0) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Wrong password!");
        return;
    }

    if (p->logged_in) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Already logged in elsewhere!");
        return;
    }

    p->logged_in = 1;
    p->client_pid = req->sender_pid;

    // Build profile response: "idx\nusername\ngold\nlvl\nxp\nweapon_id"
    char resp[MSG_DATA_SIZE];
    snprintf(resp, sizeof(resp), "%d\n%s\n%d\n%d\n%d\n%d",
             idx, p->username, p->gold, p->lvl, p->xp, p->weapon_id);

    sem_unlock(sem_id);

    printf("> %s logged in (PID: %d)\n", user, req->sender_pid);
    send_response(req->sender_pid, ACT_RESP_PROFILE, resp);
}

/*
XoXoXoXoXoXoXoXo
Handle Logout
XoXoXoXoXoXoXoXo
*/
void handle_logout(MQMessage *req)
{
    int idx = atoi(req->data);
    sem_lock(sem_id);
    if (idx >= 0 && idx < gs->player_count && gs->players[idx].active) {
        gs->players[idx].logged_in = 0;
        gs->players[idx].in_battle = 0;
        gs->players[idx].client_pid = 0;
        printf("> %s logged out\n", gs->players[idx].username);
    }
    sem_unlock(sem_id);
    send_response(req->sender_pid, ACT_RESP_OK, "Logged out");
}

/*
XoXoXoXoXoXoXoXo
Handle Battle Request (Matchmaking)
data = "player_idx"
XoXoXoXoXoXoXoXo
*/
void handle_battle(MQMessage *req)
{
    int pidx = atoi(req->data);

    sem_lock(sem_id);

    // Cari lawan yang juga sedang menunggu matchmaking
    int opponent = -1;
    for (int i = 0; i < gs->mm_count; i++) {
        int oidx = gs->mm_queue[i];
        if (oidx != pidx && gs->players[oidx].active &&
            gs->players[oidx].logged_in && !gs->players[oidx].in_battle) {
            opponent = oidx;
            // Hapus dari queue
            for (int j = i; j < gs->mm_count - 1; j++)
                gs->mm_queue[j] = gs->mm_queue[j + 1];
            gs->mm_count--;
            break;
        }
    }

    if (opponent != -1) {
        // Match ditemukan! Buat arena
        int aidx = gs->arena_count++;
        BattleArena *a = &gs->arenas[aidx];
        memset(a, 0, sizeof(BattleArena));
        a->active = 1;
        a->player_idx[0] = opponent;
        a->player_idx[1] = pidx;

        for (int s = 0; s < 2; s++) {
            Player *pp = &gs->players[a->player_idx[s]];
            strncpy(a->name[s], pp->username, MAX_NAME - 1);
            a->lvl[s] = pp->lvl;
            a->max_hp[s] = calc_health(pp);
            a->hp[s] = a->max_hp[s];
            a->damage[s] = calc_damage(pp);
            a->weapon_id[s] = pp->weapon_id;
            a->last_atk[s] = 0;
            a->last_ult[s] = 0;
            pp->in_battle = 1;
        }
        a->winner = -1;
        a->finished = 0;
        a->log_count = 0;

        // Kirim ke kedua pemain
        char resp[MSG_DATA_SIZE];
        snprintf(resp, sizeof(resp), "%d", aidx);

        sem_unlock(sem_id);

        send_response(gs->players[opponent].client_pid, ACT_RESP_BATTLE_START, resp);
        send_response(req->sender_pid, ACT_RESP_BATTLE_START, resp);
    } else {
        // Masukkan ke antrian matchmaking
        gs->mm_queue[gs->mm_count++] = pidx;
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_MATCHMAKING, "searching");
    }
}

/*
XoXoXoXoXoXoXoXo
Handle Battle Attack
data = "arena_idx\nplayer_idx\ntype"
type: 0 = normal attack, 1 = ultimate
XoXoXoXoXoXoXoXo
*/
void handle_attack(MQMessage *req, int is_ult)
{
    int aidx, pidx;
    sscanf(req->data, "%d\n%d", &aidx, &pidx);

    sem_lock(sem_id);

    if (aidx < 0 || aidx >= gs->arena_count) {
        sem_unlock(sem_id);
        return;
    }

    BattleArena *a = &gs->arenas[aidx];
    if (a->finished) { sem_unlock(sem_id); return; }

    // Tentukan side (0 atau 1)
    int side = -1;
    if (a->player_idx[0] == pidx) side = 0;
    else if (a->player_idx[1] == pidx) side = 1;
    if (side == -1) { sem_unlock(sem_id); return; }

    int target = 1 - side;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;

    if (is_ult) {
        // Ultimate — butuh senjata
        if (a->weapon_id[side] < 0) { sem_unlock(sem_id); return; }
        if (now - a->last_ult[side] < ULT_COOLDOWN) { sem_unlock(sem_id); return; }
        int dmg = a->damage[side] * 3;
        a->hp[target] -= dmg;
        if (a->hp[target] < 0) a->hp[target] = 0;
        a->last_ult[side] = now;
        a->last_atk[side] = now;

        // Log
        if (a->log_count < MAX_LOG) {
            snprintf(a->log[a->log_count], LOG_LEN,
                     "%s ULTIMATE for %d damage!", a->name[side], dmg);
            a->log_count++;
        } else {
            for (int i = 0; i < MAX_LOG - 1; i++)
                strncpy(a->log[i], a->log[i + 1], LOG_LEN);
            snprintf(a->log[MAX_LOG - 1], LOG_LEN,
                     "%s ULTIMATE for %d damage!", a->name[side], dmg);
        }
    } else {
        // Normal attack — cek cooldown
        if (now - a->last_atk[side] < ATK_COOLDOWN) { sem_unlock(sem_id); return; }
        int dmg = a->damage[side];
        a->hp[target] -= dmg;
        if (a->hp[target] < 0) a->hp[target] = 0;
        a->last_atk[side] = now;

        if (a->log_count < MAX_LOG) {
            snprintf(a->log[a->log_count], LOG_LEN,
                     "%s hit for %d damage!", a->name[side], dmg);
            a->log_count++;
        } else {
            for (int i = 0; i < MAX_LOG - 1; i++)
                strncpy(a->log[i], a->log[i + 1], LOG_LEN);
            snprintf(a->log[MAX_LOG - 1], LOG_LEN,
                     "%s hit for %d damage!", a->name[side], dmg);
        }
    }

    // Cek apakah ada yang mati
    if (a->hp[target] <= 0) {
        a->winner = side;
        a->finished = 1;

        // Update stats kedua pemain
        for (int s = 0; s < 2; s++) {
            int pi = a->player_idx[s];
            if (pi < 0) continue; // bot
            Player *p = &gs->players[pi];
            int won = (s == a->winner) ? 1 : 0;
            p->xp += won ? XP_WIN : XP_LOSS;
            p->gold += won ? GOLD_WIN : GOLD_LOSS;
            p->lvl = 1 + (p->xp / 100);
            p->in_battle = 0;

            // Catat history
            if (p->history_count < MAX_HISTORY) {
                MatchRecord *r = &p->history[p->history_count++];
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                snprintf(r->time_str, sizeof(r->time_str), "%02d:%02d",
                         tm->tm_hour, tm->tm_min);
                strncpy(r->opponent, a->name[1 - s], MAX_NAME - 1);
                r->won = won;
                r->xp_gained = won ? XP_WIN : XP_LOSS;
            }
        }
    }

    sem_unlock(sem_id);

    // Kirim update ke kedua pemain
    char resp[MSG_DATA_SIZE];
    snprintf(resp, sizeof(resp), "%d", aidx);
    for (int s = 0; s < 2; s++) {
        int pi = a->player_idx[s];
        if (pi >= 0 && gs->players[pi].client_pid > 0) {
            if (a->finished)
                send_response(gs->players[pi].client_pid, ACT_RESP_BATTLE_END, resp);
            else
                send_response(gs->players[pi].client_pid, ACT_RESP_BATTLE_UPDATE, resp);
        }
    }
}

/*
XoXoXoXoXoXoXoXo
Handle Armory Buy
data = "player_idx\nweapon_idx"
XoXoXoXoXoXoXoXo
*/
void handle_armory(MQMessage *req)
{
    int pidx, widx;
    sscanf(req->data, "%d\n%d", &pidx, &widx);

    sem_lock(sem_id);

    if (pidx < 0 || pidx >= gs->player_count) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Invalid player");
        return;
    }

    Player *p = &gs->players[pidx];

    if (widx < 0 || widx >= NUM_WEAPONS) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Invalid weapon");
        return;
    }

    if (p->gold < WEAPON_LIST[widx].price) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Not enough gold!");
        return;
    }

    p->gold -= WEAPON_LIST[widx].price;

    // Auto-equip senjata dengan damage terbesar
    if (p->weapon_id < 0 || WEAPON_LIST[widx].bonus_dmg > WEAPON_LIST[p->weapon_id].bonus_dmg)
        p->weapon_id = widx;

    char resp[MSG_DATA_SIZE];
    snprintf(resp, sizeof(resp), "%d\n%d\n%d", p->gold, p->weapon_id, WEAPON_LIST[widx].bonus_dmg);

    sem_unlock(sem_id);

    printf("> %s bought %s\n", p->username, WEAPON_LIST[widx].name);
    send_response(req->sender_pid, ACT_RESP_ARMORY, resp);
}

/*
XoXoXoXoXoXoXoXo
Handle History Request
XoXoXoXoXoXoXoXo
*/
void handle_history(MQMessage *req)
{
    int pidx = atoi(req->data);

    sem_lock(sem_id);

    if (pidx < 0 || pidx >= gs->player_count) {
        sem_unlock(sem_id);
        send_response(req->sender_pid, ACT_RESP_FAIL, "Invalid");
        return;
    }

    Player *p = &gs->players[pidx];
    char resp[MSG_DATA_SIZE];
    int off = 0;

    off += snprintf(resp + off, MSG_DATA_SIZE - off, "%d\n", p->history_count);
    // Kirim dari terbaru ke terlama
    for (int i = p->history_count - 1; i >= 0 && off < MSG_DATA_SIZE - 80; i--) {
        MatchRecord *r = &p->history[i];
        off += snprintf(resp + off, MSG_DATA_SIZE - off, "%s|%s|%s|%d\n",
                        r->time_str, r->opponent,
                        r->won ? "WIN" : "LOSS", r->xp_gained);
    }

    sem_unlock(sem_id);
    send_response(req->sender_pid, ACT_RESP_HISTORY, resp);
}

/*
XoXoXoXoXoXoXoXo
Handle Matchmaking timeout — spawn bot battle
XoXoXoXoXoXoXoXo
*/
void spawn_bot_battle(int pidx)
{
    sem_lock(sem_id);

    Player *p = &gs->players[pidx];
    int aidx = gs->arena_count++;
    BattleArena *a = &gs->arenas[aidx];
    memset(a, 0, sizeof(BattleArena));
    a->active = 1;

    // Slot 0 = bot (Wild Beast)
    a->player_idx[0] = -1;
    strncpy(a->name[0], "Wild Beast", MAX_NAME - 1);
    a->lvl[0] = p->lvl;
    a->max_hp[0] = calc_health(p);
    a->hp[0] = a->max_hp[0];
    a->damage[0] = BASE_DAMAGE + (p->xp / 50);
    a->weapon_id[0] = -1;

    // Slot 1 = pemain
    a->player_idx[1] = pidx;
    strncpy(a->name[1], p->username, MAX_NAME - 1);
    a->lvl[1] = p->lvl;
    a->max_hp[1] = calc_health(p);
    a->hp[1] = a->max_hp[1];
    a->damage[1] = calc_damage(p);
    a->weapon_id[1] = p->weapon_id;

    a->winner = -1;
    a->finished = 0;
    a->log_count = 0;
    p->in_battle = 1;

    pid_t cpid = p->client_pid;
    sem_unlock(sem_id);

    char resp[MSG_DATA_SIZE];
    snprintf(resp, sizeof(resp), "%d", aidx);
    send_response(cpid, ACT_RESP_BATTLE_START, resp);
    printf("> %s fighting Wild Beast (bot)\n", p->username);
}

/*
XoXoXoXoXoXoXoXo
Bot AI thread — bot attacks periodically
XoXoXoXoXoXoXoXo
*/
void *bot_ai_thread(void *arg)
{
    (void)arg;
    while (gs->server_running) {
        usleep(1500000); // 1.5 detik

        sem_lock(sem_id);
        for (int i = 0; i < gs->arena_count; i++) {
            BattleArena *a = &gs->arenas[i];
            if (!a->active || a->finished) continue;

            // Cek apakah ada bot (player_idx == -1)
            int bot_side = -1;
            if (a->player_idx[0] == -1) bot_side = 0;
            if (a->player_idx[1] == -1) bot_side = 1;
            if (bot_side == -1) continue;

            int target = 1 - bot_side;
            int dmg = a->damage[bot_side];
            a->hp[target] -= dmg;
            if (a->hp[target] < 0) a->hp[target] = 0;

            // Log
            if (a->log_count < MAX_LOG) {
                snprintf(a->log[a->log_count], LOG_LEN,
                         "%s hit for %d damage!", a->name[bot_side], dmg);
                a->log_count++;
            } else {
                for (int j = 0; j < MAX_LOG - 1; j++)
                    strncpy(a->log[j], a->log[j + 1], LOG_LEN);
                snprintf(a->log[MAX_LOG - 1], LOG_LEN,
                         "%s hit for %d damage!", a->name[bot_side], dmg);
            }

            int player_side = target;
            int pi = a->player_idx[player_side];

            // Cek kematian
            if (a->hp[target] <= 0) {
                a->winner = bot_side;
                a->finished = 1;

                // Update player stats (kalah)
                if (pi >= 0) {
                    Player *p = &gs->players[pi];
                    p->xp += XP_LOSS;
                    p->gold += GOLD_LOSS;
                    p->lvl = 1 + (p->xp / 100);
                    p->in_battle = 0;

                    if (p->history_count < MAX_HISTORY) {
                        MatchRecord *r = &p->history[p->history_count++];
                        time_t t = time(NULL);
                        struct tm *tm = localtime(&t);
                        snprintf(r->time_str, sizeof(r->time_str), "%02d:%02d",
                                 tm->tm_hour, tm->tm_min);
                        strncpy(r->opponent, a->name[bot_side], MAX_NAME - 1);
                        r->won = 0;
                        r->xp_gained = XP_LOSS;
                    }

                    if (p->client_pid > 0) {
                        char resp[MSG_DATA_SIZE];
                        snprintf(resp, sizeof(resp), "%d", i);
                        sem_unlock(sem_id);
                        send_response(p->client_pid, ACT_RESP_BATTLE_END, resp);
                        goto bot_continue;
                    }
                }
            } else {
                // Kirim update
                if (pi >= 0 && gs->players[pi].client_pid > 0) {
                    char resp[MSG_DATA_SIZE];
                    snprintf(resp, sizeof(resp), "%d", i);
                    sem_unlock(sem_id);
                    send_response(gs->players[pi].client_pid, ACT_RESP_BATTLE_UPDATE, resp);
                    goto bot_continue;
                }
            }
        }
        sem_unlock(sem_id);
        bot_continue:;
    }
    return NULL;
}

/*
XoXoXoXoXoXoXoXo
Handle Battle Status (client polling arena state)
XoXoXoXoXoXoXoXo
*/
void handle_battle_status(MQMessage *req)
{
    int aidx = atoi(req->data);
    sem_lock(sem_id);
    if (aidx >= 0 && aidx < gs->arena_count) {
        BattleArena *a = &gs->arenas[aidx];
        if (a->finished) {
            char resp[MSG_DATA_SIZE];
            snprintf(resp, sizeof(resp), "%d", aidx);
            sem_unlock(sem_id);
            send_response(req->sender_pid, ACT_RESP_BATTLE_END, resp);
            return;
        }
    }
    sem_unlock(sem_id);
}

/*
XoXoXoXoXoXoXoXo
Main — setup IPC dan event loop
XoXoXoXoXoXoXoXo
*/
int main()
{
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Buat shared memory
    shm_id = shmget(SHM_KEY, sizeof(GameState), IPC_CREAT | 0666);
    if (shm_id < 0) { perror("shmget"); exit(1); }
    gs = (GameState *)shmat(shm_id, NULL, 0);
    if (gs == (void *)-1) { perror("shmat"); exit(1); }

    // Init game state
    memset(gs, 0, sizeof(GameState));
    gs->server_running = 1;

    // Buat message queue
    mq_id = msgget(MQ_KEY, IPC_CREAT | 0666);
    if (mq_id < 0) { perror("msgget"); exit(1); }

    // Buat semaphore
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id < 0) { perror("semget"); exit(1); }
    semctl(sem_id, 0, SETVAL, 1); // init ke 1 (unlocked)

    printf("Orion is ready (PID: %d)\n", getpid());

    // Start bot AI thread
    pthread_t bot_tid;
    pthread_create(&bot_tid, NULL, bot_ai_thread, NULL);
    pthread_detach(bot_tid);

    // Event loop — terima pesan dari MQ
    MQMessage req;
    while (gs->server_running)
    {
        // Terima pesan yang ditujukan ke server (mtype = getpid())
        // Pakai mtype = 1 sebagai channel server
        ssize_t ret = msgrcv(mq_id, &req, sizeof(MQMessage) - sizeof(long), 1, IPC_NOWAIT);

        if (ret < 0) {
            if (errno == ENOMSG) {
                // Cek matchmaking timeout — jika ada player di queue > 35 detik, spawn bot
                // Simplified: kita cek setiap loop
                usleep(50000); // 50ms
                continue;
            }
            if (errno == EINTR) continue;
            perror("msgrcv");
            break;
        }

        switch (req.action)
        {
            case ACT_REGISTER:       handle_register(&req);         break;
            case ACT_LOGIN:          handle_login(&req);            break;
            case ACT_LOGOUT:         handle_logout(&req);           break;
            case ACT_BATTLE:         handle_battle(&req);           break;
            case ACT_BATTLE_ATTACK:  handle_attack(&req, 0);        break;
            case ACT_BATTLE_ULTIMATE: handle_attack(&req, 1);       break;
            case ACT_BATTLE_STATUS:  handle_battle_status(&req);    break;
            case ACT_ARMORY_BUY:     handle_armory(&req);           break;
            case ACT_HISTORY:        handle_history(&req);          break;
            case ACT_PING:
                send_response(req.sender_pid, ACT_RESP_PONG, "pong");
                break;
            default: break;
        }
    }

    cleanup(0);
    return 0;
}
