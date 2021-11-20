#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "cache.h"

void Cache::init(config_t config) {
    printf("init cache\n");
    stats.hits = 0;
    stats.misses = 0;
    stats.accesses = 0;
    stats.miss_rate = 0;
    stats.writebacks = 0;
    stats.amat = 0;
    stats.traffic = 0;
    
    stats.hit_time = config.hit_time;
    stats.miss_penalty = config.miss_penalty;

    block_size = config.line_size;
    cache_size = config.cache_size;
    ways = config.associativity;
    hit_time = config.hit_time;
    miss_penalty = config.miss_penalty;
    num_blocks = cache_size / block_size;
    num_sets = num_blocks / ways;
    num_index_bits = (int) ceil(log2(num_sets));
    num_offset_bits = (int) ceil(log2(block_size));
    cache_type = config.cache_type;

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
            cache[i].blocks[j].data = (int8_t*) malloc(block_size * sizeof(int8_t));
        }
    }
}

Cache::~Cache() {
    for (int i = 0; i < (cache_size / block_size / ways); i++) {
        delete cache[i].stack;
        for (int j = 0; j < ways; j++) {
            free(cache[i].blocks[j].data);
        }
        free(cache[i].blocks);
    }
    free(cache);
}

int8_t Cache::access(addr_t physical_addr, int access_type, int8_t data) {

    // Use bit manipulation to extract tag, index, offset from physical_addr
    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    int offset = physical_addr & ((1 << num_offset_bits) - 1);

    // increment accesses statistic
    stats.accesses++;
    if (access_type == IFETCH) stats.instr_accesses++;
    if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_accesses++;

    // variable for way within set where the data is accessed/stored
    int accessed_way = -1;
    int8_t cache_data;

    // Check if tag exists in set
    int empty_way = -1;
    for (int way = 0; way < ways; way++) {
        if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // hit
            stats.hits++;
            accessed_way = way;
            if (access_type == MEMWRITE) {
                cache[index].blocks[way].dirty = 1;
                cache[index].blocks[way].data[offset] = data;
            } 
            // printf("access %d %d %llx\n", index, way, cache[index].blocks[way].data);
            cache_data = cache[index].blocks[way].data[offset];
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
        for (int i = 0; i < block_size; i++) {
            cache[index].blocks[accessed_way].data[i] = 0;
        }
        if (access_type == MEMWRITE) {
            cache[index].blocks[accessed_way].dirty = 1;
            cache[index].blocks[accessed_way].data[offset] = data;
        } else {
            cache[index].blocks[accessed_way].dirty = 0;
        }        
        cache_data = cache[index].blocks[accessed_way].data[offset];
        // printf("access %d %d %llx\n", index, accessed_way, cache[index].blocks[accessed_way].data);

    }

    // Update LRU stack
    cache[index].stack->set_mru(accessed_way);
    return cache_data;
}

int Cache::try_access(addr_t physical_addr, int access_type) {
    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    // variable for way within set where the data is accessed/stored
    int hit = 0;

    // Check if tag exists in set
    int empty_way = -1;
    for (int way = 0; way < ways; way++) {
        if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // hit
            hit = 1;
            if (access_type == MEMWRITE || access_type == MARKDIRTY) {
                cache[index].blocks[way].dirty = 1;
            }
            // This prevents the MRU of L2 cache from being updated when a dirty block of L1 is evicted 
            // and that block is marked dirty in L2
            if (access_type != MARKDIRTY) {
                cache[index].stack->set_mru(way);
                stats.hits++;
            }
            break;
        }
    }
    if (!hit) {
        stats.misses++;
    }
    // returns 1 if hit, otherwise 0
    return hit;
}

