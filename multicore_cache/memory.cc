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

void Memory::access(addr_t physical_addr, int access_type, uint8_t* cache_block){
    uint8_t* mem_block = mem + physical_addr;
    if (access_type == MEMWRITE) {
        for (int i = 0; i < block_size; i ++) {
            mem_block[i] = cache_block[i];
        }
    } else {
        for (int i = 0; i < block_size; i ++) {
            cache_block[i] = mem_block[i];
        }
    }
}

Memory::~Memory(){
    free(mem);
}