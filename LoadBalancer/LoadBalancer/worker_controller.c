#include "worker_controller.h"
#include "queue_manager.h"
#include "user_session_table.h"
#include "../Common/network_utils.h"
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    printf("[WORKER_CONTROLLER] Spawning worker: %s\n", WORKER_EXECUTABLE);

    // Create worker process
    if (!CreateProcessA(
        NULL,
        (char*)WORKER_EXECUTABLE,
        NULL,           // Process attributes
        NULL,           // Thread attributes
        FALSE,          // Inherit handles
        0,              // Creation flags - show window
        NULL,           // Environment
        NULL,           // Working directory
        &si,
        &pi)) {

        DWORD error = GetLastError();
        printf("[WORKER_CONTROLLER] Failed to spawn worker. Error: %lu (0x%lX)\n", error, error);
        
        if (error == ERROR_FILE_NOT_FOUND) {
            printf("[WORKER_CONTROLLER] ERROR: Worker.exe file not found! Make sure Worker.exe is in the same directory as LoadBalancer.exe\n");
        } else if (error == ERROR_PATH_NOT_FOUND) {
            printf("[WORKER_CONTROLLER] ERROR: Worker.exe path not found!\n");
        }
        return;
    }

    // Store process handle
    EnterCriticalSection(&worker_lock);
    active_workers[active_worker_count++] = pi.hProcess;
    int worker_id = active_worker_count;
    LeaveCriticalSection(&worker_lock);

    // Close thread handle (we only need process handle)
    CloseHandle(pi.hThread);

    printf("[WORKER_CONTROLLER] Spawned worker #%d (PID: %lu). Active: %d/%d\n",
        worker_id, pi.dwProcessId, active_worker_count, MAX_WORKER_PROCESSES);
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
	printf("[MONITOR] SCALE_UP_THRESHOLD: %.0f%%, SCALE_DOWN_THRESHOLD: %.0f%%\n", 
		(float)SCALE_UP_THRESHOLD, (float)SCALE_DOWN_THRESHOLD);
	printf("[MONITOR] MAX_QUEUE_SIZE: %d (Scale-up triggers at %d items)\n", 
		MAX_QUEUE_SIZE, (int)(MAX_QUEUE_SIZE * SCALE_UP_THRESHOLD / 100.0f));

	int check_count = 0;
	DWORD last_scale_down_time = 0;
	DWORD last_spawn_time = 0;
	DWORD last_critical_log_time = 0;
	DWORD last_status_log_time = 0;
	bool was_critical = false;

	int last_worker_count = -1;

	while (keep_running) {
		Sleep(MONITOR_CHECK_INTERVAL);

		int queue_count = 0;
		int peak_count = 0;
		long long total_enqueued = 0;
		long long total_dequeued = 0;
		request_queue_get_stats(&queue_count, &peak_count, &total_enqueued, &total_dequeued);
		float occupancy = ((float)queue_count / (float)MAX_QUEUE_SIZE) * 100.0f;
		float peak_occupancy = ((float)peak_count / (float)MAX_QUEUE_SIZE) * 100.0f;

		DWORD now = GetTickCount();
		check_count++;

		cleanup_dead_workers();

		// Session cleanup is intentionally not periodic because active sockets are short-lived

		EnterCriticalSection(&worker_lock);

		// Print status only on worker-count changes or periodic heartbeat (max every 3s)
		if (active_worker_count != last_worker_count || now - last_status_log_time >= 3000) {
			printf("[MONITOR] Queue: %d/%d items (%.1f%%) | Workers: %d/%d\n", 
				queue_count, MAX_QUEUE_SIZE, occupancy, 
				active_worker_count, MAX_WORKER_PROCESSES);

			last_worker_count = active_worker_count;
			last_status_log_time = now;
		}

		bool is_critical = (occupancy > SCALE_UP_THRESHOLD && active_worker_count >= MAX_WORKER_PROCESSES);

		// Scale UP when queue gets too full - use ADAPTIVE spawn rate
		if (occupancy > SCALE_UP_THRESHOLD || peak_occupancy > SCALE_UP_THRESHOLD) {
			if (active_worker_count < MAX_WORKER_PROCESSES) {
				was_critical = false;

				// Adaptive spawn interval: spawn faster if queue is very full
				int spawn_interval = MIN_SPAWN_INTERVAL_MS;
				if (occupancy > 90.0f) {
					spawn_interval = 100;  // Spawn faster if queue > 90%
				} else if (occupancy > 80.0f) {
					spawn_interval = 150;
				}

				if (now - last_spawn_time >= spawn_interval) {
					printf("[MONITOR] SCALE UP: queue %.1f%% (peak %.1f%%), spawning worker.\n", occupancy, peak_occupancy);
					LeaveCriticalSection(&worker_lock);
					spawn_worker();
					EnterCriticalSection(&worker_lock);
					last_spawn_time = now;
				}
			}
			else {
				// Log critical state only on transitions or after 3s timeout
				if (!was_critical || now - last_critical_log_time >= 3000) {
					printf("[MONITOR] Queue critical (%.1f%%), max workers (%d) reached.\n",
						occupancy, MAX_WORKER_PROCESSES);
					last_critical_log_time = now;
					was_critical = true;
				}
			}
		}

		// Scale DOWN - only one worker at a time and only if occupancy is low for a sustained period
		else if (occupancy < SCALE_DOWN_THRESHOLD && active_worker_count > 1) {
			was_critical = false;

			// Shut down one worker if it's been a while since the last scale down (to avoid rapid flapping)
			// AND peak occupancy is also low (meaning sustained low load, not temporary dip)
			if (now - last_scale_down_time >= WORKER_SHUTDOWN_INTERVAL && peak_occupancy < SCALE_DOWN_THRESHOLD) {
				printf("[MONITOR] SCALE DOWN: queue %.1f%% (peak %.1f%%), terminating one worker.\n", 
					occupancy, peak_occupancy);

				EnergyRequest poison_pill = { 0 };
				poison_pill.userId = -1;
				LeaveCriticalSection(&worker_lock);
				request_queue_enqueue(poison_pill);
				EnterCriticalSection(&worker_lock);

				last_scale_down_time = now;
			}
		}
		else {
			was_critical = false;
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
