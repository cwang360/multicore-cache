#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "system.h"

void System::init(unsigned int _num_caches, protocol_t _protocol, Cache::config_t cache_config, unsigned int mem_size, unsigned int _bus_width){
    this->protocol = _protocol;
    this->num_caches = _num_caches;
    this->bus_width = _bus_width;
    invalidations = 0;
    data_bus_transactions = 0;
    pthread_mutex_init(&bus_mutex, NULL);

    bus.message = NONE;
    bus.data = new uint8_t[cache_config.line_size];

    shared_mem = new Memory();
    shared_mem->init(mem_size, cache_config.line_size, &bus);

    caches = new Cache[num_caches];
    for (unsigned int i = 0; i < num_caches; i++) {
        caches[i].init(cache_config, protocol, &bus);
    }
}

uint8_t System::access(unsigned int core, addr_t physical_addr, access_t access_type, uint8_t data){
    pthread_mutex_lock(&bus_mutex);
    bus.addr = physical_addr;
    uint8_t result_data = caches[core].try_access(physical_addr, access_type, data);
    message_t message = bus.message;

    if (message == READ_MISS || message == WRITE_MISS) {
        bool sent_data_from_cache = false;
        // request data from other caches first.
        for (unsigned int i = 0; i < num_caches; i++) {
            if (i != core) {
                sent_data_from_cache = caches[i].system_access(physical_addr, SEND);
                if (sent_data_from_cache) {
                    shared_mem->access(physical_addr, STORE);
                    break;
                }
            }
        }
        // If none of the caches has a dirty copy, request data from memory
        if (!sent_data_from_cache) {
            shared_mem->access(physical_addr, SEND);
        }
        
        // Store data into target cache
        data_bus_transactions++;
        bus.message = NONE;
        caches[core].system_access(physical_addr, STORE);

        // If the new block evicts an old block, writeback.
        if (bus.message == WRITEBACK) {
            data_bus_transactions++;
            shared_mem->access(bus.addr, STORE);
        }
        bus.message = NONE;

        result_data = caches[core].processor_access(physical_addr, access_type, data);
    } 
    if (message == INVALIDATE || message == WRITE_MISS) {
        // invalidate others
        invalidations++;
        if (verbose) std::cout << "    INVALIDATION\n";
        for (unsigned int i = 0; i < num_caches; i++) {
            if (i != core) caches[i].invalidate(physical_addr);
        }
    }
    pthread_mutex_unlock(&bus_mutex);
    return result_data;
}

void System::print_stats(){
    for (unsigned int i = 0; i < num_caches; i++) {
        std::cout << "\n======================== Core " << i << " Cache Stats=========================\n";
        caches[i].print_stats();
    }
    std::cout << "========================== System Stats ===========================\n";
    shared_mem->print_stats();
    std::cout << "Invalidations: " << invalidations << "\n";
    std::cout << "Total data transactions through bus: "  << data_bus_transactions << "\n";
}

System::~System() {
    delete [] caches;
    delete shared_mem;
    delete [] bus.data;
    pthread_mutex_destroy(&bus_mutex);
}