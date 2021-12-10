# multicore-cache
Trace-driven simulation for cache coherence protocols.
### Snoopy (bus-based) coherence protocols
Blue arrows are transitions driven by local (CPU) input, while red arrows are transitions driven by messages snooped from the bus. Bus message-triggered transitions not shown do not change state.   
- **MSI [implemented]**: Simple coherence protocol for write-back caches, with modified, shared, and invalid states. A dirty cache block is also written back to memory whenever the data is put on the bus due to another cache requesting it.   
    ![MSI state diagram](diagrams/MSI.png)
- **MESI [implemented]**: MSI with an addition of "Exclusive" state, when an invalid block receives a local read, and no other cache contains the block. The advantage of this is fewer bus transactions/invalidations since the exclusive block can be modified without invalidating the block in other caches. Based on the [Illinois protocol](https://dl.acm.org/doi/10.1145/800015.808204).
    ![MESI state diagram](diagrams/MESI.png)
- **MOESI [in progress]**: MESI with an addition of "Owner" state
- etc.
- Update-based like Dragon and Firefly?
- Hierarchical snooping?

State transition logic is abstracted in [Cache::transition_bus](cache.cc#L278) and [Cache::transition_processor](cache.cc#L338)
### Directory based coherence protocols
- Each cache block has corresponding entry in directory. Entry is num_cores + 1 bits wide, with one bit corresponding to whether the block is valid in each core, and one bit for whether the block is exclusive to that core. 
### Considerations:
- LRU replacement policy used.
- Currently supports byte-sized read and writes. Default value (if not written to before) is 0.
- TODO: 
    - Fix writeback and AMAT stats for cache
    - Deal with bus widths smaller than line size (for calculating access times)
    - Generate memory traces with Intel's [Pin](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html) tool
    - Potentially model caches/memory as threads running concurrently. Will need arbiter or some kind of control for synchronizing bus usage.
## Compile and run simulation
To compile/link, run `make`. 
To run, using a single trace file for all cores:
```
$ ./simulator <config> -s <trace file>
```
Example:
```
$ ./simulator config.txt -s traces/simple.trace
```
If using a separate trace file for each core (order of accesses is unpredictable):
```
$ ./simulator <config> -p <space delimited list of trace files>
```
Use the `-v` flag for verbose output (see when there is a data request from memory, writeback to memory, invalidation, and state changes for cache blocks) and/or `-v` for testing  mode.
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
Each line should be of the following form:
```
<core num> <access type> <address> <data>
```
### Access type:
- data read = 0
- data write = 1
- instr fetch = 2
### Data:
- Expected value for a read
    - Required for testing mode, may omit for non-testing mode
- Value to be stored for a write