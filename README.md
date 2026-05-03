# SISOP-3-2026-IT-101

## Identitas Praktikan

| Nama                     | NRP        | Kode Asisten | Kelas |
| ------------------------ | ---------- | ------------ | ----- |
| Putu Putra Sakti Sadhana | 5027251101 | NINN         | A     |
## Reporting

### Soal 1

Pada soal ini praktikan diperintahkan untuk membuat suatu **Message Passing RPC** yang dimana `protocol.h` merupakan header untuk mendefinisikan struktur Message dan juga port yang bisa digunakan oleh client (`navi.c`) untuk terhubung dengan server (`wired.c`) 

#### Penjelasan

Sesuai dengan struktur folder soal dan juga instruksi dari soal, praktikan harus membuat 3 file.

##### protocol.h

##### wired.c

##### navi.c

#### Output
1. Menjalankan `wired.c`
![[Pasted image 20260503184458.png]]

2. Menjalankan `navi.c` (User Pertama)
![[Pasted image 20260503184946.png]]

3. User Pertama terkoneksi
![[Pasted image 20260503184655.png]]

4. Menjalankan `navi.c` (User Kedua)
![[Pasted image 20260503185012.png]]

5. User Kedua terkoneksi
![[Pasted image 20260503185039.png]]

6. Chat dari user lain
![[Pasted image 20260503185123.png]]

![[Pasted image 20260503185136.png]]

7. Nama yang sama
![[Pasted image 20260503185314.png]]

8. login to `The Knights`
![[Pasted image 20260503185543.png]]

9. Opsi 1 console `The Knights`
![[Pasted image 20260503185840.png]]

10. Opsi 2 console `The Knights`
![[Pasted image 20260503185857.png]]

11. Opsi 3 console `The Knights`
![[Pasted image 20260503190006.png]]

12. Opsi 4 console `The Knights`
![[Pasted image 20260503190443.png]]

![[Pasted image 20260503190459.png]]

13. Selain 4 opsi tersebut
![[Pasted image 20260503190541.png]]

14. `history.log`
![[Pasted image 20260503190616.png]]

#### Kendala

Tidak ada kendala.
### Soal 2

Pada soal ini praktikan diinstruksikan untuk membuat suatu **Battle Arena Multiplayer** bernama **Eterion** menggunakan **System V IPC** (Shared Memory, Message Queue, dan Semaphore). Server (`orion.c`) mengelola seluruh game state sedangkan client (`eternal.c`) digunakan oleh pemain untuk berinteraksi.

#### Penjelasan

Sesuai dengan struktur folder soal, praktikan harus membuat 3 file:

1. `arena.h` — Header file yang mendefinisikan seluruh struktur data dan protokol IPC.
2. `orion.c` — Server yang mengelola game state, matchmaking, battle, dan armory.
3. `eternal.c` — Client yang digunakan pemain untuk register, login, battle, dll.

##### arena.h

Header file ini berfungsi sebagai "kontrak" antara server dan client. Di dalamnya terdapat definisi IPC keys, konfigurasi game, dan seluruh struktur data yang digunakan.

###### IPC Keys

```c
#define SHM_KEY  0x00001234   // Shared memory key
#define MQ_KEY   0x00005678   // Message queue key
#define SEM_KEY  0x00009012   // Semaphore key
```

Tiga key ini digunakan agar server dan client bisa mengakses resource IPC yang sama. `SHM_KEY` untuk shared memory, `MQ_KEY` untuk message queue, dan `SEM_KEY` untuk semaphore.

###### Konfigurasi Game

```c
#define MAX_PLAYERS     20
#define BASE_DAMAGE     10
#define BASE_HEALTH     100
#define DEFAULT_GOLD    150

#define ATK_COOLDOWN     1.0
#define ULT_COOLDOWN     3.0

#define XP_WIN          50
#define GOLD_WIN        120
```

Konstanta-konstanta ini mengatur parameter game seperti jumlah maksimal pemain, damage dasar, health dasar, gold awal, cooldown serangan, serta reward menang/kalah.

###### Struktur Data Player

```c
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

    MatchRecord history[MAX_HISTORY];
    int history_count;
} Player;
```

Struct `Player` menyimpan seluruh data pemain secara persisten di shared memory. Field `logged_in` dan `in_battle` digunakan untuk mencegah double login dan mencegah pemain melakukan aksi lain saat sedang bertarung.

###### Struktur Data BattleArena

```c
typedef struct {
    int active;
    int player_idx[2];        // index ke array player (-1 = bot)
    char name[2][MAX_NAME];
    int  hp[2];
    int  max_hp[2];
    int  damage[2];
    int  weapon_id[2];

    char log[MAX_LOG][LOG_LEN];
    int  log_count;

    double last_atk[2];
    double last_ult[2];

    int  winner;              // -1 = belum selesai, 0 atau 1
    int  finished;
} BattleArena;
```

