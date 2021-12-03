#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <iostream>
#include <pthread.h>

#include "system.h"

System sys;
pthread_t* cpu_threads;
pthread_mutex_t simulator_mutex;

FILE* open_file(const char *filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        std::cerr << "File " << filename << " could not be opened\n";
        exit(-1);
    }
    return file;
}

/**
 * Read in next line of the trace
 * 
 * @param trace is the file handler for the trace
 * @return 0 when error or EOF and 1 otherwise. 
 */
int next_line(FILE* trace) {
    if (feof(trace) || ferror(trace)) {
        return 0;
    }
    else {
        int core;
        access_t t;
        addr_t address;
        uint8_t data;
        fscanf(trace, "%u %d %llx %" SCNx8 "\n", &core, &t, &address, &data);
        pthread_mutex_lock(&simulator_mutex);
        uint8_t accessed_data = sys.access(core, address, t, data);
        if (t == MEMWRITE) {
            printf("core%u w 0x%.6llx <= 0x%.2hhx expected: 0x%.2hhx", core, address, accessed_data, data);
        } else {
            printf("core%u r 0x%.6llx => 0x%.2hhx expected: 0x%.2hhx", core, address, accessed_data, data);
        }
        if (accessed_data != data) {
            printf(" ERROR: MISMATCH\n");
        } else {
            printf("\n");
        }
        pthread_mutex_unlock(&simulator_mutex);
    }
    return 1;
}

int init(FILE* config) {
    int num_cpus;
    protocol_t protocol;
    Cache::config_t cache_cfg1;
    cache_cfg1.cache_type = L1;
    int mem_size;
    int bus_width;
    fscanf(config, "%d, %d\n", &num_cpus, &protocol);
    fscanf(config, "%d, %d, %d, %d, %d\n", &cache_cfg1.line_size, 
            &cache_cfg1.cache_size, &cache_cfg1.associativity, &cache_cfg1.hit_time, &cache_cfg1.miss_penalty);
    fscanf(config, "%d, %d", &mem_size, &bus_width);
    sys.init(num_cpus, protocol, cache_cfg1, mem_size, bus_width);
    return num_cpus;
}

void* cpu_thread_sim(void* trace) {
    FILE* input = (FILE*) trace;
    while (next_line(input));
    fclose(input);
    pthread_exit(NULL);
}

void print_usage_and_exit() {
    std::cout   << "Usage:\n  ./simulator [-s | -p] <config file> <space-separated list of trace files>\n"
                << "   -s : Single trace file for all cores, single thread for sequential accesses to cores\n"
                << "   -p : One trace file for each core, cores access in parallel\n";
    exit(-1);
}

int main(int argc, char **argv) {
    FILE *config;
    
    if (argc < 4) {
        print_usage_and_exit();
    }
    
    config = open_file(argv[2]);
    int num_cpus = init(config);

    if (strcmp(argv[1], "-p") == 0) {
        if (argc - 3 < num_cpus) {
            std::cout << "Not enough trace files provided for parallel access.\n";
            print_usage_and_exit();
        }
        cpu_threads = (pthread_t*) malloc(sizeof(pthread_t) * num_cpus);
        pthread_mutex_init(&simulator_mutex, NULL);

        // create thread for each cpu
        for (int i = 0; i < num_cpus; i++) {
            pthread_create(&cpu_threads[i], NULL, cpu_thread_sim, (void*) open_file(argv[i + 3]));
        }

        // wait for all threads to finish
        for (int i = 0; i < num_cpus; i++) {
            pthread_join(cpu_threads[i], NULL);
        }

        sys.print_stats();

        fclose(config);
        free(cpu_threads);
        pthread_mutex_destroy(&simulator_mutex);
    } else if (strcmp(argv[1], "-s") == 0) {
        FILE* input = open_file(argv[3]);
        while (next_line(input));
        sys.print_stats();
        fclose(input);
    } else {
        print_usage_and_exit();
    }

    std::cout << "Simulation Completed\n";

    return 0;
}

