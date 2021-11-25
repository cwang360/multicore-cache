#ifndef __GLOBAL_TYPES_H
#define __GLOBAL_TYPES_H


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
    uint8_t* data;
    int source;
} bus_t;

typedef unsigned long long addr_t;		// Data type to hold addresses
typedef unsigned long long counter_t;	// Data type to hold cache statistic variables
typedef long long data_t;

#endif