Setiap pertempuran memiliki arena sendiri yang menyimpan state kedua pemain (HP, damage, cooldown). Field `last_atk` dan `last_ult` menyimpan timestamp terakhir serangan untuk mengatur cooldown.

###### Shared Memory Layout

```c
typedef struct {
    Player players[MAX_PLAYERS];
    int player_count;

    int  mm_queue[MAX_PLAYERS];
    int  mm_count;

    BattleArena arenas[MAX_PLAYERS / 2];
    int arena_count;

    int server_running;
} GameState;
```

Seluruh game state disimpan dalam satu struct `GameState` yang dialokasikan di shared memory. Di dalamnya terdapat array pemain, antrian matchmaking (`mm_queue`), dan array arena pertempuran.

###### Message Queue Protocol

```c
enum msg_action {
    ACT_REGISTER = 1,
    ACT_LOGIN,
    ACT_LOGOUT,
    ACT_BATTLE,
    ACT_ARMORY_BUY,
    ACT_HISTORY,
    ...
    ACT_RESP_OK = 100,
    ACT_RESP_FAIL,
    ACT_RESP_PROFILE,
    ACT_RESP_BATTLE_START,
    ...
};

typedef struct {
    long mtype;            // destination PID (atau server PID)
    int  action;           // enum msg_action
    pid_t sender_pid;      // PID pengirim
    char data[MSG_DATA_SIZE];
} MQMessage;
```

Komunikasi antara server dan client menggunakan message queue. Field `mtype` diisi dengan PID tujuan agar setiap client hanya menerima pesan yang ditujukan kepadanya. Server mendengarkan pada `mtype = 1`.

###### Semaphore Helper

```c
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
```

Dua fungsi inline ini digunakan untuk mengunci dan membuka semaphore. `sem_lock` menurunkan nilai semaphore (mengunci), sedangkan `sem_unlock` menaikkan nilai semaphore (membuka). Semaphore digunakan untuk mencegah race condition saat mengakses shared memory.

---

##### orion.c (Server)

Server bertanggung jawab mengelola seluruh game state. Saat dijalankan, server membuat shared memory, message queue, dan semaphore, lalu masuk ke event loop untuk memproses pesan dari client.

###### Setup IPC

```c
int main()
{
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // Buat shared memory
    shm_id = shmget(SHM_KEY, sizeof(GameState), IPC_CREAT | 0666);
    gs = (GameState *)shmat(shm_id, NULL, 0);

    // Init game state
    memset(gs, 0, sizeof(GameState));
    gs->server_running = 1;

    // Buat message queue
    mq_id = msgget(MQ_KEY, IPC_CREAT | 0666);

    // Buat semaphore
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    semctl(sem_id, 0, SETVAL, 1); // init ke 1 (unlocked)
```

Server membuat ketiga resource IPC dengan flag `IPC_CREAT | 0666`. Shared memory diinisialisasi dengan `memset` ke 0, dan semaphore diset ke 1 (unlocked). Signal handler `cleanup` didaftarkan untuk `SIGINT` dan `SIGTERM` agar server bisa shutdown dengan bersih.

###### Event Loop

```c
    MQMessage req;
    while (gs->server_running)
    {
        ssize_t ret = msgrcv(mq_id, &req, sizeof(MQMessage) - sizeof(long), 1, IPC_NOWAIT);

        if (ret < 0) {
            if (errno == ENOMSG) {
                usleep(50000); // 50ms
                continue;
            }
            ...
        }

        switch (req.action)
        {
            case ACT_REGISTER:       handle_register(&req);         break;
            case ACT_LOGIN:          handle_login(&req);            break;
            case ACT_BATTLE:         handle_battle(&req);           break;
            case ACT_BATTLE_ATTACK:  handle_attack(&req, 0);        break;
            case ACT_BATTLE_ULTIMATE: handle_attack(&req, 1);       break;
            case ACT_ARMORY_BUY:     handle_armory(&req);           break;
            case ACT_HISTORY:        handle_history(&req);          break;
            case ACT_PING:
                send_response(req.sender_pid, ACT_RESP_PONG, "pong");
                break;
        }
    }
```

Server menggunakan `msgrcv` dengan flag `IPC_NOWAIT` untuk non-blocking receive. Jika tidak ada pesan (`ENOMSG`), server tidur 50ms lalu cek lagi. Setiap pesan yang diterima di-dispatch ke handler yang sesuai berdasarkan `action`.

###### Handle Register

```c
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

    int idx = gs->player_count++;
    Player *p = &gs->players[idx];
    ...
    p->gold = DEFAULT_GOLD;
    p->weapon_id = -1;
    p->active = 1;

    sem_unlock(sem_id);
    send_response(req->sender_pid, ACT_RESP_OK, "Account created!");
}
```

