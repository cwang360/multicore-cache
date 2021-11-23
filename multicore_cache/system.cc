#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

uint8_t System::access(int core, addr_t physical_addr, access_t access_type, data_t data){
    access_result_t result = caches[core].user_access(physical_addr, access_type, data);
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