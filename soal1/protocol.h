#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 8080 
#define MAX_BUFFER 6767
#define MAX_CLIENTS 67
#define ADMIN_NAME "The Knights"
#define ADMIN_PASS "protocol7"

// Type of message — enum so it is clear and not using magic number
enum msg_type {
    MSG_LOGIN,          // client send name
    MSG_LOGIN_OK,       // server: name accepted
    MSG_LOGIN_FAIL,     // server: name already exists
    MSG_CHAT,           // normal chat message
    MSG_EXIT,           // client wants to exit
    MSG_SYSTEM,         // system message from server
    MSG_ADMIN_AUTH,     // admin send password
    MSG_ADMIN_OK,       // admin authentication success
    MSG_ADMIN_FAIL,     // admin authentication failed
    MSG_RPC_USERS,      // admin ask for user count
    MSG_RPC_UPTIME,     // admin ask for uptime
    MSG_RPC_SHUTDOWN,   // admin ask for shutdown
    MSG_RPC_RESULT,     // server send RPC result
    MSG_DISCONNECT      // server disconnect    
};

// packet that is going to be sent
typedef struct {
    int type;           // enum msg_type di atas
    char sender[67];    // nama pengirim
    char content[MAX_BUFFER]; // isi pesan
} Message;

#endif