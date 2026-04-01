#ifndef TEST_HELPER_H
#define TEST_HELPER_H

#include <windows.h>
#include <stdio.h>
#include <time.h>

// Test result tracking
typedef struct {
    const char* test_name;
    int total_requests;
    int successful_requests;
    int failed_requests;
    DWORD duration_ms;
    double throughput_req_per_sec;
    double avg_response_time_ms;
    DWORD start_time;
} TestResult;

// Initialize test timer
TestResult test_begin(const char* name, int total_requests);

// Record test outcome
void test_record_success(TestResult* result);
void test_record_failure(TestResult* result);

// Finalize test and print results
void test_end(TestResult* result);

// Print comprehensive test report
void test_print_summary(TestResult results[], int num_tests);

#endif
