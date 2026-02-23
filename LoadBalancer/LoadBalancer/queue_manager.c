#include "queue_manager.h"
#include <string.h>

// ===== REQUEST QUEUE STATE =====
static EnergyRequest request_queue[MAX_QUEUE_SIZE];
static int req_head = 0, req_tail = 0, req_count = 0;
static CRITICAL_SECTION req_lock;
static CONDITION_VARIABLE req_not_empty;  // Wake workers when data arrives
static CONDITION_VARIABLE req_not_full;   // Wake listener when space freed
static CONDITION_VARIABLE req_occupancy_changed; // Wake monitor on changes

// ===== RESPONSE QUEUE STATE =====
static PriceResult response_queue[MAX_QUEUE_SIZE];
static int res_head = 0, res_tail = 0, res_count = 0;
static CRITICAL_SECTION res_lock;
static CONDITION_VARIABLE res_not_empty;  // Wake sender when data arrives

// ===== REQUEST QUEUE IMPLEMENTATION =====

void request_queue_init() {
    InitializeCriticalSection(&req_lock);
    req_head = req_tail = req_count = 0;
}

void request_queue_destroy() {
    DeleteCriticalSection(&req_lock);
}

void request_queue_enqueue(EnergyRequest req) {
    EnterCriticalSection(&req_lock);

    // Block if queue is full
    while (req_count == MAX_QUEUE_SIZE) {
        SleepConditionVariableCS(&req_not_full, &req_lock, INFINITE);
    }

    // Add to queue
    request_queue[req_tail] = req;
    req_tail = (req_tail + 1) % MAX_QUEUE_SIZE;
    req_count++;

    printf("[REQUEST_QUEUE] Enqueued request from User %d (%.2f kW). Count: %d/%d\n",
        req.userId, req.consumedEnergy, req_count, MAX_QUEUE_SIZE);

    // Wake workers waiting for data
    WakeConditionVariable(&req_not_empty);
    // Wake monitor to check occupancy
    WakeConditionVariable(&req_occupancy_changed);

    LeaveCriticalSection(&req_lock);
}

EnergyRequest request_queue_dequeue() {
    EnterCriticalSection(&req_lock);

    // Block if queue is empty
    while (req_count == 0) {
        SleepConditionVariableCS(&req_not_empty, &req_lock, INFINITE);
    }

    // Remove from queue
    EnergyRequest req = request_queue[req_head];
    req_head = (req_head + 1) % MAX_QUEUE_SIZE;
    req_count--;

    // Wake listener - space is now available
    WakeConditionVariable(&req_not_full);
    // Wake monitor - occupancy changed
    WakeConditionVariable(&req_occupancy_changed);

    LeaveCriticalSection(&req_lock);

    return req;
}

float request_queue_occupancy() {
    float occupancy = 0.0f;
    EnterCriticalSection(&req_lock);
    occupancy = ((float)req_count / MAX_QUEUE_SIZE) * 100.0f;
    LeaveCriticalSection(&req_lock);
    return occupancy;
}

int request_queue_count() {
    int count = 0;
    EnterCriticalSection(&req_lock);
    count = req_count;
    LeaveCriticalSection(&req_lock);
    return count;
}

// ===== RESPONSE QUEUE IMPLEMENTATION =====

void response_queue_init() {
    InitializeCriticalSection(&res_lock);
    res_head = res_tail = res_count = 0;
}

void response_queue_destroy() {
    DeleteCriticalSection(&res_lock);
}

void response_queue_enqueue(PriceResult res) {
    EnterCriticalSection(&res_lock);

    if (res_count < MAX_QUEUE_SIZE) {
        response_queue[res_tail] = res;
        res_tail = (res_tail + 1) % MAX_QUEUE_SIZE;
        res_count++;

        printf("[RESPONSE_QUEUE] Enqueued result for User %d (Cost: %.2f RSD). Count: %d/%d\n",
            res.userId, res.totalCost, res_count, MAX_QUEUE_SIZE);

        // Wake sender waiting for data
        WakeConditionVariable(&res_not_empty);
    }
    else {
        printf("[RESPONSE_QUEUE] WARNING: Queue full! Dropping response for User %d\n", res.userId);
    }

    LeaveCriticalSection(&res_lock);
}

PriceResult response_queue_dequeue() {
    PriceResult res = { 0 };
    EnterCriticalSection(&res_lock);

    // Block if queue is empty
    while (res_count == 0) {
        SleepConditionVariableCS(&res_not_empty, &res_lock, INFINITE);
    }

    // Remove from queue
    res = response_queue[res_head];
    res_head = (res_head + 1) % MAX_QUEUE_SIZE;
    res_count--;

    LeaveCriticalSection(&res_lock);

    return res;
}

int response_queue_count() {
    int count = 0;
    EnterCriticalSection(&res_lock);
    count = res_count;
    LeaveCriticalSection(&res_lock);
    return count;
}
