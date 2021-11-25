#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "memory.h"

void Memory::init(int size, int block_size){
    mem = (uint8_t*) malloc(size * sizeof(uint8_t));
    this->size = size;
    this->block_size = block_size;
}

void Memory::access(addr_t physical_addr, int access_type, uint8_t* bus){
    uint8_t* mem_block = mem + physical_addr;
    if (access_type == STORE) {
        memcpy(mem_block, bus, sizeof(uint8_t) * block_size);
    } else {
        memcpy(bus, mem_block, sizeof(uint8_t) * block_size);
    }
}

Memory::~Memory(){
    free(mem);
}