add_result_t Cache::add_block(addr_t physical_addr, int access_type) {
        // Use bit manipulation to extract tag, index, offset from physical_addr
    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    // variable for way within set where the data is accessed/stored
    int accessed_way = -1;

    // Check if there is an empty way
    int empty_way = -1;
    for (int way = 0; way < ways; way++) {
        if (!cache[index].blocks[way].valid) {
            empty_way = way; // keep track of empty way, if we find one
        }
    }

    add_result_t result = {0, 0, 0};

    if (empty_way > -1) {
        accessed_way = empty_way; // use empty way
    } else { // no empty way, need to replace LRU
        accessed_way = cache[index].stack->get_lru();
        // send invalidate signal if evicted from L2 cache
        if (cache_type == L2) {
            result.evicted = 1;
            result.evicted_addr =   (cache[index].blocks[accessed_way].tag 
                                    << (num_index_bits + num_offset_bits))
                                    | (index << num_offset_bits); // address of evicted block
            if (cache[index].blocks[accessed_way].dirty) {
                result.evicted_dirty = 1;
            }
        }
        if (cache_type == L1 && cache[index].blocks[accessed_way].dirty) {
            // if l1 dirty cache block is evicted, update l2
            result.evicted = 1;
            result.evicted_dirty = 1;
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
    // Update LRU stack
    cache[index].stack->set_mru(accessed_way);

    // return evicted metadata
    return result;
}

int Cache::invalidate(addr_t evicted_addr) {
    int tag = evicted_addr >> (num_index_bits + num_offset_bits);
    int index = (evicted_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    
    for (int way = 0; way < ways; way++) {
        if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // found block
            cache[index].blocks[way].valid = 0;
            if (cache[index].blocks[way].dirty) {
                return 1;
            }
            return 0;
        }
    }
    return 0;
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

void MultilevelCache::init(config_t l1_config, config_t l2_config) {
    l1_config.cache_size /= 2;
    l1_data_cache = new Cache();
    l1_instr_cache = new Cache();
    l2_cache = new Cache();

    l1_data_cache->init(l1_config);
    l1_instr_cache->init(l1_config);
    l2_cache->init(l2_config);
    
    global_stats.hits = 0;
    global_stats.misses = 0;
    global_stats.accesses = 0;
    global_stats.miss_rate = 0;
    global_stats.traffic = 0;
    global_stats.writebacks = 0;
    global_stats.amat = 0;
    global_stats.data_accesses = 0;
    global_stats.data_misses = 0;
    global_stats.instr_accesses = 0;
    global_stats.instr_misses = 0;
}

void MultilevelCache::access(addr_t physical_addr, int access_type) {
    // determine which l1 cache to look in/modify
    Cache* l1_cache = (access_type == IFETCH ? l1_instr_cache : l1_data_cache);

    // increment accesses statistic
    global_stats.accesses++;
    if (access_type == IFETCH) global_stats.instr_accesses++;
    if (access_type == MEMWRITE || access_type == MEMREAD) global_stats.data_accesses++;

    int l1_hit = l1_cache->try_access(physical_addr, access_type);
    if (l1_hit) {
        global_stats.hits++;
    } else {
        // l1 miss, so try l2
        int l2_hit = l2_cache->try_access(physical_addr, MEMREAD);
        if (l2_hit) { // if found in l2, bring to l1
            global_stats.hits++;
            l1_cache->get_stats()->traffic++;
            add_result_t result = l1_cache->add_block(physical_addr, access_type);
            if (result.evicted_dirty) {
                l1_cache->get_stats()->traffic++;
                l2_cache->try_access(physical_addr, MARKDIRTY);
            }
        } else {
            global_stats.misses++;
            if (access_type == IFETCH) global_stats.instr_misses++;
            if (access_type == MEMWRITE || access_type == MEMREAD) global_stats.data_misses++;
            // l1 and l2 miss, so need to put data in both l1 and l2
            add_result_t result = l2_cache->add_block(physical_addr, MEMREAD); // in case of write, only l1 cache should be written
            if (result.evicted) { // block was evicted from L2
                l1_cache->get_stats()->traffic++;
                int invalidated_dirty_in_l1 = l1_cache->invalidate(result.evicted_addr);
                // write back occurs if:
                if (result.evicted_dirty || invalidated_dirty_in_l1) {
                    global_stats.writebacks++;
                }
            }
            result = l1_cache->add_block(physical_addr, access_type);
            if (result.evicted_dirty) {
                l1_cache->get_stats()->traffic++;
                l2_cache->try_access(physical_addr, MARKDIRTY);
            }
        }
    }
}

void MultilevelCache::print_stats() {
    get_stats();
    printf( "======================= Multilevel Cache Stats =======================\n"
            // "L1: %d-byte %d-way set associative I-cache with %d-byte lines\n"
            // "    %d-byte %d-way set associative D-cache with %d-byte lines\n"
            // "    Hit time: %d cycles\n"
            // "L2: %d-byte %d-way set associative unified cache with %d-byte lines\n"
            // "    Hit time: %d cycles\n"
            // "    Miss penalty: %d cycles\n"
            "----------------------------------------------------------------------\n"
            "Accesses: %llu\n"
            "Global hits: %llu\n"
            "Global misses: %llu\n"
            "Global miss rate: %f%%\n"
            "    Global instruction miss rate: %f%%\n"
            "    Global data miss rate: %f%%\n"
            "AMAT: %f cycles \n"
            "Writebacks: %llu\n\n"
            "L1 I-cache hits: %llu\n"
            "L1 I-cache misses: %llu\n"
            "L1 I-cache miss rate: %f%%\n\n"
            "L1 D-cache hits: %llu\n"
            "L1 D-cache misses: %llu\n"
            "L1 D-cache miss rate: %f%%\n\n"
            "L2 unified cache hits: %llu\n"
            "L2 unified cache misses: %llu\n"
            "L2 unified cache miss rate: %f%%\n\n"
            "Traffic between L1 cache and L2 cache: %llu\n"
            "    Traffic between L1 D-cache and L2 cache: %llu\n"
            "    Traffic between L1 I-cache and L2 cache: %llu\n\n",
            // l1_instr_cache->get_stats()->cache_size, l1_instr_cache->get_stats()->ways, l1_instr_cache->get_stats()->block_size,
            // l1_data_cache->get_stats()->cache_size, l1_data_cache->get_stats()->ways, l1_data_cache->get_stats()->block_size,
            // l1_hit_time,
            // l2_cache->get_stats()->cache_size, l2_cache->get_stats()->ways, l2_cache->get_stats()->block_size,
            // l2_hit_time,
            // l2_miss_penalty,

            global_stats.accesses, 
            global_stats.hits, 
            global_stats.misses, 
            global_stats.miss_rate*100, 
            (1.0 * global_stats.instr_misses) / global_stats.instr_accesses * 100,
            (1.0 * global_stats.data_misses) / global_stats.data_accesses * 100,
            global_stats.amat,
            global_stats.writebacks, 

            l1_instr_cache->get_stats()->hits, 
            l1_instr_cache->get_stats()->misses, 
            (1.0 * l1_instr_cache->get_stats()->misses)/(l1_instr_cache->get_stats()->misses + l1_instr_cache->get_stats()->hits)*100, 

            l1_data_cache->get_stats()->hits, 
            l1_data_cache->get_stats()->misses, 
            (1.0 * l1_data_cache->get_stats()->misses)/(l1_data_cache->get_stats()->misses + l1_data_cache->get_stats()->hits)*100,

            l2_cache->get_stats()->hits, 
            l2_cache->get_stats()->misses, 
            (1.0 * l2_cache->get_stats()->misses)/(l2_cache->get_stats()->misses + l2_cache->get_stats()->hits)*100,

            global_stats.traffic,
            l1_data_cache->get_stats()->traffic, 
            l1_instr_cache->get_stats()->traffic);  
}

stats_t* MultilevelCache::get_stats() {
    global_stats.miss_rate = (1.0 * global_stats.misses)/global_stats.accesses;
    global_stats.traffic = l1_instr_cache->get_stats()->traffic + l1_data_cache->get_stats()->traffic;
    double l1_miss_rate = (1.0 * l1_data_cache->get_stats()->misses + l1_instr_cache->get_stats()->misses)/ // total L1 misses
                        (l1_data_cache->get_stats()->misses + l1_instr_cache->get_stats()->misses + l1_data_cache->get_stats()->hits + l1_instr_cache->get_stats()->hits); // total L1 accesses
    double l2_miss_rate = (1.0 * l2_cache->get_stats()->misses)/(l2_cache->get_stats()->misses + l2_cache->get_stats()->hits);
    double l1_miss_penalty = l2_cache->get_stats()->hit_time + l2_miss_rate * l2_cache->get_stats()->miss_penalty;
    global_stats.amat = l1_data_cache->get_stats()->hit_time + l1_miss_rate * l1_miss_penalty;
    return &global_stats;
}

MultilevelCache::~MultilevelCache() {
    delete l1_data_cache;
    delete l1_instr_cache;
    delete l2_cache;
}