Handler register pertama mengunci semaphore, lalu memeriksa apakah username sudah ada menggunakan `find_player()`. Jika belum ada dan server belum penuh, akun baru dibuat dengan gold awal `DEFAULT_GOLD` (150) dan tanpa senjata (`weapon_id = -1`).

###### Handle Login

```c
void handle_login(MQMessage *req)
{
    ...
    sem_lock(sem_id);
    int idx = find_player(user);

    if (idx == -1) { ... "User not found!" ... }
    if (strcmp(p->password, pass) != 0) { ... "Wrong password!" ... }
    if (p->logged_in) { ... "Already logged in elsewhere!" ... }

    p->logged_in = 1;
    p->client_pid = req->sender_pid;

    char resp[MSG_DATA_SIZE];
    snprintf(resp, sizeof(resp), "%d\n%s\n%d\n%d\n%d\n%d",
             idx, p->username, p->gold, p->lvl, p->xp, p->weapon_id);

    sem_unlock(sem_id);
    send_response(req->sender_pid, ACT_RESP_PROFILE, resp);
}
```

Login melakukan tiga pengecekan: (1) apakah username ada, (2) apakah password benar, dan (3) apakah sudah login di tempat lain. Jika berhasil, server mengirim data profil pemain ke client melalui message queue.

###### Handle Battle (Matchmaking)

```c
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
            ...
            break;
        }
    }

    if (opponent != -1) {
        // Match ditemukan! Buat arena
        int aidx = gs->arena_count++;
        BattleArena *a = &gs->arenas[aidx];
        ...
        send_response(..., ACT_RESP_BATTLE_START, resp);
    } else {
        // Masukkan ke antrian matchmaking
        gs->mm_queue[gs->mm_count++] = pidx;
        send_response(req->sender_pid, ACT_RESP_MATCHMAKING, "searching");
    }
}
```

Saat pemain memilih battle, server mencari lawan di antrian matchmaking (`mm_queue`). Jika ada lawan yang tersedia, arena baru dibuat dan kedua pemain diberitahu via `ACT_RESP_BATTLE_START`. Jika tidak ada lawan, pemain dimasukkan ke antrian dan menunggu.

###### Handle Attack

```c
void handle_attack(MQMessage *req, int is_ult)
{
    ...
    sem_lock(sem_id);
    ...
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;

    if (is_ult) {
        if (a->weapon_id[side] < 0) { sem_unlock(sem_id); return; }
        if (now - a->last_ult[side] < ULT_COOLDOWN) { sem_unlock(sem_id); return; }
        int dmg = a->damage[side] * 3;
        a->hp[target] -= dmg;
        ...
    } else {
        if (now - a->last_atk[side] < ATK_COOLDOWN) { sem_unlock(sem_id); return; }
        int dmg = a->damage[side];
        a->hp[target] -= dmg;
        ...
    }

    // Cek apakah ada yang mati
    if (a->hp[target] <= 0) {
        a->winner = side;
        a->finished = 1;
        // Update stats: XP, gold, level, history
        ...
    }
    sem_unlock(sem_id);
}
```

Handler attack menangani dua jenis serangan: normal attack dan ultimate. Ultimate membutuhkan senjata dan memiliki cooldown 3 detik, sedangkan normal attack memiliki cooldown 1 detik. Damage ultimate adalah 3x damage normal. Setelah HP lawan mencapai 0, pertempuran selesai dan stats kedua pemain diperbarui.

###### Handle Armory

```c
void handle_armory(MQMessage *req)
{
    ...
    sem_lock(sem_id);
    ...
    if (p->gold < WEAPON_LIST[widx].price) {
        send_response(..., ACT_RESP_FAIL, "Not enough gold!");
        return;
    }

    p->gold -= WEAPON_LIST[widx].price;

    // Auto-equip senjata dengan damage terbesar
    if (p->weapon_id < 0 || WEAPON_LIST[widx].bonus_dmg > WEAPON_LIST[p->weapon_id].bonus_dmg)
        p->weapon_id = widx;

    sem_unlock(sem_id);
}
```

Armory memungkinkan pemain membeli senjata. Server memeriksa apakah gold cukup, lalu mengurangi gold dan auto-equip senjata jika bonus damage-nya lebih besar dari senjata yang sedang dipakai.

###### Bot AI Thread

```c
void *bot_ai_thread(void *arg)
{
    while (gs->server_running) {
        usleep(1500000); // 1.5 detik

        sem_lock(sem_id);
        for (int i = 0; i < gs->arena_count; i++) {
            BattleArena *a = &gs->arenas[i];
            if (!a->active || a->finished) continue;

            int bot_side = -1;
            if (a->player_idx[0] == -1) bot_side = 0;
            if (a->player_idx[1] == -1) bot_side = 1;
            if (bot_side == -1) continue;

            // Bot menyerang
            int dmg = a->damage[bot_side];
            a->hp[target] -= dmg;
            ...
        }
        sem_unlock(sem_id);
    }
}
```

