#ifndef CONFIG_H
#define CONFIG_H

// ===== NETWORK PORTS =====
#define LISTENER_PORT 5059          // User incoming requests
#define PRICE_DISTRIBUTION_PORT 5060 // Price config to workers
#define RESULT_RECEIVER_PORT 5061   // Worker results
#define REQUEST_DISTRIBUTOR_PORT 5062 // Requests to workers

// ===== QUEUE CONFIGURATION =====
#define MAX_QUEUE_SIZE 100
#define SCALE_UP_THRESHOLD 70       // % when to spawn new workers
#define SCALE_DOWN_THRESHOLD 30     // % when to kill workers
#define MONITOR_CHECK_INTERVAL 100  // ms between occupancy checks
#define WORKER_SHUTDOWN_INTERVAL 1000 // ms between worker shutdowns

// ===== WORKER MANAGEMENT =====
#define MAX_WORKER_PROCESSES 20
#define MAX_CONCURRENT_USERS 10000
#define WORKER_EXECUTABLE "Worker.exe"

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