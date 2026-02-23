#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include "../Common/common_types.h"
#include "../LoadBalancer/config.h"

// ===== REQUEST QUEUE (User requests) =====

void request_queue_init();
void request_queue_destroy();

// Push request (blocks if full, wakes distributor and monitor)
void request_queue_enqueue(EnergyRequest req);

// Pop request (blocks if empty, wakes listener when space freed)
EnergyRequest request_queue_dequeue();

// Get occupancy percentage
float request_queue_occupancy();

// Get current count
int request_queue_count();


// ===== RESPONSE QUEUE (Worker results) =====

void response_queue_init();
void response_queue_destroy();

// Push response from worker
void response_queue_enqueue(PriceResult res);

// Pop response for sending to user
PriceResult response_queue_dequeue();

// Get current count
int response_queue_count();

#endif
