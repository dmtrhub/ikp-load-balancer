#ifndef WORKER_CONTROLLER_H
#define WORKER_CONTROLLER_H

#include "../Common/common_types.h"
#include "config.h"

// ===== WORKER PROCESS MANAGEMENT =====

void worker_controller_init();
void worker_controller_destroy();

void spawn_worker();

void terminate_all_workers();

int get_active_worker_count();

void cleanup_dead_workers();

// ===== MONITORING & SCALING =====

// Checks queue occupancy and scales workers
DWORD WINAPI monitor_thread(LPVOID lpParam);

// Send pricing info to workers on registration
DWORD WINAPI price_distributor_thread(LPVOID lpParam);

#endif
