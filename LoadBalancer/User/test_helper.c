#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN

#include "test_helper.h"

TestResult test_begin(const char* name, int total_requests) {
    TestResult result = {
        .test_name = name,
        .total_requests = total_requests,
        .successful_requests = 0,
        .failed_requests = 0,
        .duration_ms = 0,
        .throughput_req_per_sec = 0.0,
        .avg_response_time_ms = 0.0,
        .start_time = GetTickCount()
    };
    
    printf("\n========================================\n");
    printf("   TEST: %s\n", name);
    printf("   Target: %d requests\n", total_requests);
    printf("========================================\n\n");
    
    return result;
}

void test_record_success(TestResult* result) {
    result->successful_requests++;
}

void test_record_failure(TestResult* result) {
    result->failed_requests++;
}

void test_end(TestResult* result) {
    result->duration_ms = GetTickCount() - result->start_time;
    
    if (result->duration_ms > 0) {
        result->throughput_req_per_sec = (double)result->successful_requests / (result->duration_ms / 1000.0);
        result->avg_response_time_ms = (double)result->duration_ms / result->successful_requests;
    }
    
    printf("\n========================================\n");
    printf("   TEST RESULTS: %s\n", result->test_name);
    printf("========================================\n\n");
    printf("Duration:                  %lu ms (%.2f seconds)\n", result->duration_ms, result->duration_ms / 1000.0);
    printf("Total Requests Sent:       %d\n", result->total_requests);
    printf("Successful:                %d\n", result->successful_requests);
    printf("Failed:                    %d\n", result->failed_requests);
    printf("Success Rate:              %.2f%%\n", 
        (result->total_requests > 0) ? (100.0 * result->successful_requests / result->total_requests) : 0.0);
    printf("Throughput:                %.2f req/sec\n", result->throughput_req_per_sec);
    printf("Avg Response Time:         %.2f ms\n", result->avg_response_time_ms);
    printf("\n========================================\n\n");
}

void test_print_summary(TestResult results[], int num_tests) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║              COMPREHENSIVE TEST SUMMARY REPORT                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n\n");
    
    DWORD total_duration = 0;
    int total_requests = 0;
    int total_successful = 0;
    int total_failed = 0;
    
    for (int i = 0; i < num_tests; i++) {
        printf("[Test %d] %s\n", i + 1, results[i].test_name);
        printf("  Duration:     %lu ms | Requests: %d/%d (%d failed) | Throughput: %.2f req/sec\n",
            results[i].duration_ms, 
            results[i].successful_requests, 
            results[i].total_requests,
            results[i].failed_requests,
            results[i].throughput_req_per_sec);
        printf("  Avg Response: %.2f ms\n\n", results[i].avg_response_time_ms);
        
        total_duration += results[i].duration_ms;
        total_requests += results[i].total_requests;
        total_successful += results[i].successful_requests;
        total_failed += results[i].failed_requests;
    }
    
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║  OVERALL RESULTS                                                   ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    printf("Total Duration:            %lu ms (%.2f seconds)\n", total_duration, total_duration / 1000.0);
    printf("Total Requests:            %d\n", total_requests);
    printf("Successful:                %d\n", total_successful);
    printf("Failed:                    %d\n", total_failed);
    printf("Overall Success Rate:      %.2f%%\n", 
        (total_requests > 0) ? (100.0 * total_successful / total_requests) : 0.0);
    printf("Average Throughput:        %.2f req/sec\n", 
        (total_duration > 0) ? (total_successful / (total_duration / 1000.0)) : 0.0);
    printf("\n════════════════════════════════════════════════════════════════════\n\n");
}
