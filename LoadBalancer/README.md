# Dynamic Load Balancer (C / WinSock2 / Multi-Process)

A systems-programming portfolio project that implements a dynamic load balancing service for electricity billing requests with adaptive worker scaling.

The system is composed of three process types:
- **User** – test client sending consumption requests
- **LoadBalancer** – accepts requests, manages queue, scales workers adaptively
- **Worker** – processes requests, computes per-zone usage and pricing

All components communicate via TCP sockets on localhost (Windows `WinSock2`).

---

## Features

- **Custom queue data structure** (no STL) with blocking synchronization
- **Async socket handling** – receiver threads process worker results in parallel
- **Producer-consumer pattern** with intelligent backpressure
- **Adaptive autoscaling** – monitor watches queue occupancy and spawns/terminates workers
- **Graceful shutdown** – poison pills, thread coordination, resource cleanup
- **Stress testing** – supports ~10,000 concurrent requests with fire-and-forget mode
- **Multi-zone pricing** – green/blue/red tariffs based on consumption thresholds

---

## Autoscaling Rules

The queue occupancy controls worker scaling dynamically:

| Occupancy | Action |
|-----------|--------|
| **< 25%** | Scale down: terminate one worker per 3s (if > 1 worker active) |
| **25% - 75%** | Hold: maintain current worker count |
| **> 75%** | Scale up: spawn new worker (if < 24) |
| **≥ 80% (backpressure)** | Listener rejects new requests temporarily |

**Peak detection**: Scale-up triggers if either current OR peak occupancy exceeds threshold, preventing stalls during queue fluctuations.

---

## Project Structure

```
LoadBalancer/
├── Common/
│   ├── network_utils.c/h        → WinSock2 send/recv/socket setup
│   └── common_types.h            → shared EnergyRequest/PriceResult structs
├── LoadBalancer/
│   ├── load_balancer.c/h         → main() and thread orchestration
│   ├── connection_handler.c/h    → listener/distributor/receiver/sender threads
│   ├── worker_controller.c/h     → spawn_worker(), monitor_thread, autoscaling
│   ├── queue_manager.c/h         → custom blocking queue (request & response)
│   ├── user_session_table.c/h    → session tracking for user sockets
│   └── config.h                  → ports, thresholds, timeouts
├── Worker/
│   ├── worker.c/h                → worker process (register, receive job, calculate, send result)
│   └── pricing calculations      → green/blue/red zone logic
└── User/
    ├── user.c/h                  → test client executable
    ├── test_helper.c/h           → test harness (small/medium/stress scenarios)
    └── main() entry point
```

---

## Request Flow (Happy Path)

```
1. User sends EnergyRequest → LB Listener (port 5059)
2. LB stores request in queue + registers user session
3. Worker connects → LB Distributor (port 5062), requests job
4. LB dequeues request, sends to Worker
5. Worker calculates zone split & total cost
6. Worker sends PriceResult → LB Receiver (port 5061)
7. LB receiver async handler enqueues result
8. LB Sender forwards result to user socket
9. User receives PriceResult
10. Sockets close, sessions cleaned up
```

**Pricing Configuration**: Sent by LB to each Worker on registration (port 5060).

---

## Build & Run

### Visual Studio (Recommended)
1. Open `LoadBalancer.sln` in Visual Studio 2022+
2. Build solution → `x64 Debug` or `x64 Release`
3. Run processes:
   - Terminal 1: `LoadBalancer\x64\Debug\LoadBalancer.exe`
   - Terminal 2: `LoadBalancer\User\Debug\User.exe`
   - Select test mode (1-3)

### Command Line (PowerShell)
```powershell
cd LoadBalancer\x64\Debug
.\LoadBalancer.exe &
Start-Sleep -Seconds 2

cd ..\..\User\Debug
echo "3" | .\User.exe
```

---

## Test Modes

| Test | Requests | Concurrency | Purpose |
|------|----------|-------------|---------|
| **1 - Small** | 5 | Sequential | Functional validation (pricing calc correctness) |
| **2 - Medium** | 100 | Sequential | Stable processing, monitoring output |
| **3 - Stress** | 10,000 | 100 concurrent threads | Autoscaling validation, peak load handling |

Fire-and-forget mode enabled for Tests 2–3: workers send results, but user doesn't wait for reply. This allows Load Balancer to focus on throughput rather than session management.

---

## Performance Characteristics

- **Throughput**: ~500–1,500 requests/sec (depending on worker count and CPU)
- **Latency**: ~10–50ms per request (calculation + socket I/O)
- **Autoscaling latency**: ~500ms (spawn interval) + worker startup overhead
- **Queue depth**: 0–200 items (backpressure prevents overflow)

---

## Key Optimizations

1. **Async Receiver Handler Threads**: Each worker result gets its own handler thread → parallel recv() + enqueue() instead of sequential bottleneck
2. **Peak Occupancy Tracking**: Monitor checks both current AND peak queue levels → spawns workers before queue fills again
3. **Backpressure Backoff**: Listener rejects at 80% queue, preventing cascade failures
4. **Session Table**: Pre-allocated socket tracking (no malloc per user)
5. **Fixed Socket Buffers**: 256KB send/recv buffers for sustained throughput

---

## Configuration

All thresholds and timeouts in `LoadBalancer/config.h`:

```c
#define MAX_QUEUE_SIZE 200                    // Request queue capacity
#define SCALE_UP_THRESHOLD 75                 // Spawn when occupancy > 75%
#define SCALE_DOWN_THRESHOLD 25               // Consider termination when < 25%
#define WORKER_SHUTDOWN_INTERVAL 3000         // 3s delay before scale-down (prevent flapping)
#define MAX_WORKER_PROCESSES 24               // Hard limit on worker count
#define MIN_SPAWN_INTERVAL_MS 500             // Throttle spawn rate
```

---

## License

MIT License – see [LICENSE](LICENSE) file for details.

This project is provided as-is for educational and portfolio purposes.

---

## Notes

- **Graceful shutdown**: Pressing any key in LoadBalancer sends poison pills to all workers → clean exit
- **Fire-and-forget mode**: Large tests (2–3) intentionally skip user-socket replies to maximize throughput
- **Thread-safe queues**: Both request and response queues use critical sections for synchronization
- **Single-machine design**: All processes on localhost (127.0.0.1) – no network overhead
- **Focus**: Systems programming (sockets, processes, threads, queues, synchronization) over business logic
