#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "cache.h"

void Cache::init(int _block_size, int _cache_size, int _ways, int _hit_time, int _miss_penalty) {
    stats.hits = 0;
    stats.misses = 0;
    stats.accesses = 0;
    stats.miss_rate = 0;
    stats.traffic = 0;
    stats.writebacks = 0;
    stats.amat = 0;
    stats.data_accesses = 0;
    stats.data_misses = 0;
    stats.instr_accesses = 0;
    stats.instr_misses = 0;

    block_size = _block_size;
    cache_size = _cache_size;
    ways = _ways;
    hit_time = _hit_time;
    miss_penalty = _miss_penalty;
    num_blocks = cache_size / block_size;
    num_sets = num_blocks / ways;
    num_index_bits = (int) ceil(log2(num_sets));
    num_offset_bits = (int) ceil(log2(block_size));

    // allocate and initialize the cache as an array of cache sets with cache blocks.
    cache = (cache_set_t*) malloc(num_sets * sizeof(cache_set_t));
    if (cache == NULL) {
        printf("Could not allocate memory for cache\n");
        exit(1);
    }
    for (int i = 0; i < num_sets; i++) {
        cache[i].size = ways;
        cache[i].stack = new LruStack(ways);
        cache[i].blocks = (cache_block_t*) malloc(ways * sizeof(cache_block_t));
        if (cache[i].blocks == NULL) {
            printf("Could not allocate memory for cache blocks\n");
            exit(1);
        }
        for (int j = 0; j < ways; j++) {
            cache[i].blocks[j].tag = 0;
            cache[i].blocks[j].dirty = 0;
            cache[i].blocks[j].valid = 0;
        }
    }
}

Cache::~Cache() {
    for (int i = 0; i < (cache_size / block_size / ways); i++) {
        delete cache[i].stack;
        free(cache[i].blocks);
    }
    free(cache);
}

void Cache::access(addr_t physical_addr, int access_type) {

    // Use bit manipulation to extract tag, index, offset from physical_addr
    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    // int offset = physical_addr & ((1 << num_offset_bits) - 1);

    // increment accesses statistic
    stats.accesses++;
    if (access_type == IFETCH) stats.instr_accesses++;
    if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_accesses++;

    // variable for way within set where the data is accessed/stored
    int accessed_way = -1;

    // Check if tag exists in set
    int empty_way = -1;
    for (int way = 0; way < ways; way++) {
        if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // hit
            stats.hits++;
            accessed_way = way;
            if (access_type == MEMWRITE) {
                cache[index].blocks[way].dirty = 1;
            }
            break;
        }
        if (!cache[index].blocks[way].valid) {
            empty_way = way; // keep track of first empty way, in case we need to add a block to the cache.
        }
    }

    if (accessed_way == -1) { // miss
        stats.misses++;
        if (access_type == IFETCH) stats.instr_misses++;
        if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_misses++;
        if (empty_way > -1) {
            accessed_way = empty_way; // use empty way
        } else {
            accessed_way = cache[index].stack->get_lru(); // need to replace a block
            // write back data being replaced if it is dirty
            if (cache[index].blocks[accessed_way].dirty) {
                stats.writebacks++;
            }
        }
        // Update metadata
        cache[index].blocks[accessed_way].valid = 1;
        cache[index].blocks[accessed_way].tag = tag;
        if (access_type == MEMWRITE) {
            cache[index].blocks[accessed_way].dirty = 1;
        } else {
            cache[index].blocks[accessed_way].dirty = 0;
        }
    }

    // Update LRU stack
    cache[index].stack->set_mru(accessed_way);
}

void Cache::print_stats() {
    get_stats();
    printf( "===================== Single Level Cache Stats =====================\n"
            "%d-byte %d-way set associative cache with %d-byte lines\n"
            "Hit time: %d cycles\n"
            "Miss penalty: %d cycles\n"
            "--------------------------------------------------------------------\n"
            "Accesses: %llu\n"
            "Hits: %llu\n"
            "Misses: %llu\n" 
            "Miss Rate: %f%%\n"
            "    Instruction miss rate: %f%%\n"
            "    Data miss rate: %f%%\n"
            "AMAT: %f cycles\n"
            "Writebacks: %llu\n\n",  
            cache_size, ways, block_size, 
            hit_time, 
            miss_penalty,

            stats.accesses, 
            stats.hits, 
            stats.misses, 
            stats.miss_rate*100, 
            (1.0 * stats.instr_misses) / stats.instr_accesses * 100,
            (1.0 * stats.data_misses) / stats.data_accesses * 100,
            stats.amat,
            stats.writebacks);  
}

stats_t* Cache::get_stats() {
    stats.miss_rate = (1.0 * stats.misses) / stats.accesses;
    stats.amat = hit_time + (stats.miss_rate * miss_penalty);
    return &stats;
}