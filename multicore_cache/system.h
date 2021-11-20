#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "cache.h"
#include "memory.h"

// Cache coherence protocols
#define MSI 0

// Bus messages
#define READ_MISS 0
#define WRITE_MISS 1
#define INVALIDATE 2
#define DATA 3


class System {
    private:
        Cache* caches; // array of caches, one for each core
        Memory* shared_mem;
        int num_caches;
        int protocol;
    public:
        void init(int num_caches, int protocol, config_t cache_config, int mem_size);
        uint8_t access(int core, addr_t physical_addr, int access_type, data_t data);
        void print_stats();
        ~System();
};

#endif