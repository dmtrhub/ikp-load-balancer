#include "../Common/network_utils.h"
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <stdbool.h>
#include "config.h"
#include "connection_handler.h"
#include "worker_controller.h"
#include "queue_manager.h"
#include "user_session_table.h"

// Global shutdown flag - used by all threads
volatile bool keep_running = true;

// Function to wake up blocked threads during shutdown
static void wakeup_threads() {
    printf("[MAIN] Waking up blocked threads...\n");

    // Send stop signals to queues
    EnergyRequest stop_req = { .userId = -1 };
    request_queue_enqueue(stop_req);

    PriceResult stop_res = { .userId = -1 };
    response_queue_enqueue(stop_res);

    // Connect to listening ports to wake accept() calls
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Wake listener on LISTENER_PORT
    SOCKET wake_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_port = htons(LISTENER_PORT);
    connect(wake_listener, (struct sockaddr*)&addr, sizeof(addr));
    closesocket(wake_listener);
    Sleep(10);

    // Wake price distributor on PRICE_DISTRIBUTION_PORT
    SOCKET wake_price = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_port = htons(PRICE_DISTRIBUTION_PORT);
    connect(wake_price, (struct sockaddr*)&addr, sizeof(addr));
    closesocket(wake_price);
    Sleep(10);

    // Wake receiver on RESULT_RECEIVER_PORT
    SOCKET wake_receiver = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_port = htons(RESULT_RECEIVER_PORT);
    connect(wake_receiver, (struct sockaddr*)&addr, sizeof(addr));
    closesocket(wake_receiver);
    Sleep(10);

    // Wake distributor on REQUEST_DISTRIBUTOR_PORT
    SOCKET wake_distributor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_port = htons(REQUEST_DISTRIBUTOR_PORT);
    connect(wake_distributor, (struct sockaddr*)&addr, sizeof(addr));
    closesocket(wake_distributor);

    printf("[MAIN] All threads notified.\n");
}

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("   LOAD BALANCER - Energy Price Service\n");
    printf("========================================\n\n");

    // Initialize network subsystem
    if (!init_networks()) {
        printf("[MAIN] ERROR: Failed to initialize networks!\n");
        return 1;
    }

    // Initialize all components
    printf("[MAIN] Initializing components...\n");
    user_session_init();
    worker_controller_init();
    request_queue_init();
    response_queue_init();

    // Configure pricing zones
    PricingConfiguration pricing = {
        .greenBorder = GREEN_ZONE_BORDER,
        .blueBorder = BLUE_ZONE_BORDER,
        .greenPrice = GREEN_PRICE,
        .bluePrice = BLUE_PRICE,
        .redPrice = RED_PRICE
    };

    printf("[MAIN] Pricing Configuration:\n");
    printf("       Green: 0-%.0f kWh @ %.2f RSD/kWh\n", pricing.greenBorder, pricing.greenPrice);
    printf("       Blue:  %.0f-%.0f kWh @ %.2f RSD/kWh\n", pricing.greenBorder, pricing.blueBorder, pricing.bluePrice);
    printf("       Red:   >%.0f kWh @ %.2f RSD/kWh\n\n", pricing.blueBorder, pricing.redPrice);

    // Create and start threads
    printf("[MAIN] Creating worker threads...\n");

    HANDLE threads[6];

    // Thread 0: Price Distributor (sends pricing to workers)
    threads[0] = CreateThread(NULL, 0, price_distributor_thread, &pricing, 0, NULL);

    // Thread 1: Monitor (checks queue occupancy and scales workers)
    threads[1] = CreateThread(NULL, 0, monitor_thread, NULL, 0, NULL);

    // Thread 2: Receiver (collects results from workers)
    threads[2] = CreateThread(NULL, 0, receiver_thread, NULL, 0, NULL);

    // Thread 3: Sender (sends results back to users)
    threads[3] = CreateThread(NULL, 0, sender_thread, NULL, 0, NULL);

    // Thread 4: Listener (accepts user connections)
    threads[4] = CreateThread(NULL, 0, listener_thread, NULL, 0, NULL);

    // Thread 5: Distributor (sends requests to workers)
    threads[5] = CreateThread(NULL, 0, distributor_thread, NULL, 0, NULL);

    // Verify thread creation
    for (int i = 0; i < 6; i++) {
        if (threads[i] == NULL) {
            printf("[MAIN] ERROR: Failed to create thread %d\n", i);
            return 1;
        }
    }

    // Spawn initial worker
    printf("[MAIN] Spawning initial worker...\n");
    spawn_worker();

    printf("\n========================================\n");
    printf("Load Balancer is running!\n");
    printf("Press any key to initiate GRACEFUL SHUTDOWN...\n");
    printf("========================================\n\n");

    // Wait for user input to shutdown
    while (!_kbhit()) {
        Sleep(100);
    }

    printf("\n[MAIN] SHUTDOWN INITIATED\n");
    printf("[MAIN] Signaling all threads to stop...\n\n");

    // Signal shutdown
    keep_running = false;

    // Wake up all blocked threads
    wakeup_threads();

    // Terminate workers gracefully
    terminate_all_workers();

    // Wait for all threads to complete
    printf("[MAIN] Waiting for all threads to finish...\n");
    DWORD result = WaitForMultipleObjects(6, threads, TRUE, INFINITE);

    if (result == WAIT_OBJECT_0) {
        printf("[MAIN] All threads completed successfully.\n");
    }
    else {
        printf("[MAIN] WARNING: Thread wait returned: %lu\n", result);
    }

    // Close thread handles
    printf("[MAIN] Closing thread handles...\n");
    for (int i = 0; i < 6; i++) {
        CloseHandle(threads[i]);
    }

    // Cleanup all components
    printf("[MAIN] Cleaning up components...\n");
    user_session_destroy();
    worker_controller_destroy();
    request_queue_destroy();
    response_queue_destroy();

    // Cleanup network
    cleanup_networks(NULL, 0);

    printf("\n========================================\n");
    printf("Load Balancer shutdown complete.\n");
    printf("========================================\n");

    return 0;
}