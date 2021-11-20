#ifndef __GLOBAL_TYPES_H
#define __GLOBAL_TYPES_H


// Access types
#define MEMREAD 0
#define MEMWRITE 1
#define IFETCH 2
#define MARKDIRTY 3


typedef unsigned long long addr_t;		// Data type to hold addresses
typedef unsigned long long counter_t;	// Data type to hold cache statistic variables
typedef long long data_t;

#endif