Thread ini berjalan terpisah dari event loop utama menggunakan `pthread`. Setiap 1.5 detik, thread ini memeriksa semua arena yang memiliki bot (`player_idx == -1`) dan melakukan serangan otomatis. Semaphore digunakan untuk mencegah race condition dengan main thread.

---

##### eternal.c (Client)

Client merupakan program yang digunakan pemain untuk berinteraksi dengan server Eterion. Client berkomunikasi dengan server menggunakan message queue, dan membaca shared memory langsung saat battle untuk rendering UI secara realtime.

###### Koneksi ke Server

```c
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
```

Saat dijalankan, client pertama membuka message queue yang sudah dibuat server. Jika gagal atau server tidak merespons ping dalam 2 detik, client menampilkan pesan error dan keluar.

###### Send dan Receive MQ

```c
void send_request(int action, const char *data) {
    MQMessage msg;
    msg.mtype = 1; // Server listens on mtype 1
    msg.action = action;
    msg.sender_pid = getpid();
    ...
    msgsnd(mq_id, &msg, sizeof(MQMessage) - sizeof(long), 0);
}

int receive_response(MQMessage *resp, int timeout) {
    if (timeout) {
        for (int i = 0; i < timeout * 10; i++) {
            if (msgrcv(mq_id, resp, ..., getpid(), IPC_NOWAIT) != -1) {
                return 1;
            }
            usleep(100000); // 100ms
        }
        return 0; // Timeout
    } else {
        return msgrcv(mq_id, resp, ..., getpid(), 0) != -1;
    }
}
```

Client mengirim pesan ke server dengan `mtype = 1` (channel server), dan menerima respons menggunakan `mtype = getpid()` sehingga hanya pesan yang ditujukan ke client tersebut yang diterima. Fungsi `receive_response` mendukung mode blocking dan polling dengan timeout.

###### Battle UI (Realtime)

```c
void draw_battle_ui(const char* data, int arena_idx) {
    int shm_id = shmget(SHM_KEY, sizeof(GameState), 0666);
    GameState *gs = (GameState *)shmat(shm_id, NULL, 0);

    BattleArena *a = &gs->arenas[arena_idx];

    // Draw Enemy HP bar
    int hp_bars = (a->hp[target] * 20) / a->max_hp[target];
    for (int i=0; i<20; i++) {
        if (i < hp_bars) printf("#"); else printf("-");
    }
    ...
    // Draw Combat Log
    for (int i=0; i<MAX_LOG; i++) {
        if (strlen(a->log[i]) > 0) printf("> %s\n", a->log[i]);
    }
    printf("\nPress 'a' to Attack | 'u' for Ultimate\n");

    shmdt(gs);
}
```

Untuk rendering battle secara realtime, client langsung membaca shared memory tanpa melalui message queue. HP bar digambar menggunakan karakter `#` dan `-`, dan combat log menampilkan 5 baris terakhir.

###### Battle Loop

```c
void do_battle() {
    ...
    enable_raw_mode();

    int in_battle = 1;
    while(in_battle) {
        draw_battle_ui("", arena_idx);

        // Non-blocking keyboard input menggunakan select()
        struct timeval tv = {0, 50000}; // 50ms
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            read(STDIN_FILENO, &c, 1);
            if (c == 'a' || c == 'A') {
                send_request(ACT_BATTLE_ATTACK, req_data);
            } else if (c == 'u' || c == 'U') {
                send_request(ACT_BATTLE_ULTIMATE, req_data);
            }
        }

        // Cek battle end dari MQ atau SHM
        ...
    }

    disable_raw_mode();
}
```

Saat battle, terminal diubah ke raw mode (`enable_raw_mode`) agar input keyboard tidak perlu menekan ENTER. `select()` digunakan untuk non-blocking input dengan timeout 50ms, sehingga UI terus di-refresh. Pemain menekan `a` untuk attack dan `u` untuk ultimate.

###### SIGINT Handler

```c
void sigint_handler(int sig) {
    (void)sig;
    if (logged_in) do_logout();
    printf("\n[System] Disconnecting from Eterion...\n");
    exit(0);
}
```

Jika pemain menekan `Ctrl+C`, handler ini memastikan pemain logout terlebih dahulu sebelum keluar, sehingga status `logged_in` di shared memory direset dengan benar.

#### Output

1. Menjalankan `orion` (Server)
![Pasted image 20260503184458.png](Assets/Pasted%20image%2020260503184458.png)

2. Menjalankan `eternal` (Client)
![Pasted image 20260503184558.png](Assets/Pasted%20image%2020260503184558.png)

#### Kendala

Tidak ada kendala.
