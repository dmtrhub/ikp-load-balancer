#ifndef USER_H
#define USER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "../Common/common_types.h"
#include "../Common/network_utils.h"

#pragma comment(lib, "ws2_32.lib")

// Test a single user request
void test_single_request(int user_id, float energy_kw);

// Small load test (5 requests)
void test_small_load(void);

// Medium load test (100 requests)
void test_medium_load(void);

// Heavy load test (10000 requests)
void test_heavy_load(void);

// Worker thread for concurrent request handling
DWORD WINAPI heavy_load_worker_thread(LPVOID lpParam);

int main(int argc, char* argv[]);

#endif
