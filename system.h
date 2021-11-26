#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "cache.h"
#include "memory.h"

// Cache coherence protocols
typedef enum {
    MSI = 0, 
    MESI = 1
} protocol_t;


class System {
    private:
        Cache* caches;      // Array of caches, one for each core
        Memory* shared_mem; // Main memory, shared by all cores
        bus_t bus;
        int bus_width;      // Width of system bus; amount of data that can be transferred per usage of bus
        int num_caches;     // Number of caches/cores
        int protocol;       // Cache coherence protocol. See the definition for protocol_t.
        pthread_mutex_t bus_mutex;
    public:
        void init(int num_caches, protocol_t protocol, config_t cache_config, int mem_size, int bus_width);
        uint8_t access(int core, addr_t physical_addr, access_t access_type, uint8_t data);
        void print_stats();
        ~System();
};

#endif