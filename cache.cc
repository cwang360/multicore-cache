#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "cache.h"

void Cache::init(config_t config, protocol_t protocol, bus_t* bus) {
    this->bus = bus;
    this->protocol = protocol;

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
        exit(-1);
    }
    for (int i = 0; i < num_sets; i++) {
        cache[i].size = ways;
        cache[i].stack = new LruStack(ways);
        cache[i].blocks = (cache_block_t*) malloc(ways * sizeof(cache_block_t));
        if (cache[i].blocks == NULL) {
            printf("Could not allocate memory for cache blocks\n");
            exit(-1);
        }
        for (int j = 0; j < ways; j++) {
            cache[i].blocks[j].tag = 0;
            cache[i].blocks[j].dirty = 0;
            cache[i].blocks[j].valid = 0;
            cache[i].blocks[j].data = (uint8_t*) malloc(block_size * sizeof(uint8_t));
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

uint8_t Cache::user_access(addr_t physical_addr, access_t access_type, uint8_t data) {

    // Use bit manipulation to extract tag, index, offset from physical_addr
    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    int offset = physical_addr & ((1 << num_offset_bits) - 1);

    // // increment accesses statistic
    // stats.accesses++;
    // if (access_type == IFETCH) stats.instr_accesses++;
    // if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_accesses++;

    // variable for way within set where the data is accessed/stored
    int accessed_way = -1;

    uint8_t result;
    int hit = 0;

    // Check if tag exists in set
    int empty_way = -1;
    for (int way = 0; way < ways; way++) {
        if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // hit
            hit = 1;
            // stats.hits++;
            accessed_way = way;
            if (access_type == MEMWRITE) {
                cache[index].blocks[way].dirty = 1;
                cache[index].blocks[way].data[offset] = data;
            } 
            // printf("access %d %d %llx\n", index, way, cache[index].blocks[way].data);
            result = cache[index].blocks[way].data[offset];
            break;
        }
        if (!cache[index].blocks[way].valid) {
            empty_way = way; // keep track of first empty way, in case we need to add a block to the cache.
        }
    }

    if (accessed_way == -1) { // miss
        // stats.misses++;
        // if (access_type == IFETCH) stats.instr_misses++;
        // if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_misses++;
        if (empty_way > -1) {
            accessed_way = empty_way; // use empty way
        } else {
            accessed_way = cache[index].stack->get_lru(); // need to replace a block
            // write back data being replaced if it is dirty
            if (cache[index].blocks[accessed_way].dirty) {
                stats.writebacks++;
                bus->message = WRITEBACK;
                bus->addr = (cache[index].blocks[accessed_way].tag 
                            << (num_index_bits + num_offset_bits))
                            | (index << num_offset_bits); // address of evicted block
                memcpy(bus->data, cache[index].blocks[accessed_way].data, sizeof(uint8_t) * block_size);
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
        result = cache[index].blocks[accessed_way].data[offset];
        // printf("access %d %d %llx\n", index, accessed_way, cache[index].blocks[accessed_way].data);

    }

    // Update LRU stack
    cache[index].stack->set_mru(accessed_way);

    return result;
}

void Cache::system_access(addr_t physical_addr, access_t access_type) {

    // Use bit manipulation to extract tag, index, offset from physical_addr
    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);

    if (access_type == SEND) {
        // Find cache block and copy its data to bus
        for (int way = 0; way < ways; way++) {
            if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // hit
                cache[index].blocks[way].dirty = 0; // now is shared, and memory will be updated
                memcpy(bus->data, cache[index].blocks[way].data, sizeof(uint8_t) * block_size);
            } 
        }
        return; // Not found; don't do anything
    }

    if (access_type == STORE) {
        // Check if there is an empty way and if so, use first one
        int accessed_way = -1;
        for (int way = 0; way < ways; way++) {
            if (!cache[index].blocks[way].valid) {
                accessed_way = way;
                break;
            }
        }
        // No empty way. Use LRU
        if (accessed_way < 0) {
            accessed_way = cache[index].stack->get_lru();
            // write back old data in block if dirty
            if (cache[index].blocks[accessed_way].dirty) {
                stats.writebacks++;
            }
        }
        memcpy(cache[index].blocks[accessed_way].data, bus->data, sizeof(uint8_t) * block_size);
        cache[index].blocks[accessed_way].valid = 1;
        cache[index].blocks[accessed_way].dirty = 0;
        cache[index].blocks[accessed_way].tag = tag;
        // // Update LRU stack?
        // cache[index].stack->set_mru(accessed_way);
    }
}

uint8_t Cache::try_access(addr_t physical_addr, access_t access_type, uint8_t data) {
    stats.accesses++;
    if (access_type == IFETCH) stats.instr_accesses++;
    if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_accesses++;

    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    int offset = physical_addr & ((1 << num_offset_bits) - 1);

    uint8_t result;
    int hit = 0;

    // Check if tag exists in set
    int empty_way = -1;
    for (int way = 0; way < ways; way++) {
        if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // hit
            stats.hits++;
            hit = 1;
            if (access_type == MEMWRITE) {
                if (!cache[index].blocks[way].dirty) {
                    bus->message = INVALIDATE;
                }
                cache[index].blocks[way].dirty = 1;
                cache[index].blocks[way].data[offset] = data;
            }
            result = cache[index].blocks[way].data[offset];
            break;
        }
    }
    if (!hit) { // miss
        stats.misses++;
        if (access_type == IFETCH) stats.instr_misses++;
        if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_misses++;

        if (access_type == MEMWRITE) {
            bus->message = WRITE_MISS;
        } else if (access_type == MEMREAD || access_type == IFETCH) {
            bus->message = READ_MISS; // MSI
        }
    }

    return result;
}

