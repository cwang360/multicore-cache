#ifndef __CACHE_H
#define __CACHE_H

#include "lrustack.h"
#include "global_types.h"


// cache types
#define L1 0
#define L2 1

typedef struct cache_block_t {
	int tag;
	int valid;
	int dirty;
    uint8_t* data;
} cache_block_t;

typedef struct cache_set_t {
	int size;				// Number of blocks in this cache set
	LruStack* stack;		    // LRU Stack 
	cache_block_t* blocks;	// Array of cache block structs. 
} cache_set_t;


/**
 * Struct to hold global stats
*/
typedef struct stats_t {
	counter_t accesses;
	counter_t hits;
	counter_t misses;
	double miss_rate;
	counter_t traffic;
	counter_t writebacks;
	double amat;
	counter_t data_accesses;
    counter_t data_misses;
   	counter_t instr_accesses;
    counter_t instr_misses;

    int hit_time;
    int miss_penalty;
} stats_t;

typedef struct config_t {
    int line_size;
    int cache_size;
    int associativity;
    int hit_time;
    int miss_penalty;
    int cache_type;
} config_t;

typedef struct add_result_t {
	int evicted;
	addr_t evicted_addr;
	int evicted_dirty;
} add_result_t;

typedef struct access_result_t {
    int hit;
    uint8_t data;
} access_result_t;

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
        cache_set_t* cache;		// Array of cache sets representing the cache.
        int cache_type;
        bus_t* bus;
    public:
        // Cache(int _block_size, int _cache_size, int _ways, int _hit_time, int _miss_penalty);
        void init(config_t config, bus_t* bus);
        ~Cache();
        
        uint8_t user_access(addr_t physical_addr, access_t access_type, uint8_t data);
        void system_access(addr_t physical_addr, access_t access_type);

        uint8_t try_access(addr_t physical_addr, access_t access_type, uint8_t data);
        add_result_t add_block(addr_t physical_addr, access_t access_type);
        int invalidate(addr_t evicted_addr);
        int check_dirty(addr_t physical_addr);

        void print_stats();
        stats_t* get_stats();
};

#endif