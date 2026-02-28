#include "worker.h"

// Pricing configuration received from Load Balancer
static PricingConfiguration pricing = { 0 };

// Function to calculate electricity cost based on consumption
static float calculate_cost(float consumption_kw) {
	float cost = 0.0f;

	// Green zone: 0 to greenBorder
	if (consumption_kw <= pricing.greenBorder) {
		return consumption_kw * pricing.greenPrice;
	}

	// Consumption enters green zone
	cost += pricing.greenBorder * pricing.greenPrice;
	float remaining = consumption_kw - pricing.greenBorder;

	// Blue zone: greenBorder to blueBorder
	if (remaining <= (pricing.blueBorder - pricing.greenBorder)) {
		cost += remaining * pricing.bluePrice;
		return cost;
	}

	// Consumption enters blue zone
	cost += (pricing.blueBorder - pricing.greenBorder) * pricing.bluePrice;
	remaining = consumption_kw - pricing.blueBorder;

	// Red zone: above blueBorder
	cost += remaining * pricing.redPrice;

	return cost;
}

// Process a single request from Load Balancer
static void process_request(EnergyRequest request) {
	printf("[WORKER] Processing request from User %d (%.2f kW)\n",
		request.userId, request.consumedEnergy);

	// Calculate cost
	float total_cost = calculate_cost(request.consumedEnergy);

	// Determine energy distribution by zone
	float green_energy = 0.0f;
	float blue_energy = 0.0f;
	float red_energy = 0.0f;

	if (request.consumedEnergy <= pricing.greenBorder) {
		green_energy = request.consumedEnergy;
	}
	else if (request.consumedEnergy <= pricing.blueBorder) {
		green_energy = pricing.greenBorder;
		blue_energy = request.consumedEnergy - pricing.greenBorder;
	}
	else {
		green_energy = pricing.greenBorder;
		blue_energy = pricing.blueBorder - pricing.greenBorder;
		red_energy = request.consumedEnergy - pricing.blueBorder;
	}

	// Prepare result
	PriceResult result = {
	.userId = request.userId,
	.socketId = request.socketId,
	.greenEnergy = green_energy,
	.blueEnergy = blue_energy,
	.redEnergy = red_energy,
	.totalCost = total_cost
	};

	printf("[WORKER] Calculated: Green=%.2f, Blue=%.2f, Red=%.2f, Total=%.2f RSD\n",
		green_energy, blue_energy, red_energy, total_cost);

	// Send result back to Load Balancer on port 5061
	SOCKET result_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in lb_addr;
	lb_addr.sin_family = AF_INET;
	lb_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	lb_addr.sin_port = htons(5061);  // RESULT_RECEIVER_PORT

	// Retry sending with exponential backoff
	int retry_count = 0;
	int retry_delay = 50;
	int max_retries = 20;

	while (connect(result_socket, (struct sockaddr*)&lb_addr, sizeof(lb_addr)) != 0) {
		if (retry_count++ >= max_retries) {
			printf("[WORKER] ERROR: Could not connect to Load Balancer on port 5061 after %d retries\n", max_retries);
			closesocket(result_socket);
			return;  // Resign from sending result
		}
		Sleep(retry_delay);
		retry_delay = (retry_delay * 2 > 2000) ? 2000 : (retry_delay * 2);
		closesocket(result_socket);
		result_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}

	if (send_all(result_socket, &result, sizeof(PriceResult))) {
		printf("[WORKER] Result sent to Load Balancer\n");
	}
	else {
		printf("[WORKER] ERROR: Failed to send result\n");
	}

	closesocket(result_socket);
}

// Main worker loop
int main(int argc, char* argv[]) {
	printf("========================================\n");
	printf("   WORKER PROCESS\n");
	printf("========================================\n\n");

	// Initialize network
	if (!init_networks()) {
		printf("[WORKER] ERROR: Failed to initialize network!\n");
		return 1;
	}

	// Step 1: Register with Load Balancer and get pricing info (port 5060)
	printf("[WORKER] Registering with Load Balancer (port 5060)...\n");
	SOCKET price_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in lb_addr;
	lb_addr.sin_family = AF_INET;
	lb_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	lb_addr.sin_port = htons(5060);  // PRICE_DISTRIBUTION_PORT

	if (connect(price_socket, (struct sockaddr*)&lb_addr, sizeof(lb_addr)) != 0) {
		printf("[WORKER] ERROR: Could not connect to Load Balancer for pricing!\n");
		return 1;
	}

	// Receive pricing configuration
	if (!recv_all(price_socket, &pricing, sizeof(PricingConfiguration))) {
		printf("[WORKER] ERROR: Failed to receive pricing configuration!\n");
		closesocket(price_socket);
		return 1;
	}

	closesocket(price_socket);

	printf("[WORKER] Pricing received:\n");
	printf("         Green: 0-%.0f kWh @ %.2f RSD/kWh\n",
		pricing.greenBorder, pricing.greenPrice);
	printf("         Blue: %.0f-%.0f kWh @ %.2f RSD/kWh\n",
		pricing.greenBorder, pricing.blueBorder, pricing.bluePrice);
	printf("         Red: >%.0f kWh @ %.2f RSD/kWh\n\n",
		pricing.blueBorder, pricing.redPrice);

	// Step 2: Connect to request distributor and process jobs (port 5062)
	printf("[WORKER] Connecting to request distributor (port 5062)...\n");

	bool running = true;
	while (running) {
		// Connect to distributor to request a job
		SOCKET dist_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		lb_addr.sin_port = htons(5062);  // REQUEST_DISTRIBUTOR_PORT

		// Wait for connection - if LB is temporarily unavailable, retry with fixed delay
		// Worker should keep trying to connect to LB to get jobs, even if LB is restarting or temporarily overloaded
		while (connect(dist_socket, (struct sockaddr*)&lb_addr, sizeof(lb_addr)) != 0) {
			closesocket(dist_socket);
			Sleep(100);  // Short fixed delay, try again
			dist_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}

		// Receive request
		EnergyRequest request;
		if (!recv_all(dist_socket, &request, sizeof(EnergyRequest))) {
			closesocket(dist_socket);
			Sleep(50);
			continue;
		}

		closesocket(dist_socket);

		// Check for shutdown signal (poison pill by LB)
		if (request.userId == -1) {
			printf("[WORKER] Shutdown signal received. Exiting...\n");
			running = false;
			continue;
		}

		// Process the request
		process_request(request);
	}

	printf("\n========================================\n");
	printf("Worker process shutdown complete.\n");
	printf("========================================\n");

	return 0;
}