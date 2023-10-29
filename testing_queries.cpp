#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "betree.hpp"
#include <fstream> 

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

/*int next_command(FILE *input, int *op, uint64_t *arg)
{
    int ret;
    char command[64];

    ret = fscanf(input, "%s %ld", command, arg);
    if (ret == EOF)
        return EOF;
    else if (ret != 2)
    {
        fprintf(stderr, "Parse error\n");
        exit(3);
    }

    if (strcmp(command, "Query") == 0)
    {
        *op = 3;
        if (1 != fscanf(input, " -> %s", command))
        {
            fprintf(stderr, "Parse error\n");
            exit(3);
        }
    }
    else
    {
        return -1; // Skip other operations
    }

    return 0;
}*/

void benchmark_queries(betree<uint64_t, std::string> &b, uint64_t nops, uint64_t number_of_distinct_keys, uint64_t random_seed, const char* outputFileName)
{
    srand(random_seed);

    std::vector<uint64_t> times;
    std::vector<double> throughputs;

    // Pre-load the tree with data
    for (uint64_t i = 0; i < nops; i++)
    {
        uint64_t t = rand() % number_of_distinct_keys;
        std::string value = std::to_string(t) + ":";
        b.update(t, value);
        printf("Updated key %ld with value: %s\n", t, value.c_str());
    }

    // Now go back and query it
    srand(random_seed);
    uint64_t overall_timer = 0;
    timer_start(overall_timer);
    for (uint64_t i = 0; i < nops; i++)
    {
        //uint64_t t = rand() % number_of_distinct_keys;
        //b.query(t);
        uint64_t timer = 0;
        timer_start(timer);

        uint64_t t = rand() % number_of_distinct_keys;
        std::string result = b.query(t);

        timer_stop(timer);
        printf("Query for key %ld returned value: %s\n", t, result.c_str());
        double throughput2 = (1000000) / timer;
        times.push_back(i + 1);
        throughputs.push_back(throughput2);
    }
    timer_stop(overall_timer);

    // Output time and throughput data to the output file
    std::ofstream outputFile(outputFileName);
    if (outputFile.is_open())
    {
        for (size_t i = 0; i < times.size(); i++)
        {
            outputFile << times[i] << " " << throughputs[i] << "\n";
        }
        outputFile.close();
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

    char* outputFileName = "throughput.txt"; // Name of the output file
    benchmark_queries(b, nops, number_of_distinct_keys, random_seed, outputFileName);

    return 0;
}
