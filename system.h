#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "cache.h"
#include "memory.h"


class System {
    private:
        Cache* caches;      // Array of caches, one for each core
        Memory* shared_mem; // Main memory, shared by all cores
        bus_t bus;
        unsigned int bus_width;      // Width of system bus; amount of data that can be transferred per usage of bus
        unsigned int num_caches;     // Number of caches/cores
        protocol_t protocol;       // Cache coherence protocol. See the definition for protocol_t.
        pthread_mutex_t bus_mutex;
        counter_t invalidations;
        counter_t data_bus_transactions;
    public:
        void init(unsigned int _num_caches, protocol_t _protocol, Cache::config_t cache_config, unsigned int mem_size, unsigned int _bus_width);
        uint8_t access(unsigned int core, addr_t physical_addr, access_t access_type, uint8_t data);
        void print_stats();
        ~System();
};

#endif