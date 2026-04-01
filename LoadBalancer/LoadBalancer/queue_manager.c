#include "queue_manager.h"

// ===== REQUEST QUEUE STATE =====
static EnergyRequest request_queue[MAX_QUEUE_SIZE];
static int req_head = 0, req_tail = 0, req_count = 0;
static int req_peak_count = 0;
static long long req_total_enqueued = 0;
static long long req_total_dequeued = 0;
static CRITICAL_SECTION req_lock;
static CONDITION_VARIABLE req_not_empty;
static CONDITION_VARIABLE req_not_full;

// ===== RESPONSE QUEUE STATE =====
static PriceResult response_queue[RESPONSE_QUEUE_SIZE];
static int res_head = 0, res_tail = 0, res_count = 0;
static CRITICAL_SECTION res_lock;
static CONDITION_VARIABLE res_not_empty;
static CONDITION_VARIABLE res_not_full;

void request_queue_init() {
    InitializeCriticalSection(&req_lock);
    InitializeConditionVariable(&req_not_empty);
    InitializeConditionVariable(&req_not_full);
    req_head = req_tail = req_count = 0;
    req_peak_count = 0;
    req_total_enqueued = 0;
    req_total_dequeued = 0;
}

void request_queue_destroy() {
    DeleteCriticalSection(&req_lock);
}

void request_queue_enqueue(EnergyRequest req) {
    EnterCriticalSection(&req_lock);
    while (req_count == MAX_QUEUE_SIZE) {
        SleepConditionVariableCS(&req_not_full, &req_lock, INFINITE);
    }

    request_queue[req_tail] = req;
    req_tail = (req_tail + 1) % MAX_QUEUE_SIZE;
    req_count++;
    req_total_enqueued++;
    if (req_count > req_peak_count) {
        req_peak_count = req_count;
    }

    WakeConditionVariable(&req_not_empty);
    LeaveCriticalSection(&req_lock);
}

EnergyRequest request_queue_dequeue() {
    EnergyRequest req;
    EnterCriticalSection(&req_lock);
    while (req_count == 0) {
        SleepConditionVariableCS(&req_not_empty, &req_lock, INFINITE);
    }

    req = request_queue[req_head];
    req_head = (req_head + 1) % MAX_QUEUE_SIZE;
    req_count--;
    req_total_dequeued++;

    WakeConditionVariable(&req_not_full);
    LeaveCriticalSection(&req_lock);
    return req;
}

void request_queue_get_stats(int* current_count, int* peak_count, long long* total_enqueued, long long* total_dequeued) {
    EnterCriticalSection(&req_lock);
    if (current_count != NULL) {
        *current_count = req_count;
    }
    if (peak_count != NULL) {
        *peak_count = req_peak_count;
    }
    if (total_enqueued != NULL) {
        *total_enqueued = req_total_enqueued;
    }
    if (total_dequeued != NULL) {
        *total_dequeued = req_total_dequeued;
    }
    req_peak_count = req_count;
    LeaveCriticalSection(&req_lock);
}

float request_queue_occupancy() {
    float occupancy;
    EnterCriticalSection(&req_lock);
    occupancy = ((float)req_count / (float)MAX_QUEUE_SIZE) * 100.0f;
    LeaveCriticalSection(&req_lock);
    return occupancy;
}

int request_queue_count() {
    int count;
    EnterCriticalSection(&req_lock);
    count = req_count;
    LeaveCriticalSection(&req_lock);
    return count;
}

void response_queue_init() {
    InitializeCriticalSection(&res_lock);
    InitializeConditionVariable(&res_not_empty);
    InitializeConditionVariable(&res_not_full);
    res_head = res_tail = res_count = 0;
}

void response_queue_destroy() {
    DeleteCriticalSection(&res_lock);
}

void response_queue_enqueue(PriceResult res) {
    EnterCriticalSection(&res_lock);
    while (res_count == RESPONSE_QUEUE_SIZE) {
        SleepConditionVariableCS(&res_not_full, &res_lock, INFINITE);
    }

    response_queue[res_tail] = res;
    res_tail = (res_tail + 1) % RESPONSE_QUEUE_SIZE;
    res_count++;

    WakeConditionVariable(&res_not_empty);
    LeaveCriticalSection(&res_lock);
}

PriceResult response_queue_dequeue() {
    PriceResult res;
    EnterCriticalSection(&res_lock);
    while (res_count == 0) {
        SleepConditionVariableCS(&res_not_empty, &res_lock, INFINITE);
    }

    res = response_queue[res_head];
    res_head = (res_head + 1) % RESPONSE_QUEUE_SIZE;
    res_count--;

    WakeConditionVariable(&res_not_full);
    LeaveCriticalSection(&res_lock);
    return res;
}

int response_queue_count() {
    int count;
    EnterCriticalSection(&res_lock);
    count = res_count;
    LeaveCriticalSection(&res_lock);
    return count;
}

float response_queue_occupancy() {
    float occupancy;
    EnterCriticalSection(&res_lock);
    occupancy = ((float)res_count / (float)RESPONSE_QUEUE_SIZE) * 100.0f;
    LeaveCriticalSection(&res_lock);
    return occupancy;
}
