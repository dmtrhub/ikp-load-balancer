#include "connection_handler.h"
#include "queue_manager.h"
#include "user_session_table.h"
#include "../Common/network_utils.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

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

    unsigned long long accepted_count = 0;
    unsigned long long rejected_count = 0;
    unsigned long long no_reply_count = 0;

    while (keep_running) {
        struct sockaddr_in user_addr;
        int addr_len = sizeof(user_addr);

        SOCKET user_socket = accept(listener_socket, (struct sockaddr*)&user_addr, &addr_len);

        if (user_socket != INVALID_SOCKET) {

            // Backpressure: reject if queue >= 80% full (don't wait until 100%)
            // This prevents listener from flooding the queue and helps system stay balanced
            int current_queue = request_queue_count();
            if (current_queue >= (int)(MAX_QUEUE_SIZE * 0.80f)) {
                closesocket(user_socket);
                rejected_count++;
                continue;
            }

            EnergyRequest req;

            // Receive request from user
            if (recv_all(user_socket, &req, sizeof(EnergyRequest))) {

                // Fire-and-forget mode for large tests (client explicitly sets socketId = -1)
                if (req.socketId == -1) {
                    request_queue_enqueue(req);
                    req.socketId = -1;
                    closesocket(user_socket);
                    accepted_count++;
                    no_reply_count++;
                    continue;
                }

                int session_id = user_session_register(user_socket);
                if (session_id == -1) {
                    req.socketId = -1;
                    request_queue_enqueue(req);
                    closesocket(user_socket);
                    no_reply_count++;
                }
                else {
                    req.socketId = session_id;
                    request_queue_enqueue(req);
                }
                accepted_count++;
            }
            else {
                closesocket(user_socket);
            }
        }
    }

    closesocket(listener_socket);
    printf("[LISTENER] Stats: accepted=%llu rejected=%llu no_reply=%llu\n", accepted_count, rejected_count, no_reply_count);
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
            if (!send_all(worker_socket, &req, sizeof(EnergyRequest))) {
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

// ===== RECEIVER HANDLER THREAD =====
// Handles a single worker result asynchronously

DWORD WINAPI receiver_handler_thread(LPVOID lpParam) {
    SOCKET worker_socket = (SOCKET)(intptr_t)lpParam;
    PriceResult result;

    // Receive result from worker
    if (recv_all(worker_socket, &result, sizeof(PriceResult))) {
        // Enqueue result for sending to user
        if (result.userId != -1) {  // Skip poison pills
            response_queue_enqueue(result);
        }
    } else {
        //printf("[RECEIVER] Handler: Failed to receive result\n");
    }

    closesocket(worker_socket);
    return 0;
}

// ===== RECEIVER THREAD =====
// Receives calculated results from workers (spawns handler thread per connection)

static long long handler_threads_created = 0;
static long long handler_fallback_count = 0;

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
            // Spawn async handler thread for this worker result
            HANDLE handler_thread = CreateThread(
                NULL,
                0,
                receiver_handler_thread,
                (LPVOID)(intptr_t)worker_socket,
                0,
                NULL);

            if (handler_thread != NULL) {
                // Don't wait - immediately loop back to accept next connection
                // Thread will close socket and clean up
                handler_threads_created++;
                CloseHandle(handler_thread);
            } else {
                // If thread creation fails, handle directly as fallback
                handler_fallback_count++;
                PriceResult result;
                if (recv_all(worker_socket, &result, sizeof(PriceResult))) {
                    if (result.userId != -1) {
                        response_queue_enqueue(result);
                    }
                }
                closesocket(worker_socket);
            }
        }
    }

    closesocket(receiver_socket);
    printf("[RECEIVER] Shutdown complete. Threads created: %lld, Fallbacks: %lld\n", 
        handler_threads_created, handler_fallback_count);
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

        // Skip fire-and-forget results (socketId == -1)
        if (result.socketId == -1) {
            // No socket to reply to (heavy load test mode)
            continue;
        }

        // Retrieve user socket using session ID
        SOCKET user_socket = user_session_retrieve(result.socketId);

        if (user_socket != INVALID_SOCKET) {
            // Send result to user
            send_all(user_socket, &result, sizeof(PriceResult));

            // CRITICAL: Close socket to free up session slot immediately
            closesocket(user_socket);
        }
        else {
            // Session already closed or fire-and-forget request
        }
    }

    printf("[SENDER] Shutdown complete.\n");
    return 0;
}
