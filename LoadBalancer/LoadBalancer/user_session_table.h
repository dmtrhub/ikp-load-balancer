#ifndef USER_SESSION_TABLE_H
#define USER_SESSION_TABLE_H

#include "../Common/common_types.h"
#include "config.h"

// Tracks connected users and their associated sockets
void user_session_init();
void user_session_destroy();

// Register new user session
int user_session_register(SOCKET socket);

// Find and remove user session
SOCKET user_session_retrieve(int session_id);

int user_session_count();

// Cleanup dead/closed sockets to free session slots
void user_session_cleanup();

#endif
