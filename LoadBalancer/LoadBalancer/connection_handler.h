#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include "../Common/common_types.h"
#include "config.h"

// ===== USER REQUEST LISTENER =====
// Listens on LISTENER_PORT (5059) for incoming user requests
DWORD WINAPI listener_thread(LPVOID lpParam);

// ===== REQUEST DISTRIBUTOR =====
// Distributes requests from queue to workers on DISTRIBUTOR_PORT (5062)
DWORD WINAPI distributor_thread(LPVOID lpParam);

// ===== RESULT RECEIVER =====
// Receives results from workers on RECEIVER_PORT (5061)
DWORD WINAPI receiver_thread(LPVOID lpParam);

// ===== RESPONSE SENDER =====
// Sends results back to users
DWORD WINAPI sender_thread(LPVOID lpParam);

#endif
