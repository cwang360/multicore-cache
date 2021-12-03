# multicore-cache
Trace-driven simulation for cache coherence protocols
### Snoopy (bus-based) coherence protocols
Blue arrows are transitions driven by local (CPU) input, while red arrows are transitions driven by messages snooped from the bus. Bus message-triggered transitions not shown do not change state.   
- **MSI [implemented]**: Simple coherence protocol for write-back caches, with modified, shared, and invalid states.  
    ![MSI state diagram](diagrams/MSI.png)
- **MESI [in progress]**: MSI with an addition of "Exclusive" state, when an invalid block receives a local read, and no other cache contains the block. The advantage of this is fewer bus transactions/invalidations since the exclusive block can be modified without invalidating the block in other caches. Based on the [Illinois protocol](https://dl.acm.org/doi/10.1145/800015.808204).
    ![MESI state diagram](diagrams/MESI.png)
- **MOESI**: MESI with an addition of "Owner" state
- etc.
- Update-based like Dragon and Firefly?
- Hierarchical snooping?

State transition logic is abstracted in Cache::transition_bus and Cache::transition_processor (see [cache.cc](cache.cc#L278))
### Directory based coherence protocols
- Each cache block has corresponding entry in directory. Entry is num_cores + 1 bits wide, with one bit corresponding to whether the block is valid in each core, and one bit for whether the block is exclusive to that core. 
### Considerations:
- LRU replacement policy used.
- Need to keep track of data as well (done)
- How to model bus and bus control
    - arbiter
        - one core per clock cycle?
        - smarter arbiter to take requesting core when no other cores are requesting?
- trace file format? line by line wouldn't take into account simultaneous access
    - a separate trace for each core?
    - time, access type, address

- Currently supports byte-sized read and writes. Default value (if not written to before) is 0.
- Deal with bus widths smaller than line size
- Generate memory traces with Intel's [Pin](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html) tool
## Compile and run simulation
```
$ g++ cache.cc memory.cc system.cc simulator.cc -o simulator
```
```
$ ./simulator <config> <space-delimited list of trace files>
```
List one trace file per core.
## Config file format
```
<number of cores>, <coherence protocol>
<line size>, <cache size>, <associativity>, <hit time>, <miss penalty>
<shared memory size>, <data bus width>
```
All sizes are in bytes.
### Coherence protocols:
- MSI = 0
- MESI = 1
## Trace file format
```
<core num> <access type> <address> <data>
```
### Access type:
- data read = 0
- data write = 1
- instr fetch = 2
### Data:
- Expected value for a read
- Value to be stored for a write