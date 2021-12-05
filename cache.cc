#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <iostream>

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
    stats.data_accesses = 0;
    stats.instr_accesses = 0;
    stats.data_misses = 0;
    stats.instr_misses = 0;
    
    stats.hit_time = config.hit_time;
    stats.miss_penalty = config.miss_penalty;

    block_size = config.line_size;
    cache_size = config.cache_size;
    ways = config.associativity;
    hit_time = config.hit_time;
    miss_penalty = config.miss_penalty;
    num_blocks = cache_size / block_size;
    num_sets = num_blocks / ways;
    num_index_bits = (unsigned int) ceil(log2(num_sets));
    num_offset_bits = (unsigned int) ceil(log2(block_size));
    cache_type = config.cache_type;

    // allocate and initialize the cache as an array of cache sets with cache blocks.
    cache = new cache_set_t[num_sets];
    for (unsigned int i = 0; i < num_sets; i++) {
        cache[i].size = ways;
        cache[i].stack = new Cache::LruStack(ways);
        cache[i].blocks = new cache_block_t[ways];
        for (unsigned int j = 0; j < ways; j++) {
            cache[i].blocks[j].tag = 0;
            cache[i].blocks[j].dirty = 0;
            cache[i].blocks[j].valid = 0;
            cache[i].blocks[j].data = new uint8_t[block_size];
            cache[i].blocks[j].state = INVALID;
        }
    }
}

Cache::~Cache() {
    for (unsigned int i = 0; i < (cache_size / block_size / ways); i++) {
        delete cache[i].stack;
        for (unsigned int j = 0; j < ways; j++) {
            delete [] cache[i].blocks[j].data;
        }
        delete [] cache[i].blocks;
    }
    delete [] cache;
}

Cache::addr_split_t Cache::split_address(addr_t physical_addr) {
    addr_split_t split = {
        physical_addr >> (num_index_bits + num_offset_bits),
        (physical_addr >> num_offset_bits) & ((1 << num_index_bits) - 1),
        physical_addr & ((1 << num_offset_bits) - 1)
    };
    return split;
}

uint8_t Cache::processor_access(addr_t physical_addr, access_t access_type, uint8_t data) {

    addr_split_t addr = split_address(physical_addr);

    // // increment accesses statistic
    // stats.accesses++;
    // if (access_type == IFETCH) stats.instr_accesses++;
    // if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_accesses++;

    uint8_t result = 0;

    for (unsigned int way = 0; way < ways; way++) {
        if (cache[addr.index].blocks[way].tag == addr.tag && cache[addr.index].blocks[way].valid) { // hit
            // stats.hits++;
            if (access_type == MEMWRITE) {
                cache[addr.index].blocks[way].dirty = 1;
                cache[addr.index].blocks[way].data[addr.offset] = data;
            } 
            // printf("access %d %d %llx\n", index, way, cache[index].blocks[way].data);
            result = cache[addr.index].blocks[way].data[addr.offset];
            // Update LRU stack
            cache[addr.index].stack->set_mru(way);
            break;
        }
    }
    return result;
}

void Cache::system_access(addr_t physical_addr, access_t access_type) {

    addr_split_t addr = split_address(physical_addr);

    if (access_type == SEND) {
        // Find cache block and copy its data to bus
        for (unsigned int way = 0; way < ways; way++) {
            if (cache[addr.index].blocks[way].tag == addr.tag && cache[addr.index].blocks[way].valid) { // hit
                cache[addr.index].blocks[way].dirty = 0; // now is shared, and memory will be updated
                // cache[index].blocks[way].state = SHARED;
                transition_bus(&cache[addr.index].blocks[way], READ_MISS);
                memcpy(bus->data, cache[addr.index].blocks[way].data, sizeof(uint8_t) * block_size);
            } 
        }
        return; // Not found; don't do anything
    }

    if (access_type == STORE) {
        // Check if there is an empty way and if so, use first one
        unsigned int accessed_way;
        bool found_empty = false;
        for (unsigned int way = 0; way < ways; way++) {
            if (!cache[addr.index].blocks[way].valid) {
                accessed_way = way;
                found_empty = true;
                break;
            }
        }
        // No empty way. Use LRU
        if (!found_empty) {
            accessed_way = cache[addr.index].stack->get_lru();
            // write back old data in block if dirty
            if (cache[addr.index].blocks[accessed_way].dirty) {
                stats.writebacks++;
            }
        }
        memcpy(cache[addr.index].blocks[accessed_way].data, bus->data, sizeof(uint8_t) * block_size);
        cache[addr.index].blocks[accessed_way].valid = 1;
        cache[addr.index].blocks[accessed_way].dirty = 0;
        cache[addr.index].blocks[accessed_way].tag = addr.tag;
    }
}

