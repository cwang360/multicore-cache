#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "cache.h"

// Cache coherence protocols
#define MSI 0


class System {
    private:
        Cache* caches; // array of caches, one for each core
        int num_caches;
        int protocol;
    public:
        void init(int num_caches, int protocol, config_t cache_config);
        data_t access(int core, addr_t physical_addr, int access_type, data_t data);
        void print_stats();
        ~System();
};

#endif