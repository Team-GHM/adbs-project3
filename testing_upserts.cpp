#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "betree.hpp"
//#include "gnuplot-iostream.h"

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

    if (strcmp(command, "Updating") == 0)
    {
        *op = 1;
    }
    else
    {
        return -1; // Skip other operations
    }

    return 0;
}*/

void benchmark_upserts(betree<uint64_t, std::string> &b, uint64_t nops, uint64_t number_of_distinct_keys, uint64_t random_seed)
{
    srand(random_seed);

    std::vector<std::pair<uint64_t, double>> throughputData;  // Stores (iteration, throughput) pairs
    uint64_t totalIterations = 100;  // Total number of iterations

    for (uint64_t j = 0; j < totalIterations; j++)
    {
        uint64_t timer = 0;
        timer_start(timer);
        for (uint64_t i = 0; i < nops / totalIterations; i++)
        {
            uint64_t t = rand() % number_of_distinct_keys;
            b.update(t, std::to_string(t) + ":");
        }
        timer_stop(timer);
        double throughput = (1.0 * (nops / totalIterations) * 1000000) / timer;
        throughputData.push_back(std::make_pair(j, throughput));
    }

    // Create a Gnuplot object
    /*Gnuplot gp;

    // Configure the plot
    gp << "set title 'Upserts Throughput Over Iterations'\n";
    gp << "set xlabel 'Iteration'\n";
    gp << "set ylabel 'Throughput (ops/sec)'\n";

    // Plot the throughput data
    gp << "plot '-' with lines title 'Throughput'\n";
    gp.send1d(throughputData);
    gp.flush();*/
}

#define DEFAULT_TEST_MAX_NODE_SIZE (1ULL << 6)
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

    if (mode == NULL || (strcmp(mode, "benchmark-upserts") != 0))
    {
        std::cerr << "Must specify mode as \"benchmark-upserts\"" << std::endl;
        exit(1);
    }

    if (backing_store_dir == NULL)
    {
        std::cerr << "-d <backing_store_directory> is required" << std::endl;
        exit(1);
    }

    ////////////////////////////////////////////////////////
    // Construct a betree and run the benchmark upserts  //
    ////////////////////////////////////////////////////////

    one_file_per_object_backing_store ofpobs(backing_store_dir);
    swap_space sspace(&ofpobs, cache_size);
    betree<uint64_t, std::string> b(&sspace, max_node_size, min_flush_size);

    benchmark_upserts(b, nops, number_of_distinct_keys, random_seed);

    return 0;
}
