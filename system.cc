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
    bus.data = (uint8_t*) malloc(sizeof(uint8_t) * cache_config.line_size);
    if (!bus.data) {
        std::cout << "Could not allocate memory for bus data\n";
        exit(-1);
    }

    shared_mem = new Memory();
    shared_mem->init(mem_size, cache_config.line_size, &bus);

    caches = new Cache[num_caches];
    for (unsigned int i = 0; i < num_caches; i++) {
        caches[i].init(cache_config, protocol, &bus);
    }
}

uint8_t System::access(unsigned int core, addr_t physical_addr, access_t access_type, uint8_t data){
    pthread_mutex_lock(&bus_mutex);
    uint8_t result_data = caches[core].try_access(physical_addr, access_type, data);
    message_t message = bus.message;
    bus.message = NONE;

    if (message == READ_MISS || message == WRITE_MISS) {
        unsigned int dirty_cache;
        bool dirty = false;
        // check if other caches have dirty copy
        for (unsigned int i = 0; i < num_caches; i++) {
            if (i != core) {
                if (caches[i].check_dirty(physical_addr)) {
                    dirty_cache = i;
                    dirty = true;
                }
            }
        }
        if (!dirty) {
            shared_mem->access(physical_addr, SEND);
        } else {
            caches[dirty_cache].system_access(physical_addr, SEND);
            shared_mem->access(physical_addr, STORE);
        }
        data_bus_transactions++;
        caches[core].system_access(physical_addr, STORE);
        result_data = caches[core].processor_access(physical_addr, access_type, data);
        if (bus.message == WRITEBACK) {
            data_bus_transactions++;
            shared_mem->access(bus.addr, STORE);
        }
        bus.message = NONE;
    } 
    if (message == WRITE_MISS || message == INVALIDATE) {
        // std::cout << "INVALIDATION\n";
        // invalidate others
        invalidations++;
        for (unsigned int i = 0; i < num_caches; i++) {
            if (i != core) caches[i].invalidate(physical_addr);
        }
    }
    pthread_mutex_unlock(&bus_mutex);
    return result_data;
}

void System::print_stats(){
    for (unsigned int i = 0; i < num_caches; i++) {
        caches[i].print_stats();
    }
    std::cout << "======================== System Stats =========================\n";
    shared_mem->print_stats();
    std::cout << "Invalidations: " << invalidations << "\n";
    std::cout << "Total data transactions through bus: "  << data_bus_transactions << "\n";
}

System::~System() {
    delete [] caches;
    delete shared_mem;
    free(bus.data);
    pthread_mutex_destroy(&bus_mutex);
}