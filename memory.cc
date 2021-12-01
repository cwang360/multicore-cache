#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "memory.h"

void Memory::init(int size, int block_size, bus_t* bus){
    mem = (uint8_t*) malloc(size * sizeof(uint8_t));
    this->size = size;
    this->block_size = block_size;
    this->bus = bus;
    writebacks = 0;
    data_reqs = 0;
}

void Memory::access(addr_t physical_addr, int access_type){
    uint8_t* mem_block = mem + physical_addr;
    if (access_type == STORE) {
        memcpy(mem_block, bus->data, sizeof(uint8_t) * block_size);
        writebacks++;
    } else {
        memcpy(bus->data, mem_block, sizeof(uint8_t) * block_size);
        data_reqs++;
    }
}

void Memory::print_stats() {
    printf("Writebacks: %llu\n"
            "Data requests from memory: %llu\n", 
            writebacks, data_reqs);
}

Memory::~Memory(){
    free(mem);
}