#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "betree.hpp"
#include <fstream> 
#include <chrono>

void timer_start(uint64_t &timer)
{
    struct timeval t;
    assert(!gettimeofday(&t, NULL));
    timer -= 1000000 * t.tv_sec + t.tv_usec;
}

void timer_stop(uint64_t &timer)
{
    struct timeval t;
    assert(!gettimeofday(&t, NULL));
    timer += 1000000 * t.tv_sec + t.tv_usec;
}

void benchmark_queries(betree<uint64_t, std::string> &b, uint64_t nops, uint64_t number_of_distinct_keys, uint64_t random_seed, const char* outputFileName, const char* outputFileName2)
{
    std::vector<double> throughputs;
    std::vector<double> ops_times;
    std::string line;
    std::vector<uint64_t> queryKeys; // Store the keys to be queried

    // Pre-load the tree with data
    std::ifstream file("skewed_keys.txt");
    if (!file) {
        std::cerr << "Error: Could not open the file." << std::endl;
    }

    while (std::getline(file, line)) {
        uint64_t key;
        std::string value;
        std::istringstream iss(line);
        if (iss >> key >> value) {
            // Update b with the key and value
            b.update(key, value);
            queryKeys.push_back(key); // Add the key to the queryKeys vector
        }
    }

    // Now go back and query it
    srand(random_seed);
    uint64_t overall_timer = 0;
    timer_start(overall_timer);
    uint64_t nops_query = 0.7 * nops;  // 70% of nops
    uint64_t nops_update = 0.3 * nops;  // 30% of nops

    for (uint64_t i = 0; i < nops; i++)
    {
        std::chrono::high_resolution_clock::time_point start_time, end_time;

        uint64_t timers = 0;
        uint64_t timers2 = 0;

        if (nops_query > 0)
        {
            // Perform a query
            if (!queryKeys.empty()) {
            uint64_t t = queryKeys.back();
            queryKeys.pop_back();
            timer_start(timers);
            std::string result = b.query(t);
            timer_stop(timers);
            double query_timers = timers;
            ops_times.push_back(query_timers);
            throughputs.push_back(1.0/(query_timers)); //Converting into seconds
            nops_query--;
            }
        }
        else if (nops_update > 0)
        {
            // Perform an update
            uint64_t t = rand() % number_of_distinct_keys;
            std::string value = std::to_string(t) + ":";
            timer_start(timers2);
            b.update(t, value);
            timer_stop(timers2);
            nops_update--;
            double update_timers = timers2;
            ops_times.push_back(update_timers);
            throughputs.push_back(1.0/(update_timers));
            printf("# overall: %ld %ld, %f\n", nops, overall_timer, 1.0/(update_timers));
        }
    }
    timer_stop(overall_timer); 

    // Output time and throughput data to the output file
    std::ofstream outputFile(outputFileName);
    if (outputFile.is_open())
    {
        int cnt = 0;
        for(int i=0; i<ops_times.size(); i+=100) {
            
            cnt++;
            double total = 0;
            
            for(int j=0; j<100 && i+j < ops_times.size(); j++) {
                total += ops_times[i+j]; 
            }

            double average = total / 100;
        
            // Output average
            std::cout << "Average time for queries " << i << " to " << i+99 << ": " << average << std::endl;
            outputFile << cnt << " " << average << "\n";
        }
        
        outputFile.close();
    }
    else
    {
        std::cerr << "Failed to open output file for writing." << std::endl;
        exit(1);
    }

    std::ofstream outputFile2(outputFileName2);
    if (outputFile2.is_open())
    {
        int cnt = 0;
        for(int i=0; i<throughputs.size(); i+=100) {
            
            cnt++;
            double total2 = 0;
            
            for(int j=0; j<100 && i+j < throughputs.size(); j++) {
                total2 += throughputs[i+j]; 
                std::cout << " Throughput for queries " << i <<  ": " << total2 << std::endl;
            }

            double average = total2 / 100;
        
            // Output average
            std::cout << "Average throughput for queries " << i << " to " << i+99 << ": " << average << std::endl;
            outputFile2 << cnt << " " << average << "\n";
        }
        
        outputFile2.close();
    }
    else
    {
        std::cerr << "Failed to open output file for writing." << std::endl;
        exit(1);
    }

    double throughput = (1.0 * nops * 1000000) / overall_timer;
    printf("# overall: %ld %ld, %f\n", nops, overall_timer, throughput);
}

