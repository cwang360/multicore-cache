#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"

Cache cache;

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
        int t;
        unsigned long long address, instr;
        fscanf(trace, "%d %llx %llx\n", &t, &address, &instr);
        cache.access(address, t);
    }
    return 1;
}

void init(FILE* config) {
    int line_size, cache_size, associativity, hit_time, miss_penalty;
    int line_size2, cache_size2, associativity2, hit_time2, miss_penalty2;
    fscanf(config, "%d, %d, %d, %d, %d\n", &line_size, &cache_size, &associativity, &hit_time, &miss_penalty);
    cache.init(line_size, cache_size, associativity, hit_time, miss_penalty);
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

    cache.print_stats();

    fclose(input);
    fclose(config);
    return 0;
}

