#include "user.h"

// Simulate a single user sending a request and receiving result
static void test_single_request(int user_id, float energy_kw) {
	printf("[USER %d] Sending request: %.2f kW\n", user_id, energy_kw);

	// Connect to Load Balancer on port 5059
	SOCKET user_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in lb_addr;
	lb_addr.sin_family = AF_INET;
	lb_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	lb_addr.sin_port = htons(5059);  // LISTENER_PORT

	if (connect(user_socket, (struct sockaddr*)&lb_addr, sizeof(lb_addr)) != 0) {
		printf("[USER %d] ERROR: Could not connect to Load Balancer!\n", user_id);
		closesocket(user_socket);
		return;
	}

	// Send energy request
	EnergyRequest request = {
		.userId = user_id,
      .socketId = 0,   // LB will replace with session id
		.consumedEnergy = energy_kw
	};

	if (!send_all(user_socket, &request, sizeof(EnergyRequest))) {
		printf("[USER %d] ERROR: Failed to send request!\n", user_id);
		closesocket(user_socket);
		return;
	}

	printf("[USER %d] Waiting for result...\n", user_id);

	// Receive result back
	PriceResult result;
	if (!recv_all(user_socket, &result, sizeof(PriceResult))) {
		printf("[USER %d] ERROR: Failed to receive result!\n", user_id);
		closesocket(user_socket);
		return;
	}

	closesocket(user_socket);

	// Display result
	printf("[USER %d] ===== RESULT =====\n", user_id);
	printf("[USER %d] Green Energy:  %.2f kWh\n", user_id, result.greenEnergy);
	printf("[USER %d] Blue Energy:   %.2f kWh\n", user_id, result.blueEnergy);
	printf("[USER %d] Red Energy:    %.2f kWh\n", user_id, result.redEnergy);
	printf("[USER %d] TOTAL COST:    %.2f RSD\n", user_id, result.totalCost);
	printf("[USER %d] ==================\n\n", user_id);
}

// Test mode 1: Small load (5 sequential requests)
static void test_small_load() {
	printf("\n========================================\n");
	printf("   SMALL LOAD TEST (5 requests)\n");
	printf("========================================\n\n");

	// Simulate 5 different users with different consumption
	float test_consumptions[] = { 100.0f, 500.0f, 1500.0f, 2000.0f, 3000.0f };

	for (int i = 0; i < 5; i++) {
		test_single_request(i + 1, test_consumptions[i]);
	}

	printf("Small load test completed!\n\n");
}

// Test mode 2: Medium load (100 requests)
static void test_medium_load() {
	printf("\n========================================\n");
	printf("   MEDIUM LOAD TEST (100 requests)\n");
	printf("========================================\n\n");

	int num_requests = 100;

	for (int i = 0; i < num_requests; i++) {
		// Random energy consumption between 50 and 4000 kW
		float energy = 50.0f + (rand() % 3950);
		test_single_request(i + 1, energy);
	}

	printf("Medium load test completed!\n\n");
}

// Test mode 3: Stress load for demonstrating auto-scaling under extreme load
// Sending 10000 concurrent requests with NO DELAYS to show how LB auto-scales and handles overload gracefully
typedef struct {
	int user_id;
	float energy;
} StressTask;

DWORD WINAPI stress_worker_thread(LPVOID lpParam) {
	StressTask* task = (StressTask*)lpParam;
	int user_id = task->user_id;
	float energy = task->energy;
	free(task);

	SOCKET user_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in lb_addr;
	lb_addr.sin_family = AF_INET;
	lb_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	lb_addr.sin_port = htons(5059);

	// Retry with exponential backoff if LB is overloaded (queue full)
	int retries = 0;
	while (connect(user_socket, (struct sockaddr*)&lb_addr, sizeof(lb_addr)) != 0) {
		if (retries++ > 100) {
			closesocket(user_socket);
			return 0;
		}
		Sleep(10);
		closesocket(user_socket);
		user_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}

	EnergyRequest request = {
		.userId = user_id,
		.socketId = -1,
		.consumedEnergy = energy
	};

	send_all(user_socket, &request, sizeof(EnergyRequest));

  // Fire-and-forget in stress mode (allowed by assignment for large tests)
	closesocket(user_socket);
	return 0;
}

static void test_stress_load() {
	printf("\n========================================\n");
	printf("   STRESS LOAD TEST (10000 concurrent)\n");
	printf("========================================\n\n");

	int num_requests = 10000;
	int max_concurrent = 100;  // 100 concurrent threads

	printf("Sending %d requests with %d concurrent threads (NO DELAYS)...\n\n",
		num_requests, max_concurrent);

	HANDLE active_threads[100];
	int active_count = 0;

	LARGE_INTEGER start, end, freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);

	for (int i = 0; i < num_requests; i++) {
		while (active_count >= max_concurrent) {
			Sleep(5);
			for (int j = 0; j < active_count; j++) {
				DWORD exit_code;
				if (GetExitCodeThread(active_threads[j], &exit_code) && exit_code != STILL_ACTIVE) {
					CloseHandle(active_threads[j]);
					for (int k = j; k < active_count - 1; k++) {
						active_threads[k] = active_threads[k + 1];
					}
					active_count--;
					j--;
				}
			}
		}

		StressTask* task = (StressTask*)malloc(sizeof(StressTask));
		task->user_id = i + 1;
		task->energy = 50.0f + (rand() % 3950);

		HANDLE thread = CreateThread(NULL, 0, stress_worker_thread, task, 0, NULL);
		if (thread != NULL) {
			active_threads[active_count++] = thread;
		} else {
			free(task);
		}

		if ((i + 1) % 1000 == 0) {
			printf("[STRESS] Spawned %d/%d threads...\n", i + 1, num_requests);
		}
	}

	printf("\nWaiting for remaining %d threads to complete...\n", active_count);
	WaitForMultipleObjects(active_count, active_threads, TRUE, INFINITE);

	for (int i = 0; i < active_count; i++) {
		CloseHandle(active_threads[i]);
	}

	QueryPerformanceCounter(&end);
	double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

	printf("\nStress test completed in %.2f seconds!\n", elapsed);
	printf("Throughput: %.0f req/sec\n\n", num_requests / elapsed);
}

int main(int argc, char* argv[]) {
	printf("========================================\n");
	printf("   USER CLIENT TEST PROGRAM\n");
	printf("========================================\n\n");

	// Initialize network
	if (!init_networks()) {
		printf("[USER] ERROR: Failed to initialize network!\n");
		return 1;
	}

	printf("Choose test mode:\n");
	printf("1 = Small load (5 requests)\n");
	printf("2 = Medium load (100 requests)\n");
	printf("3 = STRESS load (10000 concurrent, NO DELAYS - shows auto-scaling)\n\n");
	printf("Enter choice (1-3): ");

	int choice;
	scanf_s("%d", &choice);

	switch (choice) {
	case 1:
		test_small_load();
		break;
	case 2:
		test_medium_load();
		break;
	case 3:
		test_stress_load();
		break;
	default:
		printf("Invalid choice!\n");
		return 1;
	}

	printf("========================================\n");
	printf("All tests completed. Press any key to exit.\n");
	printf("========================================\n");
	getchar();
	getchar();  // Second getchar to consume newline

	return 0;
}