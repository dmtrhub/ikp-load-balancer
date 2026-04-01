#ifndef CONFIG_H
#define CONFIG_H

// ===== NETWORK PORTS =====
#define LISTENER_PORT 5059          // User incoming requests
#define PRICE_DISTRIBUTION_PORT 5060 // Price config to workers
#define RESULT_RECEIVER_PORT 5061   // Worker results
#define REQUEST_DISTRIBUTOR_PORT 5062 // Requests to workers

// ===== QUEUE CONFIGURATION =====
#define MAX_QUEUE_SIZE 200
#define RESPONSE_QUEUE_SIZE 200
#define SCALE_UP_THRESHOLD 75        // Spawn when queue > 75%
#define SCALE_DOWN_THRESHOLD 25      // Consider scale-down when queue < 25%
#define MONITOR_CHECK_INTERVAL 150
#define WORKER_SHUTDOWN_INTERVAL 3000 // Wait 3s before actually scaling down (prevents flapping)

// ===== WORKER MANAGEMENT =====
#define MAX_WORKER_PROCESSES 24      // Dobar balans (umesto 15, ali ne preuobiman kao 32)
#define MAX_CONCURRENT_USERS 10000
#define WORKER_EXECUTABLE "Worker.exe"

// ===== WORKER SPAWN CONTROL =====
#define MIN_SPAWN_INTERVAL_MS 500    // Sporiji spawn (daj vremena da vidiš da li je potreban)
#define MAX_SPAWN_PER_CHECK 2

// ===== TIMEOUTS =====
#define SOCKET_TIMEOUT_MS 5000
#define ACCEPT_TIMEOUT_MS 1000

// ===== PRICE ZONES =====
#define GREEN_ZONE_BORDER 350.0     // kWh
#define BLUE_ZONE_BORDER 1600.0     // kWh
#define GREEN_PRICE 12.0            // RSD/kWh
#define BLUE_PRICE 18.0             // RSD/kWh
#define RED_PRICE 36.0              // RSD/kWh

#endif