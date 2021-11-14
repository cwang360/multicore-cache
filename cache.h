#ifndef __CACHE_H
#define __CACHE_H

#include "lrustack.h"

typedef unsigned long long addr_t;		// Data type to hold addresses
typedef unsigned long long counter_t;	// Data type to hold cache statistic variables

// Access types
#define MEMREAD 0
#define MEMWRITE 1
#define IFETCH 2
#define MARKDIRTY 3

typedef struct cache_block_t {
	int tag;
	int valid;
	int dirty;
} cache_block_t;

/**
 * Struct for a cache set. Feel free to change any of this if you want. 
 */
typedef struct cache_set_t {
	int size;				// Number of blocks in this cache set
	LruStack* stack;		    // LRU Stack 
	cache_block_t* blocks;	// Array of cache block structs. You will need to
							// 	dynamically allocate based on number of blocks
							//	per set. 
} cache_set_t;


/**
 * Struct to hold global stats
*/
typedef struct stats_t {
	counter_t accesses;
	counter_t hits;
	counter_t misses;
	float miss_rate;
	counter_t traffic;
	counter_t writebacks;
	float amat;
	counter_t data_accesses;
    counter_t data_misses;
   	counter_t instr_accesses;
    counter_t instr_misses;
} stats_t;

typedef struct config_t {
    int line_size;
    int cache_size;
    int associativity;
    int hit_time;
    int miss_penalty;
} config_t;

class Cache {
    private:
        stats_t stats;
        int block_size;			// Size of a cache block in bytes
        int cache_size;			// Size of cache in bytes
        int ways;				// Number of ways
        int num_blocks;         // Number of total cache blocks
        int num_sets;           // Number of sets
        int num_offset_bits;    // Number of offset bits
        int num_index_bits;     // Number of index bits. 
        int hit_time;
        int miss_penalty;
        counter_t hits;			// local hits for cache
        counter_t misses;		// local misses for cache
        counter_t traffic;		// local traffic with l2 cache
        cache_set_t* cache;		// Array of cache sets representing the cache.
    public:
        // Cache(int _block_size, int _cache_size, int _ways, int _hit_time, int _miss_penalty);
        void init(int _block_size, int _cache_size, int _ways, int _hit_time, int _miss_penalty);
        ~Cache();
        void access(addr_t physical_addr, int access_type);
        void print_stats();
        stats_t* get_stats();
};

#endif