uint8_t Cache::try_access(addr_t physical_addr, access_t access_type, uint8_t data) {
    stats.accesses++;
    if (access_type == IFETCH) stats.instr_accesses++;
    if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_accesses++;

    addr_split_t addr = split_address(physical_addr);

    uint8_t result = 0;
    int hit = 0;

    // Check if valid block exists
    unsigned int empty_way;
    bool found_empty = false;
    for (unsigned int way = 0; way < ways; way++) {
        if (cache[addr.index].blocks[way].tag == addr.tag && cache[addr.index].blocks[way].valid) { // hit
            stats.hits++;
            hit = 1;
            if (access_type == MEMWRITE) {
                cache[addr.index].blocks[way].dirty = 1;
                cache[addr.index].blocks[way].data[addr.offset] = data;
            }
            transition_processor(&cache[addr.index].blocks[way], access_type);
            result = cache[addr.index].blocks[way].data[addr.offset];
            break;
        }
        if (!found_empty && !cache[addr.index].blocks[way].valid) {
            empty_way = way; // keep track of first empty way, in case we need to add a block to the cache.
            found_empty = true;
        }
    }
    if (!hit) { // miss
        stats.misses++;
        if (access_type == IFETCH) stats.instr_misses++;
        if (access_type == MEMWRITE || access_type == MEMREAD) stats.data_misses++;

        transition_processor(
            found_empty ? &cache[addr.index].blocks[empty_way] : &cache[addr.index].blocks[cache[addr.index].stack->get_lru()],
            access_type
        );
    }

    return result;
}

Cache::add_result_t Cache::add_block(addr_t physical_addr, access_t access_type) {
    addr_split_t addr = split_address(physical_addr);
    // variable for way within set where the data is accessed/stored
    unsigned int accessed_way;

    // Check if there is an empty way
    unsigned int empty_way;
    bool found_empty = false;
    for (unsigned int way = 0; way < ways; way++) {
        if (!cache[addr.index].blocks[way].valid) {
            empty_way = way; // keep track of empty way, if we find one
            found_empty = true;
            break;
        }
    }

    add_result_t result = {0, 0, 0};

    if (found_empty) {
        accessed_way = empty_way; // use empty way
    } else { // no empty way, need to replace LRU
        accessed_way = cache[addr.index].stack->get_lru();
        // send invalidate signal if evicted from L2 cache
        if (cache_type == L2) {
            result.evicted = 1;
            result.evicted_addr =   (cache[addr.index].blocks[accessed_way].tag 
                                    << (num_index_bits + num_offset_bits))
                                    | (addr.index << num_offset_bits); // address of evicted block
            if (cache[addr.index].blocks[accessed_way].dirty) {
                result.evicted_dirty = 1;
            }
        }
        if (cache_type == L1 && cache[addr.index].blocks[accessed_way].dirty) {
            // if l1 dirty cache block is evicted, update l2
            result.evicted = 1;
            result.evicted_dirty = 1;
        }
    }

    // Update metadata
    cache[addr.index].blocks[accessed_way].valid = 1;
    cache[addr.index].blocks[accessed_way].tag = addr.tag;

    if (access_type == MEMWRITE) {
        cache[addr.index].blocks[accessed_way].dirty = 1;
    } else {
        cache[addr.index].blocks[accessed_way].dirty = 0;
    }
    // Update LRU stack
    cache[addr.index].stack->set_mru(accessed_way);

    // return evicted metadata
    return result;
}

bool Cache::invalidate(addr_t evicted_addr) {
    addr_split_t addr = split_address(evicted_addr);
    
    for (unsigned int way = 0; way < ways; way++) {
        if (cache[addr.index].blocks[way].tag == addr.tag && cache[addr.index].blocks[way].valid) { // found block
            cache[addr.index].blocks[way].valid = 0;
            transition_bus(&cache[addr.index].blocks[way], INVALIDATE);
            if (cache[addr.index].blocks[way].dirty) {
                return true;
            }
            return false;
        }
    }
    return false;
}

