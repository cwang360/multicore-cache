# memory-sim
Simulation for memory system with variable cache and virtual memory configurations
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