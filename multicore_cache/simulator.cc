#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"

System sys;

/**
 * Function to open the trace file
 * You do not need to update this function. 
 */
FILE *open_file(const char *filename) {
    return fopen(filename, "r");
}

/**
 * Read in next line of the trace
 * 
 * @param trace is the file handler for the trace
 * @return 0 when error or EOF and 1 otherwise. 
 */
int next_line(FILE* trace) {
    if (feof(trace) || ferror(trace)) return 0;
    else {
        int core;
        int t;
        addr_t address;
        data_t data;
        fscanf(trace, "%u %d %llx %llx\n", &core, &t, &address, &data);
        data_t accessed_data = sys.access(core, address, t, data);
        if (t == MEMWRITE) {
            printf("w 0x%.6llx <= 0x%.6llx expected: 0x%.6llx\n", address, accessed_data, data);
        } else if (t == MEMREAD) {
            printf("r 0x%.6llx => 0x%.6llx expected: 0x%.6llx\n", address, accessed_data, data);
        }
    }
    return 1;
}

void init(FILE* config) {
    int num_caches;
    int protocol;
    config_t cache_cfg1;
    cache_cfg1.cache_type = L1;
    fscanf(config, "%d, %d\n", &num_caches, &protocol);
    fscanf(config, "%d, %d, %d, %d, %d\n", &cache_cfg1.line_size, 
            &cache_cfg1.cache_size, &cache_cfg1.associativity, &cache_cfg1.hit_time, &cache_cfg1.miss_penalty);
    sys.init(num_caches, protocol, cache_cfg1);
}

/**
 * Main function. See error message for usage. 
 * 
 * @param argc number of arguments
 * @param argv Argument values
 * @returns 0 on success. 
 */
int main(int argc, char **argv) {
    FILE *input;
    FILE *config;
    
    if (argc != 3) {
        fprintf(stderr, "Usage:\n  %s <config> <trace>\n", argv[0]);
        return 1;
    }
    
    input = open_file(argv[2]);
    config = open_file(argv[1]);

    init(config);

    while (next_line(input));

    sys.print_stats();

    fclose(input);
    fclose(config);
    return 0;
}