bool Cache::check_dirty(addr_t physical_addr) {
    addr_split_t addr = split_address(physical_addr);
    
    for (unsigned int way = 0; way < ways; way++) {
        if (cache[addr.index].blocks[way].tag == addr.tag && cache[addr.index].blocks[way].valid) { // found block
            if (cache[addr.index].blocks[way].dirty) {
                return true;
            }
            return false;
        }
    }
    return false;
}

void Cache::transition_bus(cache_block_t* cache_block, message_t bus_message) {
    state_t old_state = cache_block->state;
    switch (protocol) {
        case MSI:
            switch (cache_block->state) {
                case SHARED:
                    if (bus_message == WRITE_MISS || bus_message == INVALIDATE) {
                        cache_block->state = INVALID;
                    }
                    break;
                case MODIFIED:
                    if (bus_message == INVALIDATE) {
                        cache_block->state = INVALID;
                    } else if (bus_message == WRITE_MISS) {
                        cache_block->state = INVALID;
                        memcpy(bus->data, cache_block->data, sizeof(uint8_t) * block_size);
                    } else if (bus_message == READ_MISS) {
                        cache_block->state = SHARED;
                        memcpy(bus->data, cache_block->data, sizeof(uint8_t) * block_size);
                    }
                    break;
                default:
                    break;
            }
            break;
        case MESI:
            switch (cache_block->state) {
                case EXCLUSIVE:
                    if (bus_message == READ_MISS) {
                        cache_block->state = SHARED;
                    } else if (bus_message == WRITE_MISS) {
                        cache_block->state = INVALID;
                    }
                    break;
                case SHARED:
                    if (bus_message == WRITE_MISS || bus_message == INVALIDATE) {
                        cache_block->state = INVALID;
                    }
                    break;
                case MODIFIED:
                    if (bus_message == INVALIDATE) {
                        cache_block->state = INVALID;
                    } else if (bus_message == WRITE_MISS) {
                        cache_block->state = INVALID;
                        memcpy(bus->data, cache_block->data, sizeof(uint8_t) * block_size);
                    } else if (bus_message == READ_MISS) {
                        cache_block->state = SHARED;
                        memcpy(bus->data, cache_block->data, sizeof(uint8_t) * block_size);
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    std::cout << "    Cache block " << cache_block << " state: " << old_state << " -> " << cache_block->state << "\n";
}

void Cache::transition_processor(cache_block_t* cache_block, access_t request) {
    state_t old_state = cache_block->state;
    switch (protocol) {
        case MSI:
            switch (cache_block->state) {
                case INVALID:
                    if (request == MEMREAD || request == IFETCH) {
                        cache_block->state = SHARED;
                        bus->message = READ_MISS;
                    } else if (request == MEMWRITE) {
                        cache_block->state = MODIFIED;
                        bus->message = WRITE_MISS;
                    }
                    break;
                case SHARED:
                    if (request == MEMWRITE) {
                        cache_block->state = MODIFIED;
                        bus->message = INVALIDATE;
                    }
                    break;
                default:
                    break;
            }
            break;
        case MESI:
            switch (cache_block->state) {
                case INVALID:
                    if (request == MEMWRITE) {
                        cache_block->state = MODIFIED;
                        bus->message = WRITE_MISS;
                    } else if (request == MEMREAD) {
                        bus->message = READ_MISS;
                        // see if other caches have block to determine whether to move to exclusive or shared state
                    }
                    break;
                case EXCLUSIVE:
                    if (request == MEMWRITE) {
                        cache_block->state = MODIFIED;
                    }
                    break;
                case SHARED:
                    if (request == MEMWRITE) {
                        cache_block->state = MODIFIED;
                        bus->message = INVALIDATE;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    std::cout << "    Cache block " << cache_block << " state: " << old_state << " -> " << cache_block->state << "\n";
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

Cache::LruStack::LruStack(unsigned int size) {
	this->size = size;
    
    index_map = new stack_node*[size];
    for (unsigned int i = 0; i < size; i++) {
        index_map[i] = NULL;
    }
    least_recent = NULL;
    most_recent = NULL;
}

// this should never be called before an mru is set
unsigned int Cache::LruStack::get_lru() {
    return least_recent->index;
}

void Cache::LruStack::set_mru(unsigned int n) {
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
        stack_node* temp = new stack_node;
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

Cache::LruStack::~LruStack() {
    // Free all nodes in linked list.
    stack_node* curr = most_recent;
    while (curr != NULL) {
        stack_node* temp = curr->next_less_recent;
        delete curr;
        curr = temp;
    }
    delete [] index_map;
}