add_result_t Cache::add_block(addr_t physical_addr, access_t access_type) {
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

int Cache::check_dirty(addr_t physical_addr) {
    int tag = physical_addr >> (num_index_bits + num_offset_bits);
    int index = (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1);
    
    for (int way = 0; way < ways; way++) {
        if (cache[index].blocks[way].tag == tag && cache[index].blocks[way].valid) { // found block
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

LruStack::LruStack(int size) {
	size = size;
    
    index_map = (stack_node**) malloc(size * sizeof(stack_node*));
    if (index_map == NULL) {
        printf("Could not allocate memory for LRU stack index map\n");
        exit(1);
    }
    for (int i = 0; i < size; i++) {
        index_map[i] = NULL;
    }
    least_recent = NULL;
    most_recent = NULL;
}

int LruStack::get_lru() {
    return least_recent->index;
}

void LruStack::set_mru(int n) {
    // If already most recent, no need to do anything else.
    if (most_recent != NULL && most_recent->index == n) {
        return;
    }
    // get node corresponding to index
    stack_node* curr = index_map[n];

    // If not NULL, curr must be node with matching address. Update pointers
    // to move curr to most recent spot.
    if (curr != NULL) {
        curr->next_more_recent->next_less_recent = curr->next_less_recent;
        if (curr->next_less_recent != NULL) {
            curr->next_less_recent->next_more_recent = curr->next_more_recent;
        } else {
            // curr is at end (least recent) spot. Update least recent pointer
            // to node before curr.
            least_recent = curr->next_more_recent;
        }
        curr->next_more_recent = NULL;
        curr->next_less_recent = most_recent;
        most_recent->next_more_recent = curr;
        most_recent = curr;
    } else {
        // Node with index n does not exist, so make one and add it to the
        // most recent spot in the linked list.
        stack_node* temp = (stack_node*) malloc(sizeof(stack_node));
        if (temp == NULL) {
            printf("Could not allocate memory for LRU stack entry\n");
            exit(1);
        }
        temp->next_more_recent = NULL;
        temp->next_less_recent = most_recent;
        temp->index = n;
        if (most_recent != NULL) {
            most_recent->next_more_recent = temp;
        } else {
            least_recent = temp;
        }
        most_recent = temp;
        index_map[n] = temp;
    }
}

LruStack::~LruStack() {
    // Free all nodes in linked list.
    stack_node* curr = most_recent;
    while (curr != NULL) {
        stack_node* temp = curr->next_less_recent;
        free(curr);
        curr = temp;
    }
    free(index_map);
}