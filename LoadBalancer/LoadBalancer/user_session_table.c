#include "user_session_table.h"
#include <string.h>

// Session storage
static SOCKET user_sessions[MAX_CONCURRENT_USERS];
static CRITICAL_SECTION session_lock;
static int active_sessions = 0;

void user_session_init() {
    InitializeCriticalSection(&session_lock);

    for (int i = 0; i < MAX_CONCURRENT_USERS; i++) {
        user_sessions[i] = INVALID_SOCKET;
    }

    active_sessions = 0;
    printf("[USER_SESSION] Initialized. Max capacity: %d\n", MAX_CONCURRENT_USERS);
}

void user_session_destroy() {
    EnterCriticalSection(&session_lock);

    // Close all open sockets
    for (int i = 0; i < MAX_CONCURRENT_USERS; i++) {
        if (user_sessions[i] != INVALID_SOCKET) {
            closesocket(user_sessions[i]);
            user_sessions[i] = INVALID_SOCKET;
        }
    }

    LeaveCriticalSection(&session_lock);
    DeleteCriticalSection(&session_lock);

    printf("[USER_SESSION] Destroyed. Closed %d sessions.\n", active_sessions);
}

int user_session_register(SOCKET socket) {
    int session_id = -1;

    EnterCriticalSection(&session_lock);

    // Find first available slot
    for (int i = 0; i < MAX_CONCURRENT_USERS; i++) {
        if (user_sessions[i] == INVALID_SOCKET) {
            user_sessions[i] = socket;
            session_id = i;
            active_sessions++;
            break;
        }
    }

    if (session_id == -1) {
        printf("[USER_SESSION] WARNING: No available slots! Max capacity reached.\n");
    }
    else {
        printf("[USER_SESSION] Registered user #%d. Active sessions: %d\n",
            session_id, active_sessions);
    }

    LeaveCriticalSection(&session_lock);

    return session_id;
}

SOCKET user_session_retrieve(int session_id) {
    SOCKET socket = INVALID_SOCKET;

    EnterCriticalSection(&session_lock);

    if (session_id >= 0 && session_id < MAX_CONCURRENT_USERS) {
        socket = user_sessions[session_id];
        user_sessions[session_id] = INVALID_SOCKET;

        if (socket != INVALID_SOCKET) {
            active_sessions--;
            printf("[USER_SESSION] Retrieved socket for user #%d. Active: %d\n",
                session_id, active_sessions);
        }
    }

    LeaveCriticalSection(&session_lock);

    return socket;
}

int user_session_count() {
    int count = 0;
    EnterCriticalSection(&session_lock);
    count = active_sessions;
    LeaveCriticalSection(&session_lock);
    return count;
}

// Cleanup invalid/closed sockets to free up session slots
void user_session_cleanup() {
    EnterCriticalSection(&session_lock);

    int cleaned = 0;
    for (int i = 0; i < MAX_CONCURRENT_USERS; i++) {
        if (user_sessions[i] != INVALID_SOCKET) {
            // For safety: just set all open sockets as inactive if not being used
            // Better approach: track timeout or explicit cleanup after send
            // For now: verify socket is not in error state
            int opt_val;
            int opt_len = sizeof(opt_val);
            if (getsockopt(user_sessions[i], SOL_SOCKET, SO_ERROR, (char*)&opt_val, &opt_len) == SOCKET_ERROR
                || opt_val != 0) {
                // Socket has error
                closesocket(user_sessions[i]);
                user_sessions[i] = INVALID_SOCKET;
                active_sessions--;
                cleaned++;
            }
        }
    }

    if (cleaned > 0) {
        printf("[USER_SESSION] Cleanup: Freed %d dead session slots. Active: %d\n", 
            cleaned, active_sessions);
    }

    LeaveCriticalSection(&session_lock);
}