#define DEFAULT_TEST_MAX_NODE_SIZE (1ULL<<6)
#define DEFAULT_TEST_MIN_FLUSH_SIZE (DEFAULT_TEST_MAX_NODE_SIZE / 4)
#define DEFAULT_TEST_CACHE_SIZE (4)
#define DEFAULT_TEST_NDISTINCT_KEYS (1ULL << 10)
#define DEFAULT_TEST_NOPS (1ULL << 12)

int main(int argc, char **argv)
{
    char *mode = NULL;
    uint64_t max_node_size = DEFAULT_TEST_MAX_NODE_SIZE;
    uint64_t min_flush_size = DEFAULT_TEST_MIN_FLUSH_SIZE;
    uint64_t cache_size = DEFAULT_TEST_CACHE_SIZE;
    char *backing_store_dir = NULL;
    uint64_t number_of_distinct_keys = DEFAULT_TEST_NDISTINCT_KEYS;
    uint64_t nops = DEFAULT_TEST_NOPS;
    unsigned int random_seed = time(NULL) * getpid();

    int opt;
    char *term;

    //////////////////////
    // Argument parsing //
    //////////////////////

    while ((opt = getopt(argc, argv, "m:d:N:f:C:k:t:s:")) != -1)
    {
        switch (opt)
        {
        case 'm':
            mode = optarg;
            break;
        case 'd':
            backing_store_dir = optarg;
            break;
        case 'N':
            max_node_size = strtoull(optarg, &term, 10);
            if (*term)
            {
                std::cerr << "Argument to -N must be an integer" << std::endl;
                exit(1);
            }
            break;
        case 'f':
            min_flush_size = strtoull(optarg, &term, 10);
            if (*term)
            {
                std::cerr << "Argument to -f must be an integer" << std::endl;
                exit(1);
            }
            break;
        case 'C':
            cache_size = strtoull(optarg, &term, 10);
            if (*term)
            {
                std::cerr << "Argument to -C must be an integer" << std::endl;
                exit(1);
            }
            break;
        case 'k':
            number_of_distinct_keys = strtoull(optarg, &term, 10);
            if (*term)
            {
                std::cerr << "Argument to -k must be an integer" << std::endl;
                exit(1);
            }
            break;
        case 't':
            nops = strtoull(optarg, &term, 10);
            if (*term)
            {
                std::cerr << "Argument to -t must be an integer" << std::endl;
                exit(1);
            }
            break;
        case 's':
            random_seed = strtoull(optarg, &term, 10);
            if (*term)
            {
                std::cerr << "Argument to -s must be an integer" << std::endl;
                exit(1);
            }
            break;
        default:
            std::cerr << "Unknown option '" << (char)opt << "'" << std::endl;
            exit(1);
        }
    }

    FILE *script_input = NULL;
    FILE *script_output = NULL;

    if (mode == NULL || (strcmp(mode, "benchmark-queries") != 0))
    {
        std::cerr << "Must specify mode as \"benchmark-queries\"" << std::endl;
        exit(1);
    }

    if (backing_store_dir == NULL)
    {
        std::cerr << "-d <backing_store_directory> is required" << std::endl;
        exit(1);
    }

    ////////////////////////////////////////////////////////
    // Construct a betree and run the benchmark queries   //
    ////////////////////////////////////////////////////////

    one_file_per_object_backing_store ofpobs(backing_store_dir);
    swap_space sspace(&ofpobs, cache_size);
    betree<uint64_t, std::string> b(&sspace, max_node_size, min_flush_size);

    char* outputFileName = "ops_times.txt"; // Name of the output file
    char* outputFileName2 = "throughput.txt";
    benchmark_queries(b, nops, number_of_distinct_keys, random_seed, outputFileName, outputFileName2);

    return 0;
}
