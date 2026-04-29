// Let's All Love Lain
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include "protocol.h"
/*
XoXoXoXoXoXoXoXo
Global Variables
XoXoXoXoXoXoXoXo
*/
int sock_fd;
int is_admin = 0;
int logged_in = 0;
char my_name[67];
/*
XoXoXoXoXoXoXoXo
Helper: kirim Message struct ke server
XoXoXoXoXoXoXoXo
*/
void send_msg(int type, const char *sender, const char *content)
{
    Message msg;
    msg.type = type;
    strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
    msg.sender[sizeof(msg.sender) - 1] = '\0';
    strncpy(msg.content, content, sizeof(msg.content) - 1);
    msg.content[sizeof(msg.content) - 1] = '\0';
    send(sock_fd, &msg, sizeof(msg), 0);
}
/*
XoXoXoXoXoXoXoXo
SIGINT Handler (Ctrl+C di client)
XoXoXoXoXoXoXoXo
*/
void sigint_handler(int sig)
{
    (void)sig;
    if (logged_in)
        send_msg(MSG_EXIT, my_name, "");
    printf("\n[System] Disconnecting from The Wired...\n");
    close(sock_fd);
    exit(0);
}
/*
XoXoXoXoXoXoXoXo
Tampilkan Admin Console
XoXoXoXoXoXoXoXo
*/
void show_admin_console()
{
    printf("\n=== THE KNIGHTS CONSOLE ===\n");
    printf("1. Check Active Entites (Users)\n");
    printf("2. Check Server Uptime\n");
    printf("3. Execute Emergency Shutdown\n");
    printf("4. Disconnect\n");
    printf("Command >> ");
    fflush(stdout);
}
/*
XoXoXoXoXoXoXoXo
Handle input dari stdin
XoXoXoXoXoXoXoXo
*/
void handle_stdin()
{
    char buf[MAX_BUFFER];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
    {
        // EOF
        send_msg(MSG_EXIT, my_name, "");
        printf("[System] Disconnecting from The Wired...\n");
        close(sock_fd);
        exit(0);
    }

    // Hapus newline
    buf[strcspn(buf, "\n")] = '\0';

    if (is_admin)
    {
        // Admin mode: baca nomor menu
        int choice = atoi(buf);
        switch (choice)
        {
            case 1:
                send_msg(MSG_RPC_USERS, my_name, "");
                break;
            case 2:
                send_msg(MSG_RPC_UPTIME, my_name, "");
                break;
            case 3:
                send_msg(MSG_RPC_SHUTDOWN, my_name, "");
                break;
            case 4:
                send_msg(MSG_EXIT, my_name, "");
                printf("[System] Disconnecting from The Wired...\n");
                close(sock_fd);
                exit(0);
            default:
                printf("Command >> ");
                fflush(stdout);
                return;
        }
    }
    else
    {
        // Normal user mode
        if (strlen(buf) == 0) 
        {
            printf("> ");
            fflush(stdout);
            return;
        }

        if (strcmp(buf, "/exit") == 0)
        {
            send_msg(MSG_EXIT, my_name, "");
            printf("[System] Disconnecting from The Wired...\n");
            close(sock_fd);
            exit(0);
        }

        send_msg(MSG_CHAT, my_name, buf);
        printf("> ");
        fflush(stdout);
    }
}
/*
XoXoXoXoXoXoXoXo
Handle pesan dari server
XoXoXoXoXoXoXoXo
*/
void handle_server()
{
    Message msg;
    int bytes = recv(sock_fd, &msg, sizeof(Message), 0);

    if (bytes <= 0)
    {
        printf("[System] Disconnecting from The Wired...\n");
        close(sock_fd);
        exit(0);
    }

    switch (msg.type)
    {
        case MSG_LOGIN_OK:
            logged_in = 1;
            printf("%s\n", msg.content);

            // Kalau nama adalah The Knights, minta password
            if (strcmp(my_name, ADMIN_NAME) == 0)
            {
                printf("Enter Password: ");
                fflush(stdout);
                char pass[MAX_BUFFER];
                if (fgets(pass, sizeof(pass), stdin) != NULL)
                {
                    pass[strcspn(pass, "\n")] = '\0';
                    send_msg(MSG_ADMIN_AUTH, my_name, pass);
                }
            }
            else
            {
                printf("> ");
                fflush(stdout);
            }
            break;

        case MSG_LOGIN_FAIL:
            printf("[System] %s\n", msg.content);
            // Minta nama lagi
            printf("Enter your name: ");
            fflush(stdout);
            if (fgets(my_name, sizeof(my_name), stdin) != NULL)
            {
                my_name[strcspn(my_name, "\n")] = '\0';
                send_msg(MSG_LOGIN, my_name, "");
            }
            break;

        case MSG_CHAT:
            printf("[%s]: %s\n", msg.sender, msg.content);
            if (is_admin)
            {
                printf("Command >> ");
            }
            else
            {
                printf("> ");
            }
            fflush(stdout);
            break;

        case MSG_ADMIN_OK:
            is_admin = 1;
            printf("[System] %s\n", msg.content);
            show_admin_console();
            break;

        case MSG_ADMIN_FAIL:
            printf("[System] %s\n", msg.content);
            printf("> ");
            fflush(stdout);
            break;

        case MSG_RPC_RESULT:
            printf("[System] %s\n", msg.content);
            show_admin_console();
            break;

        case MSG_SYSTEM:
            printf("[System] %s\n", msg.content);
            fflush(stdout);
            break;

        case MSG_DISCONNECT:
            printf("[System] Disconnecting from The Wired...\n");
            close(sock_fd);
            exit(0);

        default:
            break;
    }
}

int main()
{
    /*
    _o_o_o_o_o_o_o
    Connect ke Server
    _o_o_o_o_o_o_o
    */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    // Minta nama
    printf("Enter your name: ");
    fflush(stdout);
    fgets(my_name, sizeof(my_name), stdin);
    my_name[strcspn(my_name, "\n")] = '\0';

    // Kirim MSG_LOGIN
    send_msg(MSG_LOGIN, my_name, "");

    /*
    _o_o_o_o_o_o_o
    Poll Loop — async I/O
    Pantau stdin DAN socket sekaligus (tanpa fork)
    _o_o_o_o_o_o_o
    */
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; // input dari user
    fds[0].events = POLLIN;
    fds[1].fd = sock_fd;      // pesan dari server
    fds[1].events = POLLIN;

    while (1)
    {
        int activity = poll(fds, 2, -1);
        if (activity < 0) break;

        if (fds[0].revents & POLLIN)
            handle_stdin();

        if (fds[1].revents & POLLIN)
            handle_server();
    }

    close(sock_fd);
    return 0;
}
