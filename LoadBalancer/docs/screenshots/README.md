# Screenshots for Load Balancer Project

Add screenshots of:

1. **01-lb-startup.png** - LoadBalancer initialization output showing:
   - Component initialization
   - Port setup (5059, 5060, 5061, 5062)
   - Initial worker spawn

2. **02-autoscaling-monitor.png** - Monitor thread logs showing:
   - Queue occupancy progression
   - SCALE UP messages
   - SCALE DOWN messages
   - Worker count changes (1 → 24)

3. **03-stress-test-output.png** - Stress test completion:
   - "Stress test completed in 7.05 seconds!"
   - "Throughput: 1419 req/sec"
   - Worker scaling output

4. **04-test1-small.png** - Test 1 results:
   - 5 requests with sequential processing
   - Individual pricing results (green/blue/red zones)
   - Total costs displayed

5. **05-test2-medium.png** - Test 2 results:
   - 100 sequential requests
   - Sample user results
   - "Medium load test completed!" message

6. **06-test3-stress.png** - Test 3 results:
   - 10,000 concurrent threads spawned
   - Final throughput: 1419 req/sec
   - Duration: 7.05 seconds

## How to Add Screenshots

1. Run each test scenario
2. Take screenshots of the console output
3. Save as PNG files with the names above (01-, 02-, etc.)
4. Place in this directory (docs/screenshots/)
5. Reference in main README.md will work automatically
