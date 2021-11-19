#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"

void System::init(int num_caches, int protocol, config_t cache_config){
    this->protocol = protocol;
    this->num_caches = num_caches;
    caches = new Cache[num_caches];
    for (int i = 0; i < num_caches; i++) {
        caches[i].init(cache_config);
    }
}

data_t System::access(int core, addr_t physical_addr, int access_type, data_t data){
    return caches[core].access(physical_addr, access_type, data);
}

void System::print_stats(){
    for (int i = 0; i < num_caches; i++) {
        caches[i].print_stats();
    }
}

System::~System() {
    delete [] caches;
}