#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <string.h>
#include <inttypes.h>
#include <iostream>
#include <pthread.h>

#include "global_types.h"
#include "system.h"

using namespace std;

System sys;
pthread_t* cpu_threads;
pthread_mutex_t simulator_mutex;
bool verbose;

FILE* open_file(const char *filename);
int next_line(FILE* trace);
unsigned int init(FILE* config);
void* cpu_thread_sim(void* trace);
void print_usage_and_exit(void);
map<string, vector<string> > parse_args(int argc, char** argv);

FILE* open_file(const char *filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        cerr << "File " << filename << " could not be opened\n";
        print_usage_and_exit();
    }
    return file;
}

int next_line(FILE* trace) {
    if (feof(trace) || ferror(trace)) {
        return 0;
    }
    else {
        unsigned int core;
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

unsigned int init(FILE* config) {
    unsigned int num_cpus;
    protocol_t protocol;
    Cache::config_t cache_cfg1;
    cache_cfg1.cache_type = L1;
    unsigned int mem_size;
    unsigned int bus_width;
    fscanf(config, "%u, %d\n", &num_cpus, &protocol);
    fscanf(config, "%u, %u, %u, %d, %d\n", &cache_cfg1.line_size, 
            &cache_cfg1.cache_size, &cache_cfg1.associativity, &cache_cfg1.hit_time, &cache_cfg1.miss_penalty);
    fscanf(config, "%u, %u", &mem_size, &bus_width);
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
    cout << "Usage:\n  ./simulator <config file> {-s <trace file> | -p <trace file>...} [options]\n"
            "   -s : Single trace file for all cores, single thread for sequential accesses to cores\n"
            "   -p : One trace file for each core, cores access in parallel. Must have one trace file listed per core in config\n"
            "  options:\n"
            "   -v : Optional, verbose output; see when there is a data request from memory, writeback to memory, invalidation, and state changes for cache blocks\n";
    exit(-1);
}

map<string, vector<string> > parse_args(int argc, char** argv) {
    if (argc < 4) {
        print_usage_and_exit();
    }
    map<string, vector<string> > args;
    for (int i = 2; i < argc; i++) { // skip first (program name) and second (config file)
        if (argv[i][0] == '-') {
            const string key = argv[i];
            vector<string> values;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                values.push_back(string(argv[i + 1]));
                i++;
            }
            args[key] = values;
        }
    }
    return args;
}

int main(int argc, char** argv) {
    FILE *config;

    map<string, vector<string> > args = parse_args(argc, argv);
    
    config = open_file(argv[1]);
    unsigned int num_cpus = init(config);
    pthread_mutex_init(&simulator_mutex, NULL);

    if (args.count("-v")) {
        verbose = true;
    }

    if (args.count("-p")) {
        if (args["-p"].size() < num_cpus) {
            cout << "Not enough trace files provided for parallel access.\n";
            print_usage_and_exit();
        }
        cpu_threads = new pthread_t[num_cpus];
    

        // create thread for each cpu
        for (unsigned int i = 0; i < num_cpus; i++) {
            pthread_create(&cpu_threads[i], NULL, cpu_thread_sim, (void*) open_file(&args["-p"][i][0]));
        }

        // wait for all threads to finish
        for (unsigned int i = 0; i < num_cpus; i++) {
            pthread_join(cpu_threads[i], NULL);
        }

        sys.print_stats();

        fclose(config);
        delete cpu_threads;
        pthread_mutex_destroy(&simulator_mutex);
    } else if (args.count("-s")) {
        FILE* input = open_file(&args["-s"][0][0]);
        while (next_line(input));
        sys.print_stats();
        fclose(input);
        fclose(config);
    } else {
        print_usage_and_exit();
    }

    cout << "Simulation Completed\n";

    return 0;
}

