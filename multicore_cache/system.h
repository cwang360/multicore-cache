#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "cache.h"
#include "memory.h"

// Cache coherence protocols
typedef enum {
    MSI = 0
} protocol_t;

// Bus messages
typedef enum {
    READ_MISS,
    WRITE_MISS,
    INVALIDATE,
    DATA
} bus_message_t;



class System {
    private:
        Cache* caches;      // Array of caches, one for each core
        Memory* shared_mem; // Main memory, shared by all cores
        uint8_t* bus;       // Pointer to beginning of space allocated to hold data on the system bus
        int bus_width;      // Width of system bus; amount of data that can be transferred per usage of bus
        int num_caches;     // Number of caches/cores
        int protocol;       // Cache coherence protocol. See the definition for protocol_t.
    public:
        void init(int num_caches, protocol_t protocol, config_t cache_config, int mem_size, int bus_width);
        uint8_t access(int core, addr_t physical_addr, access_t access_type, data_t data);
        void print_stats();
        ~System();
};

#endif