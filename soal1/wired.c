// Let's All Love Lain
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "protocol.h"
/*
XoXoXoXoXoXoXoXo
Global Variables
XoXoXoXoXoXoXoXo
*/
int server_fd;
time_t server_uptime;
/*
XoXoXoXoXoXoXoXo
Polling
XoXoXoXoXoXoXoXo
*/
struct pollfd fds[MAX_CLIENTS + 1];  // +1 untuk server_fd sendiri (fds[0])
int nfds = 1;                        // jumlah fd yang aktif
// Creating userInfo
typedef struct
{
    int fd;          // file descriptor client
    char name[67];   // nama client
    int isAdmin;     // apakah sudah login sebagai admin?
    int loggedIn;    // apakah sudah kirim nama dan diterima?
} userInfo;

userInfo users[MAX_CLIENTS + 1]; // +1 karena index 0 tidak dipakai (fds[0] = server)
int user_count = 0;

// Forward declarations
void handle_newConnection();
void handle_clientMessage(int poll_index);
void handle_login(int poll_index, Message *msg);
void handle_chat(int poll_index, Message *msg);
void handle_admin_auth(int poll_index, Message *msg);
void handle_rpc(int poll_index, Message *msg);
void handle_disconnect(int poll_index);
void sigint_handler(int sig);
/*
XoXoXoXoXoXoXoXo
Nulis ke history.log
Format: [YYYY-MM-DD HH:MM:SS] [Category] [Detail]
XoXoXoXoXoXoXoXo
*/
void writeLog(const char *category, const char *detail)
{
    FILE *f = fopen("history.log", "a");
    if (f == NULL) return;

    time_t now = time(NULL);
    struct tm *tp = localtime(&now);

    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] [%s]\n",
            tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday,
            tp->tm_hour, tp->tm_min, tp->tm_sec,
            category, detail);
    fclose(f);
}
/*
XoXoXoXoXoXoXoXo
Helper: kirim Message struct ke fd
XoXoXoXoXoXoXoXo
*/
void send_msg(int fd, int type, const char *sender, const char *content)
{
    Message msg;
    msg.type = type;
    strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
    msg.sender[sizeof(msg.sender) - 1] = '\0';
    strncpy(msg.content, content, sizeof(msg.content) - 1);
    msg.content[sizeof(msg.content) - 1] = '\0';
    send(fd, &msg, sizeof(msg), 0);
}
/*
XoXoXoXoXoXoXoXo
Helper: cari index berdasarkan nama
XoXoXoXoXoXoXoXo
*/
int find_by_name(const char *name)
{
    for (int i = 1; i < nfds; i++)
        if (users[i].loggedIn && strcmp(users[i].name, name) == 0) return i;
    return -1;
}
/*
XoXoXoXoXoXoXoXo
Handle New Connection
XoXoXoXoXoXoXoXo
*/
void handle_newConnection()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock < 0)
    {
        fprintf(stderr, "> Accept failed from %s: %s\n",
                inet_ntoa(client_addr.sin_addr), strerror(errno));
        return;
    }

    if (nfds >= MAX_CLIENTS + 1)
    {
        send_msg(client_sock, MSG_SYSTEM, ADMIN_NAME, "Server penuh!");
        close(client_sock);
        return;
    }

    // tambah ke array poll
    fds[nfds].fd = client_sock;
    fds[nfds].events = POLLIN;
    users[nfds].fd = client_sock;
    users[nfds].name[0] = '\0';
    users[nfds].isAdmin = 0;
    users[nfds].loggedIn = 0;
    nfds++;

    printf("> Koneksi baru dari %s (fd: %d), menunggu nama...\n",
           inet_ntoa(client_addr.sin_addr), client_sock);
}
/*
XoXoXoXoXoXoXoXo
Handle Disconnect
XoXoXoXoXoXoXoXo
*/
void handle_disconnect(int poll_index)
{
    int client_fd = fds[poll_index].fd;

    if (users[poll_index].loggedIn)
    {
        char log_detail[100];
        snprintf(log_detail, sizeof(log_detail),
                 "User '%s' disconnected", users[poll_index].name);
        writeLog("System", log_detail);
        printf("> %s disconnected\n", users[poll_index].name);
    }

    close(client_fd);

    // Geser fds[] dan users[] ke kiri
    for (int i = poll_index; i < nfds - 1; i++)
    {
        fds[i] = fds[i + 1];
        users[i] = users[i + 1];
    }
    nfds--;
}
/*
XoXoXoXoXoXoXoXo
Handle Login
XoXoXoXoXoXoXoXo
*/
void handle_login(int poll_index, Message *msg)
{
    int client_fd = fds[poll_index].fd;

    // Cek nama duplikat
    if (find_by_name(msg->sender) != -1)
    {
        char reply[MAX_BUFFER];
        snprintf(reply, sizeof(reply),
                 "The identity '%s' is already synchronized in The Wired.", msg->sender);
        send_msg(client_fd, MSG_LOGIN_FAIL, "System", reply);

        char log_detail[150];
        snprintf(log_detail, sizeof(log_detail),
                 "Login failed: '%s' already exists", msg->sender);
        writeLog("System", log_detail);
        return;
    }

    // Terima login
    strncpy(users[poll_index].name, msg->sender, sizeof(users[poll_index].name) - 1);
    users[poll_index].name[sizeof(users[poll_index].name) - 1] = '\0';
    users[poll_index].loggedIn = 1;

    // Kirim welcome
    char welcome[MAX_BUFFER];
    snprintf(welcome, sizeof(welcome), "--- Welcome to The Wired, %s ---", msg->sender);
    send_msg(client_fd, MSG_LOGIN_OK, "System", welcome);

    // Log
    char log_detail[100];
    snprintf(log_detail, sizeof(log_detail), "User '%s' connected", msg->sender);
    writeLog("System", log_detail);
    printf("> User '%s' connected\n", msg->sender);
}
/*
XoXoXoXoXoXoXoXo
Handle Chat (Broadcast)
XoXoXoXoXoXoXoXo
*/
void handle_chat(int poll_index, Message *msg)
{
    // Broadcast ke semua client yang sudah login, kecuali pengirim
    for (int i = 1; i < nfds; i++)
    {
        if (i != poll_index && users[i].loggedIn)
            send_msg(fds[i].fd, MSG_CHAT, msg->sender, msg->content);
    }

    // Log format: [[alice]: hello lain]
    char log_detail[MAX_BUFFER + 70];
    snprintf(log_detail, sizeof(log_detail), "[%s]: %s", msg->sender, msg->content);
    writeLog("User", log_detail);
}
/*
XoXoXoXoXoXoXoXo
Handle Admin Auth
XoXoXoXoXoXoXoXo
*/
void handle_admin_auth(int poll_index, Message *msg)
{
    int client_fd = fds[poll_index].fd;

    // Harus nama "The Knights"
    if (strcmp(users[poll_index].name, ADMIN_NAME) != 0)
    {
        send_msg(client_fd, MSG_ADMIN_FAIL, "System",
                 "Access denied. You are not The Knights.");
        return;
    }

    // Cek password
    if (strcmp(msg->content, ADMIN_PASS) != 0)
    {
        send_msg(client_fd, MSG_ADMIN_FAIL, "System",
                 "Authentication failed. Wrong password.");
        return;
    }

    users[poll_index].isAdmin = 1;
    send_msg(client_fd, MSG_ADMIN_OK, "System",
             "Authentication Successful. Granted Admin privileges.");
}
/*
XoXoXoXoXoXoXoXo
Handle RPC
XoXoXoXoXoXoXoXo
*/
void handle_rpc(int poll_index, Message *msg)
{
    int client_fd = fds[poll_index].fd;

    if (!users[poll_index].isAdmin)
    {
        send_msg(client_fd, MSG_RPC_RESULT, "System", "Access denied. Admin only.");
        return;
    }

    switch (msg->type)
    {
        case MSG_RPC_USERS:
        {
            // Hitung user aktif — admin tidak dihitung
            int active = 0;
            for (int i = 1; i < nfds; i++)
                if (users[i].loggedIn && !users[i].isAdmin) active++;

            char result[MAX_BUFFER];
            snprintf(result, sizeof(result), "Active entities: %d", active);
            send_msg(client_fd, MSG_RPC_RESULT, "System", result);
            writeLog("Admin", "RPC_GET_USERS");
            break;
        }
        case MSG_RPC_UPTIME:
        {
            time_t now = time(NULL);
            int uptime = (int)(now - server_uptime);
            int h = uptime / 3600, m = (uptime % 3600) / 60, s = uptime % 60;

            char result[MAX_BUFFER];
            snprintf(result, sizeof(result), "Server uptime: %02d:%02d:%02d", h, m, s);
            send_msg(client_fd, MSG_RPC_RESULT, "System", result);
            writeLog("Admin", "RPC_GET_UPTIME");
            break;
        }
        case MSG_RPC_SHUTDOWN:
        {
            writeLog("Admin", "RPC_SHUTDOWN");
            writeLog("System", "EMERGENCY SHUTDOWN INITIATED");

            // Kirim MSG_DISCONNECT ke semua client lalu tutup
            for (int i = 1; i < nfds; i++)
            {
                send_msg(fds[i].fd, MSG_DISCONNECT, "System",
                         "EMERGENCY SHUTDOWN INITIATED");
                close(fds[i].fd);
            }
            close(server_fd);
            exit(0);
        }
    }
}
/*
XoXoXoXoXoXoXoXo
Message Handler
XoXoXoXoXoXoXoXo
*/
void handle_clientMessage(int poll_index)
{
    int client_sock = fds[poll_index].fd;
    Message msg;

    int bytes = recv(client_sock, &msg, sizeof(Message), 0);

    if (bytes <= 0)
    {
        handle_disconnect(poll_index);
        return;
    }

    switch (msg.type)
    {
        case MSG_LOGIN:
            handle_login(poll_index, &msg);
            break;
        case MSG_CHAT:
            handle_chat(poll_index, &msg);
            break;
        case MSG_EXIT:
            handle_disconnect(poll_index);
            break;
        case MSG_ADMIN_AUTH:
            handle_admin_auth(poll_index, &msg);
            break;
        case MSG_RPC_USERS:
        case MSG_RPC_UPTIME:
        case MSG_RPC_SHUTDOWN:
            handle_rpc(poll_index, &msg);
            break;
        default:
            break;
    }
}
/*
XoXoXoXoXoXoXoXo
SIGINT Handler (Ctrl+C di server)
XoXoXoXoXoXoXoXo
*/
void sigint_handler(int sig)
{
    (void)sig;
    for (int i = 1; i < nfds; i++)
    {
        send_msg(fds[i].fd, MSG_DISCONNECT, "System", "Server shutting down.");
        close(fds[i].fd);
    }
    close(server_fd);
    writeLog("System", "SERVER OFFLINE");
    exit(0);
}

int main()
{
    /*
    _o_o_o_o_o_o_o
    Server Setup
    _o_o_o_o_o_o_o
    */
    server_uptime = time(NULL);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    /*
    AF_INET    = pakai IPv4
    SOCK_STREAM = pakai TCP (reliable, berurutan)
    0          = protocol default
    TCP = protokol nomor 6
    UDP = protokol nomor 17
    ICMP = protokol nomor 1
    */
    int option = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    struct sockaddr_in address;
    address.sin_family = AF_INET;         // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // terima koneksi dari IP manapun
    address.sin_port = htons(PORT);       // port dari protocol.h (8080)
    //bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    //listen
    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);
    writeLog("System", "SERVER ONLINE");
    printf("> The Wired is now ONLINE on port %d\n", PORT);

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    while(1)
    {
        int server_activity = poll(fds, nfds, -1);
        if (server_activity < 0) break;
        if (server_activity == 0) continue;

        // Cek setiap fd: apakah ada event?
        for (int i = 0; i < nfds; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                if (fds[i].fd == server_fd)
                {
                    handle_newConnection();
                } else
                {
                    handle_clientMessage(i);
                }
            }
        }
    }
}
