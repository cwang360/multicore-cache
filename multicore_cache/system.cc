#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"

void System::init(int num_caches, int protocol, config_t cache_config, int mem_size){
    this->protocol = protocol;
    this->num_caches = num_caches;
    shared_mem = new Memory();
    shared_mem->init(mem_size, cache_config.line_size);
    caches = new Cache[num_caches];
    for (int i = 0; i < num_caches; i++) {
        caches[i].init(cache_config);
    }
}

uint8_t System::access(int core, addr_t physical_addr, int access_type, data_t data){
    return caches[core].access(physical_addr, access_type, data);
}

void System::print_stats(){
    for (int i = 0; i < num_caches; i++) {
        caches[i].print_stats();
    }
}

System::~System() {
    delete [] caches;
    delete shared_mem;
}