#ifndef __CACHE_H
#define __CACHE_H

#include "global_types.h"


// cache types
#define L1 0
#define L2 1

typedef enum {
    INVALID = 0,
    SHARED = 1,
    MODIFIED = 2,
    EXCLUSIVE = 3,
    OWNER = 4
} state_t;

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

class Cache {
    private:
        // Private types
        class LruStack;
        
        typedef struct cache_block_t {
            unsigned int tag;
            int valid;
            int dirty;
            state_t state;
            uint8_t* data;
        } cache_block_t;

        typedef struct cache_set_t {
            unsigned int size;				// Number of blocks in this cache set
            LruStack* stack;		    // LRU Stack 
            cache_block_t* blocks;	// Array of cache block structs. 
        } cache_set_t;

        typedef struct addr_split_t {
            unsigned int tag;
            unsigned int index;
            unsigned int offset;
        } addr_split_t;
        
        // Private instance variables
        stats_t stats;
        unsigned int block_size;			// Size of a cache block in bytes
        unsigned int cache_size;			// Size of cache in bytes
        unsigned int ways;				// Number of ways
        unsigned int num_blocks;         // Number of total cache blocks
        unsigned int num_sets;           // Number of sets
        unsigned int num_offset_bits;    // Number of offset bits
        unsigned int num_index_bits;     // Number of index bits. 
        int hit_time;
        int miss_penalty;
        cache_set_t* cache;		// Array of cache sets representing the cache.
        int cache_type;
        bus_t* bus;
        protocol_t protocol;

        // Private methods
        // Transition invoked by message snooped from bus
        void transition_bus(cache_block_t* cache_block, message_t bus_message);
        // Transition invoked by access from processor (local read or local write)
        void transition_processor(cache_block_t* cache_block, access_t processor_message);

        addr_split_t split_address(addr_t physical_addr);

    public:
        // Public types
        typedef struct config_t {
            unsigned int line_size;
            unsigned int cache_size;
            unsigned int associativity;
            int hit_time;
            int miss_penalty;
            int cache_type;
        } config_t;
        
        typedef struct add_result_t {
            bool evicted;
            addr_t evicted_addr;
            bool evicted_dirty;
        } add_result_t;

        // Public methods
        // Cache(int _block_size, int _cache_size, int _ways, int _hit_time, int _miss_penalty);
        void init(config_t config, protocol_t protocol, bus_t* bus);
        ~Cache();
        
        uint8_t processor_access(addr_t physical_addr, access_t access_type, uint8_t data);
        void system_access(addr_t physical_addr, access_t access_type);

        uint8_t try_access(addr_t physical_addr, access_t access_type, uint8_t data);
        add_result_t add_block(addr_t physical_addr, access_t access_type);
        bool invalidate(addr_t evicted_addr);
        bool check_dirty(addr_t physical_addr);

        void print_stats();
        stats_t* get_stats();
};

class Cache::LruStack {
    private:
        typedef struct stack_node {
            unsigned int index; // index of way
            struct stack_node* next_more_recent;
            struct stack_node* next_less_recent;
        } stack_node;

        unsigned int size;   // Corresponds to the associativity
        stack_node* most_recent;
        stack_node* least_recent;
        stack_node** index_map; // array to map a way's index to its node (so we don't have to search through list)
    public:
        LruStack(unsigned int size);
        unsigned int get_lru();
        void set_mru(unsigned int n);
        ~LruStack();
};

#endif