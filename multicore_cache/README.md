# multicore-cache
Simulation for cache coherence protocols
1. Snoopy (bus-based) coherence protocols
    - MSI
    - MESI
    - MOESI
    - etc.
    - Hierarchical snooping?
2. Directory based coherence protocols

### Considerations:
- Need to keep track of data as well (done)
- How to model bus and bus control
    - arbiter
        - one core per clock cycle?
        - smarter arbiter to take requesting core when no other cores are requesting?
- trace file format? line by line wouldn't take into account simultaneous access
    - a separate trace for each core?
    - time, access type, address
    - what if multiple cores request at same time? Need mutex lock for writes; assume this is already handled by OS...so one request at at time
## Compile and run simulation
```
g++ lrustack.cc cache.cc simulator.cc -o simulator
```
```
./simulator <config> <trace>
```
## Config file format
```
<line size>, <cache size>, <associativity>, <hit time>, <miss penalty>
```