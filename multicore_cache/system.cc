#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "system.h"

void System::init(int num_caches, protocol_t protocol, config_t cache_config, int mem_size, int bus_width){
    this->protocol = protocol;
    this->num_caches = num_caches;
    this->bus_width = bus_width;

    bus.message = NONE;
    bus.data = (uint8_t*) malloc(sizeof(uint8_t) * cache_config.line_size);

    shared_mem = new Memory();
    shared_mem->init(mem_size, cache_config.line_size, &bus);

    caches = new Cache[num_caches];
    for (int i = 0; i < num_caches; i++) {
        caches[i].init(cache_config, &bus);
    }
}

uint8_t System::access(int core, addr_t physical_addr, access_t access_type, uint8_t data){
    uint8_t result_data = caches[core].try_access(physical_addr, access_type, data);
    // std::cout << result.message << "\n";
    message_t message = bus.message;
    bus.message = NONE;

    if (message == READ_MISS || message == WRITE_MISS) {
        int dirty_cache = -1;
        // check if other caches have dirty copy
        for (int i = 0; i < num_caches; i++) {
            if (i != core) {
                if (caches[i].check_dirty(physical_addr)) dirty_cache = i;
            }
        }
        if (dirty_cache < 0) {
            shared_mem->access(physical_addr, SEND);
        } else {
            caches[dirty_cache].system_access(physical_addr, SEND);
            shared_mem->access(physical_addr, STORE);
        }
        caches[core].system_access(physical_addr, STORE);
        result_data = caches[core].user_access(physical_addr, access_type, data);
        if (bus.message == WRITEBACK) {
            shared_mem->access(bus.addr, STORE);
        }
        bus.message = NONE;
    } 
    if (message == WRITE_MISS || message == INVALIDATE) {
        // invalidate others
        for (int i = 0; i < num_caches; i++) {
            if (i != core) caches[i].invalidate(physical_addr);
        }
    }

    return result_data;
}

void System::print_stats(){
    for (int i = 0; i < num_caches; i++) {
        caches[i].print_stats();
    }
}

System::~System() {
    delete [] caches;
    delete shared_mem;
    free(bus.data);
}