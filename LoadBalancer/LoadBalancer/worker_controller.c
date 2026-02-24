#include "worker_controller.h"
#include "queue_manager.h"
#include "../Common/network_utils.h"
#include <stdio.h>
#include <stdbool.h>

// Worker process tracking
static HANDLE active_workers[MAX_WORKER_PROCESSES];
static int active_worker_count = 0;
static CRITICAL_SECTION worker_lock;

// Network sockets
static SOCKET price_socket = INVALID_SOCKET;
static SOCKET distributor_socket = INVALID_SOCKET;

// Global shutdown flag
extern volatile bool keep_running;

// ===== WORKER CONTROLLER IMPLEMENTATION =====

void worker_controller_init() {
    InitializeCriticalSection(&worker_lock);
    active_worker_count = 0;
    memset(active_workers, 0, sizeof(active_workers));
    printf("[WORKER_CONTROLLER] Initialized. Max workers: %d\n", MAX_WORKER_PROCESSES);
}

void worker_controller_destroy() {
    DeleteCriticalSection(&worker_lock);
}

void spawn_worker() {
    EnterCriticalSection(&worker_lock);

    // Check if we can spawn more workers
    if (active_worker_count >= MAX_WORKER_PROCESSES) {
        printf("[WORKER_CONTROLLER] Max workers reached (%d). Cannot spawn more.\n", MAX_WORKER_PROCESSES);
        LeaveCriticalSection(&worker_lock);
        return;
    }

    LeaveCriticalSection(&worker_lock);

    // Prepare process startup info
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create worker process
    if (!CreateProcessA(
        NULL,
        (char*)WORKER_EXECUTABLE,
        NULL,           // Process attributes
        NULL,           // Thread attributes
        FALSE,          // Inherit handles
        CREATE_NO_WINDOW, // Creation flags
        NULL,           // Environment
        NULL,           // Working directory
        &si,
        &pi)) {

        printf("[WORKER_CONTROLLER] Failed to spawn worker. Error: %lu\n", GetLastError());
        return;
    }

    // Store process handle
    EnterCriticalSection(&worker_lock);
    active_workers[active_worker_count++] = pi.hProcess;
    int worker_id = active_worker_count;
    LeaveCriticalSection(&worker_lock);

    // Close thread handle (we only need process handle)
    CloseHandle(pi.hThread);

    printf("[WORKER_CONTROLLER] Spawned worker #%d. Active: %d/%d\n",
        worker_id, active_worker_count, MAX_WORKER_PROCESSES);
}

void cleanup_dead_workers() {
    EnterCriticalSection(&worker_lock);

    // Check each active worker
    for (int i = 0; i < active_worker_count; i++) {
        DWORD exit_code;

        if (GetExitCodeProcess(active_workers[i], &exit_code)) {
            // Worker has terminated
            if (exit_code != STILL_ACTIVE) {
                printf("[WORKER_CONTROLLER] Cleaning up terminated worker #%d\n", i + 1);

                CloseHandle(active_workers[i]);

                // Shift remaining workers
                for (int j = i; j < active_worker_count - 1; j++) {
                    active_workers[j] = active_workers[j + 1];
                }

                active_worker_count--;
                i--; // Re-check this index
            }
        }
    }

    LeaveCriticalSection(&worker_lock);
}

int get_active_worker_count() {
    int count = 0;
    EnterCriticalSection(&worker_lock);
    count = active_worker_count;
    LeaveCriticalSection(&worker_lock);
    return count;
}

void terminate_all_workers() {
    printf("[WORKER_CONTROLLER] Initiating graceful worker shutdown...\n");

    int count = get_active_worker_count();

    // Send poison pills to workers
    for (int i = 0; i < count; i++) {
        EnergyRequest poison_pill = { 0 };
        poison_pill.userId = -1;  // Signal for termination
        request_queue_enqueue(poison_pill);
    }

    Sleep(500);  // Give workers time to exit gracefully

    // Force terminate any remaining workers
    EnterCriticalSection(&worker_lock);
    for (int i = 0; i < active_worker_count; i++) {
        DWORD exit_code;
        GetExitCodeProcess(active_workers[i], &exit_code);

        if (exit_code == STILL_ACTIVE) {
            printf("[WORKER_CONTROLLER] Force terminating worker #%d\n", i + 1);
            TerminateProcess(active_workers[i], 0);
        }

        CloseHandle(active_workers[i]);
        active_workers[i] = NULL;
    }

    active_worker_count = 0;
    LeaveCriticalSection(&worker_lock);

    printf("[WORKER_CONTROLLER] All workers terminated.\n");
}

// ===== MONITORING THREAD =====

DWORD WINAPI monitor_thread(LPVOID lpParam) {
    printf("[MONITOR] Starting queue occupancy monitoring...\n");

    while (keep_running) {
        Sleep(MONITOR_CHECK_INTERVAL);

        float occupancy = request_queue_occupancy();

        cleanup_dead_workers();

        EnterCriticalSection(&worker_lock);

        // Scale UP when queue gets too full
        if (occupancy > SCALE_UP_THRESHOLD) {
            if (active_worker_count < MAX_WORKER_PROCESSES) {
                printf("[MONITOR] Queue at %.1f%%. Spawning new worker...\n", occupancy);
                LeaveCriticalSection(&worker_lock);
                spawn_worker();
                EnterCriticalSection(&worker_lock);
            }
            else {
                printf("[MONITOR] Queue critical (%.1f%%), but max workers reached!\n", occupancy);
            }
        }

        // Scale DOWN when queue gets too empty
        else if (occupancy < SCALE_DOWN_THRESHOLD && active_worker_count > 1) {
            printf("[MONITOR] Queue at %.1f%%. Terminating one worker...\n", occupancy);

            EnergyRequest poison_pill = { 0 };
            poison_pill.userId = -1;
            LeaveCriticalSection(&worker_lock);
            request_queue_enqueue(poison_pill);
            EnterCriticalSection(&worker_lock);
        }

        else if (occupancy > 0 && occupancy <= SCALE_UP_THRESHOLD &&
            occupancy >= SCALE_DOWN_THRESHOLD) {
            printf("[MONITOR] Queue at %.1f%% (stable)\n", occupancy);
        }

        LeaveCriticalSection(&worker_lock);
    }

    printf("[MONITOR] Shutdown complete.\n");
    return 0;
}

// ===== PRICE DISTRIBUTOR THREAD =====

DWORD WINAPI price_distributor_thread(LPVOID lpParam) {
    PricingConfiguration* config = (PricingConfiguration*)lpParam;

    SOCKET listen_socket = setup_server_socket(PRICE_DISTRIBUTION_PORT);
    price_socket = listen_socket;

    if (listen_socket == INVALID_SOCKET) {
        printf("[PRICE_DISTRIBUTOR] Failed to listen on port %d\n", PRICE_DISTRIBUTION_PORT);
        return 1;
    }

    printf("[PRICE_DISTRIBUTOR] Listening on port %d for worker registrations...\n",
        PRICE_DISTRIBUTION_PORT);

    while (keep_running) {
        SOCKET worker_socket = accept(listen_socket, NULL, NULL);

        if (worker_socket != INVALID_SOCKET) {
            // Send pricing configuration to worker
            if (send_all(worker_socket, config, sizeof(PricingConfiguration))) {
                printf("[PRICE_DISTRIBUTOR] Sent pricing config to worker\n");
            }
            closesocket(worker_socket);
        }
    }

    closesocket(listen_socket);
    printf("[PRICE_DISTRIBUTOR] Shutdown complete.\n");
    return 0;
}
