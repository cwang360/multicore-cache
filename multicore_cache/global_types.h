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



typedef unsigned long long addr_t;		// Data type to hold addresses
typedef unsigned long long counter_t;	// Data type to hold cache statistic variables
typedef long long data_t;

#endif