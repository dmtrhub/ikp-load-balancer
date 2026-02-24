#include "connection_handler.h"
#include "queue_manager.h"
#include "user_session_table.h"
#include "../Common/network_utils.h"
#include <stdio.h>
#include <stdbool.h>

// Global shutdown flag
extern volatile bool keep_running;

// Network sockets
static SOCKET listener_socket = INVALID_SOCKET;
static SOCKET distributor_socket = INVALID_SOCKET;
static SOCKET receiver_socket = INVALID_SOCKET;

// ===== LISTENER THREAD =====
// Accepts connections from users and enqueues their energy requests

DWORD WINAPI listener_thread(LPVOID lpParam) {
    listener_socket = setup_server_socket(LISTENER_PORT);

    if (listener_socket == INVALID_SOCKET) {
        printf("[LISTENER] Failed to setup socket on port %d\n", LISTENER_PORT);
        return 1;
    }

    printf("[LISTENER] Listening on port %d for user requests...\n", LISTENER_PORT);

    while (keep_running) {
        struct sockaddr_in user_addr;
        int addr_len = sizeof(user_addr);

        SOCKET user_socket = accept(listener_socket, (struct sockaddr*)&user_addr, &addr_len);

        if (user_socket != INVALID_SOCKET) {
            EnergyRequest req;

            // Receive request from user
            if (recv_all(user_socket, &req, sizeof(EnergyRequest))) {
                // Register user session and get session ID
                int session_id = user_session_register(user_socket);

                if (session_id != -1) {
                    req.socketId = session_id;

                    // Enqueue request for processing
                    request_queue_enqueue(req);
                    printf("[LISTENER] Received request from User %d (%.2f kW). Session: %d\n",
                        req.userId, req.consumedEnergy, session_id);
                }
                else {
                    printf("[LISTENER] Server full! Rejecting user connection.\n");
                    closesocket(user_socket);
                }
            }
            else {
                printf("[LISTENER] Failed to receive request from user\n");
                closesocket(user_socket);
            }
        }
    }

    closesocket(listener_socket);
    printf("[LISTENER] Shutdown complete.\n");
    return 0;
}

// ===== DISTRIBUTOR THREAD =====
// Distributes queued requests to workers on demand

DWORD WINAPI distributor_thread(LPVOID lpParam) {
    distributor_socket = setup_server_socket(REQUEST_DISTRIBUTOR_PORT);

    if (distributor_socket == INVALID_SOCKET) {
        printf("[DISTRIBUTOR] Failed to setup socket on port %d\n", REQUEST_DISTRIBUTOR_PORT);
        return 1;
    }

    printf("[DISTRIBUTOR] Listening on port %d for worker connections...\n", REQUEST_DISTRIBUTOR_PORT);

    while (keep_running) {
        // Worker connects to request a job
        SOCKET worker_socket = accept(distributor_socket, NULL, NULL);

        if (worker_socket != INVALID_SOCKET) {
            // Get next request from queue (blocks if empty)
            EnergyRequest req = request_queue_dequeue();

            // Send request to worker
            if (send_all(worker_socket, &req, sizeof(EnergyRequest))) {
                printf("[DISTRIBUTOR] Sent request (User %d) to worker\n", req.userId);
            }
            else {
                printf("[DISTRIBUTOR] Failed to send request to worker\n");
                // Re-enqueue if send failed (best effort)
                request_queue_enqueue(req);
            }

            closesocket(worker_socket);
        }
    }

    closesocket(distributor_socket);
    printf("[DISTRIBUTOR] Shutdown complete.\n");
    return 0;
}

// ===== RECEIVER THREAD =====
// Receives calculated results from workers

DWORD WINAPI receiver_thread(LPVOID lpParam) {
    receiver_socket = setup_server_socket(RESULT_RECEIVER_PORT);

    if (receiver_socket == INVALID_SOCKET) {
        printf("[RECEIVER] Failed to setup socket on port %d\n", RESULT_RECEIVER_PORT);
        return 1;
    }

    printf("[RECEIVER] Listening on port %d for worker results...\n", RESULT_RECEIVER_PORT);

    while (keep_running) {
        SOCKET worker_socket = accept(receiver_socket, NULL, NULL);

        if (worker_socket != INVALID_SOCKET) {
            PriceResult result;

            // Receive result from worker
            if (recv_all(worker_socket, &result, sizeof(PriceResult))) {
                // Enqueue result for sending to user
                response_queue_enqueue(result);
                printf("[RECEIVER] Received result from worker (User %d, Cost: %.2f RSD)\n",
                    result.userId, result.totalCost);
            }
            else {
                printf("[RECEIVER] Failed to receive result from worker\n");
            }

            closesocket(worker_socket);
        }
    }

    closesocket(receiver_socket);
    printf("[RECEIVER] Shutdown complete.\n");
    return 0;
}

// ===== SENDER THREAD =====
// Sends results back to connected users

DWORD WINAPI sender_thread(LPVOID lpParam) {
    printf("[SENDER] Starting response sender...\n");

    while (keep_running) {
        // Get result from response queue (blocks if empty)
        PriceResult result = response_queue_dequeue();

        // Check for shutdown signal
        if (result.userId == -1) {
            printf("[SENDER] Shutdown signal received. Exiting...\n");
            break;
        }

        // Retrieve user socket using session ID
        SOCKET user_socket = user_session_retrieve(result.socketId);

        if (user_socket != INVALID_SOCKET) {
            // Send result to user
            if (send_all(user_socket, &result, sizeof(PriceResult))) {
                printf("[SENDER] Sent result to User %d (Cost: %.2f RSD)\n",
                    result.userId, result.totalCost);
            }
            else {
                printf("[SENDER] Failed to send result to User %d\n", result.userId);
            }

            closesocket(user_socket);
        }
        else {
            printf("[SENDER] WARNING: User socket not found for session %d\n", result.socketId);
        }
    }

    printf("[SENDER] Shutdown complete.\n");
    return 0;
}
