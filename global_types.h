#ifndef __GLOBAL_TYPES_H
#define __GLOBAL_TYPES_H

typedef unsigned long long addr_t;		// Data type to hold addresses
typedef unsigned long long counter_t;	// Data type to hold cache statistic variables
typedef long long data_t;

// Access types
typedef enum {
    MEMREAD = 0,
    MEMWRITE = 1,
    IFETCH = 2,
    SEND,       // Data copied to bus
    STORE,      // Data copied from bus, stored to mem or cache
    MARKDIRTY
} access_t;

// Bus messages
typedef enum {
    NONE = 0, 
    READ_MISS = 1,
    WRITE_MISS = 2,
    INVALIDATE = 3,
    DATA = 4, 
    WRITEBACK = 5
} message_t;

// Bus struct
typedef struct bus_t {
    message_t message;
    addr_t addr; // address for a DATA request, or for WRITEBACK
    uint8_t* data;
    int source;
} bus_t;

// Cache coherence protocols
typedef enum {
    MSI = 0, 
    MESI = 1
} protocol_t;

#endif