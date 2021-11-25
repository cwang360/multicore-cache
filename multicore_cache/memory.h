#ifndef __MEMORY_H
#define __MEMORY_H

#include <inttypes.h>
#include "cache.h"
#include "global_types.h"

class Memory {
    private:
        uint8_t* mem;
        int size;
        int block_size;
    public:
        void init(int size, int block_size);
        void access(addr_t physical_addr, int access_type, uint8_t* bus);
        ~Memory();
};

#endif