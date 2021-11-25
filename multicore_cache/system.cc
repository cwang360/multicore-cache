#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "system.h"

void System::init(int num_caches, protocol_t protocol, config_t cache_config, int mem_size, int bus_width){
    this->protocol = protocol;
    this->num_caches = num_caches;
    this->bus_width = bus_width;
    bus = (uint8_t*) malloc(sizeof(uint8_t) * cache_config.line_size);
    shared_mem = new Memory();
    shared_mem->init(mem_size, cache_config.line_size);
    caches = new Cache[num_caches];
    for (int i = 0; i < num_caches; i++) {
        caches[i].init(cache_config);
    }
}

uint8_t System::access(int core, addr_t physical_addr, access_t access_type, uint8_t data){
    access_result_t result = caches[core].try_access(physical_addr, access_type, data);
    // std::cout << result.message << "\n";

    bus_message_t message = result.message;

    if (result.message == INVALIDATE) {
        // invalidate copies of the data in other caches
        for (int i = 0; i < num_caches; i++) {
            if (i != core) caches[i].invalidate(physical_addr);
        }
    }

    if (result.hit) return result.data;

    if (message == READ_MISS || message == WRITE_MISS) {
        int dirty_cache = -1;
        // check if other caches have dirty copy
        for (int i = 0; i < num_caches; i++) {
            if (i != core) {
                if (caches[i].check_dirty(physical_addr)) dirty_cache = i;
            }
        }
        if (dirty_cache < 0) {
            shared_mem->access(physical_addr, SEND, bus);
        } else {
            caches[dirty_cache].system_access(physical_addr, SEND, bus);
            shared_mem->access(physical_addr, STORE, bus);
        }
        caches[core].system_access(physical_addr, STORE, bus);
        result = caches[core].user_access(physical_addr, access_type, data);
        if (result.message == WRITEBACK) {
            // handle writeback
        }
    } 
    if (message == WRITE_MISS) {
        // invalidate others
        for (int i = 0; i < num_caches; i++) {
            if (i != core) caches[i].invalidate(physical_addr);
        }
    }

    return result.data;
}

void System::print_stats(){
    for (int i = 0; i < num_caches; i++) {
        caches[i].print_stats();
    }
}

System::~System() {
    delete [] caches;
    delete shared_mem;
    free(bus);
}