#ifndef __MEMORY_H
#define __MEMORY_H

#include <inttypes.h>
#include "cache.h"
#include "global_types.h"

class Memory {
    private:
        uint8_t* mem;
        unsigned int size;
        unsigned int block_size;
        bus_t* bus;
        counter_t writebacks;
        counter_t data_reqs;
    public:
        void init(unsigned int size, unsigned int block_size, bus_t* bus);
        void access(addr_t physical_addr, access_t access_type);
        void print_stats();
        ~Memory();
};

#endif