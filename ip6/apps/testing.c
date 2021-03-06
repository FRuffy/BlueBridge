#define _GNU_SOURCE
#include "../lib/client_lib.h"
#include "../lib/utils.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/resource.h>
#include <pthread.h>
#include <sys/stat.h>


static int NUM_THREADS = 1;
static int BATCHED_MODE = 0;
static uint64_t NUM_ITERS;

void create_dir(const char *name) {
    struct stat st = {0};
    if (stat(name, &st) == -1) {
        int MAX_FNAME = 256;
        char fname[MAX_FNAME];
        snprintf(fname, MAX_FNAME, "mkdir -p %s -m 777", name);
        printf("Creating dir %s...\n", name);
        system(fname);
    }
}

void save_time(const char *type, uint64_t* latency, uint64_t entries, uint64_t num_iterations){
    int MAX_FNAME = 256;
    char fname[MAX_FNAME];
    snprintf(fname, MAX_FNAME, "results/testing/t%d_iter%lu", NUM_THREADS, NUM_ITERS);
    create_dir(fname);
    memset(fname, 0, MAX_FNAME);
    snprintf(fname, MAX_FNAME, "results/testing/t%d_iter%lu/%s.csv", NUM_THREADS, NUM_ITERS, type);
    FILE *file = fopen(fname, "w");
    if (file == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }
    fprintf(file, "latency (ns)\n");
    uint64_t i;
    for (i = 0; i < entries; i++) {
        fprintf(file, "%llu\n", (unsigned long long) latency[i]);
    }
    fclose(file);
    uint64_t total = 0;
    for (i = 0; i < entries; i++)
        total += (unsigned long long) latency[i];
    printf("Average %s latency: "KRED"%lu us\n"RESET, type, total / (num_iterations*1000));
    double megabit = (double) 8 * NUM_ITERS * BLOCK_SIZE / (1000*1000);
    double seconds = (double) total / (1000 * 1000 * 1000);
    printf("Average %s throughput: "KRED"%f mbps\n"RESET, type, megabit / seconds);

}

uint64_t *alloc_test(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr, uint64_t iterations, struct config my_conf) {
    uint64_t *latency = calloc(iterations, sizeof(uint64_t));
    if (BATCHED_MODE) {
        uint64_t split = iterations/my_conf.num_hosts;
        int length;
        for (int i = 0; i < my_conf.num_hosts; i++) {
            uint64_t start = getns();
            uint64_t offset = split * i;
            if (i == my_conf.num_hosts - 1)
                length = iterations - offset;
            else
                length = split;
            target_ip->sin6_addr = get_ip6_target(i);
            ip6_memaddr *temp = allocate_bulk_rmem(target_ip, length);
            memcpy(&r_addr[offset], temp, length *sizeof(ip6_memaddr));
            free(temp);
            latency[i] = getns() - start;
        }
    } else {
        for (int i = 0; i < iterations; i++) {
           // Generate a random IPv6 address out of a set of available hosts
            //memcpy(&(target_ip->sin6_addr), gen_rdm_IPv6Target(), sizeof(struct in6_addr));
            target_ip->sin6_addr = get_ip6_target(i % my_conf.num_hosts);
            uint64_t start = getns();
            r_addr[i] = allocate_rmem(target_ip);
            latency[i] = getns() - start;
        }
    }
    return latency;
}

uint64_t *write_test(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr, uint64_t iterations, uint8_t *payload) {
    uint64_t *latency = calloc(iterations, sizeof(uint64_t));
    if (BATCHED_MODE) {
        printf("Transmitting...\n");
        uint64_t wStart = getns();
        write_bulk_rmem(target_ip, r_addr, iterations, (char*) payload, BLOCK_SIZE * iterations);
        latency[0] = getns() - wStart;
    } else {
        for (uint64_t i = 0; i < iterations; i++) {
            ip6_memaddr remote_mem = r_addr[i];
            print_debug("Creating payload");
            uint64_t wStart = getns();
            write_rmem(target_ip, &remote_mem, (char *) &payload[i* BLOCK_SIZE], BLOCK_SIZE);
            latency[i] = getns() - wStart;
        }
    }
    return latency;
}

uint64_t *read_test(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr, uint64_t iterations, uint8_t *expected) {
    uint64_t *latency = calloc(iterations, sizeof(uint64_t));
    if (BATCHED_MODE) {
        char *test = malloc(BLOCK_SIZE * iterations);
        printf("Retrieving data...\n");
        uint64_t rStart = getns();
        read_bulk_rmem(target_ip, r_addr, iterations, test, BLOCK_SIZE * iterations);
        latency[0] = getns() - rStart;
        printf("Comparing...\n");
        for (uint64_t i = 0; i < iterations; i++) {
            if (memcmp(&test[i*BLOCK_SIZE], &expected[i*BLOCK_SIZE], BLOCK_SIZE) != 0) {
                printf(KRED"ERROR: WRONG RESULT AT ITERATION %lu\n"RESET, i);
                printf("Expected\t");
                print_n_bytes(expected, BLOCK_SIZE);
                printf("Got \t");
                print_n_bytes(test, BLOCK_SIZE);
                exit(1);
            }
        }
        printf(KGRN"Everything okay!\n"RESET);
        free(test);
    } else {
        char test[BLOCK_SIZE];
        for (uint64_t i = 0; i < iterations; i++) {
            ip6_memaddr remote_mem = r_addr[i];
            uint64_t rStart = getns();
            read_rmem(target_ip, &remote_mem, test, BLOCK_SIZE);
            latency[i] = getns() - rStart;
            print_debug("Results of memory store: %.50s", test);
            if (memcmp(test, (char *) &expected[i*BLOCK_SIZE], BLOCK_SIZE) != 0) {
                perror(KRED"ERROR: WRONG RESULT"RESET);
                printf("Expected %s Got %s\n", expected, test );
                exit(1);
            }
        }
    }
    return latency;
}

uint64_t *free_test(struct sockaddr_in6 *target_ip, ip6_memaddr *r_addr, uint64_t iterations, struct config my_conf) {
    uint64_t *latency = calloc(iterations, sizeof(uint64_t));
    if (BATCHED_MODE) {
        uint64_t split = iterations/my_conf.num_hosts;
        for (int i = 0; i < my_conf.num_hosts; i++) {
            uint64_t fStart = getns();
            uint64_t offset = split * i;
            free_rmem(target_ip, &r_addr[offset]);
            latency[i] = getns() - fStart;
        }
    } else {
        for (int i = 0; i < iterations; i++) {
            ip6_memaddr remote_mem = r_addr[i];
            uint64_t fStart = getns();
            free_rmem(target_ip, &remote_mem);
            latency[i] = getns() - fStart;
        }
    }
    return latency;
}

void launch_tests(struct sockaddr_in6 *target_ip, struct config my_conf, uint64_t num_iterations, uint16_t thread_id){
    printf("Thread %d Allocating pointers\n", thread_id);
    ip6_memaddr *r_addr = malloc(sizeof(ip6_memaddr) * num_iterations);
    if(!r_addr)
        perror("Allocation too large");
    printf("Thread %d Generating random test data of size %f megabyte...\n", thread_id, (double) BLOCK_SIZE * num_iterations / (1000*1000));
    uint8_t *payload1 = gen_rdm_bytestream(BLOCK_SIZE * num_iterations, 1);
    // ALLOC TEST
    printf("Thread %d Starting allocation test...\n", thread_id);
    uint64_t *alloc_latency = alloc_test(target_ip, r_addr, num_iterations, my_conf);
    // WRITE TEST
    printf("Thread %d Starting write test...\n", thread_id);
    uint64_t *write_latency1 = write_test(target_ip, r_addr, num_iterations, payload1);
    // READ TEST
    printf("Thread %d Starting read test...\n", thread_id);
    uint64_t *read_latency1 = read_test(target_ip, r_addr, num_iterations, payload1);
    free(payload1);
    printf("Thread %d Generating new random test data of size %f megabyte...\n", thread_id, (double) BLOCK_SIZE * num_iterations / (1000*1000));
    uint8_t *payload2 = gen_rdm_bytestream(BLOCK_SIZE * num_iterations, 2);
    // WRITE TEST 2
    printf("Thread %d Starting second write test...\n", thread_id);
    uint64_t *write_latency2 = write_test(target_ip, r_addr, num_iterations, payload2);
    // READ TEST 2
    printf("Thread %d Starting second read test...\n", thread_id);
    uint64_t *read_latency2 = read_test(target_ip, r_addr, num_iterations, payload2);
    // FREE TEST
    printf("Thread %d Starting free test...\n", thread_id);
    uint64_t *free_latency = free_test(target_ip, r_addr, num_iterations, my_conf);
    free(payload2);

    int samples;
    if (BATCHED_MODE)
        samples = 1;
    else
        samples = num_iterations;
    int MAX_FNAME = 256;
    char fname[MAX_FNAME];
    printSendLat();
    snprintf(fname, MAX_FNAME, "alloc_t%d", thread_id);
    save_time(fname, alloc_latency, samples, num_iterations);
    memset(fname, 0, MAX_FNAME);
    snprintf(fname, MAX_FNAME, "read1_t%d", thread_id);
    save_time(fname, read_latency1, samples, num_iterations);
    memset(fname, 0, MAX_FNAME);
    snprintf(fname, MAX_FNAME, "write1_t%d", thread_id);
    save_time(fname, write_latency1, samples, num_iterations);
    snprintf(fname, MAX_FNAME, "read2_t%d", thread_id);
    save_time(fname, read_latency1, samples, num_iterations);
    memset(fname, 0, MAX_FNAME);
    snprintf(fname, MAX_FNAME, "write2_t%d", thread_id);
    save_time(fname, write_latency1, samples, num_iterations);
    snprintf(fname, MAX_FNAME, "free_t%d", thread_id);
    save_time(fname, free_latency, samples, num_iterations);
    memset(fname, 0, MAX_FNAME);
    free(alloc_latency);
    free(write_latency1);
    free(read_latency1);
    free(write_latency2);
    free(read_latency2);
    free(free_latency);
}

typedef struct _thread_data_t {
    int tid;
    struct sockaddr_in6 *target_ip;
    ip6_memaddr *r_addr;
    uint64_t length;
} thread_data_t;

void *testing_loop(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    // Assign threads to cores
    // int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    // int assigned = data->tid % num_cores;
    // pthread_t my_thread = pthread_self();
    // cpu_set_t cpuset;
    // CPU_ZERO(&cpuset);
    // CPU_SET(assigned, &cpuset);
    // pthread_setaffinity_np(my_thread, sizeof(cpu_set_t), &cpuset);
    // printf("Assigned Thread %d to core %d\n", data->tid, assigned );
    // Initialize BlueBridge
    struct config my_conf = get_bb_config();
    struct sockaddr_in6 *target_ip = init_net_thread(data->tid, &my_conf, 0);
    launch_tests(target_ip, my_conf, data->length, data->tid);
    close_sockets();
    return NULL;
}

void basic_op_threads(struct sockaddr_in6 *target_ip) {
    close_sockets();
    pthread_t thr[NUM_THREADS];
    int i;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[NUM_THREADS];

    pthread_attr_t attr;
    size_t  stacksize = 0;

    pthread_attr_init( &attr );
    pthread_attr_getstacksize( &attr, &stacksize );
    pthread_attr_setstacksize( &attr, 99800000 );
    pthread_attr_getstacksize( &attr, &stacksize );

    /* create threads */
    for (i = 0; i < NUM_THREADS; i++) {
        int rc;
        uint64_t split = NUM_ITERS/NUM_THREADS;
        thr_data[i].tid =  i;
        thr_data[i].target_ip =  target_ip;
        thr_data[i].length = split;
        struct rlimit limit;
        getrlimit (RLIMIT_STACK, &limit);
        printf ("\nStack Limit = %ld and %ld max\n", limit.rlim_cur, limit.rlim_max);
        printf("Launching thread %d\n", i );
        if ((rc = pthread_create(&thr[i],  &attr, testing_loop, &thr_data[i]))) {
          fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        }
    }

    /* block until all threads complete */
    for (i = 0; i < NUM_THREADS; i++) {
        printf("Thread %d Waiting for my friends...\n", i);
        pthread_join(thr[i], NULL);
    }
}

void basic_ops(struct sockaddr_in6 *target_ip) {

    // Initialize BlueBridge
    struct config my_conf = get_bb_config();
    launch_tests(target_ip, my_conf, NUM_ITERS, 0);
}

/*
 * Main workhorse method. Parses arguments, setups connections
 * Allows user to issue commands on the command line.
 */
int main(int argc, char *argv[]) {
    // Example Call:
    //./applications/bin/testing -c ./tmp/config/distMem.cnf -t 4 -i 10000
    int c;
    struct config my_conf;
    while ((c = getopt (argc, argv, "c:i:t:b")) != -1) {
    switch (c)
      {
      case 'c':
        my_conf = set_bb_config(optarg, 0);
        break;
      case 'i':
        NUM_ITERS = atoi(optarg);
        break;
      case 't':
        NUM_THREADS = atoi(optarg);
        break;
      case 'b':
        BATCHED_MODE = 1;
        break;
      case '?':
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          printf("usage: -c config -t num_threads -i NUM_ITERS>\n");
        return 1;
      default:
        abort ();
      }
    }
    printf("Running test with %d threads and %lu iterations.\n", NUM_THREADS, NUM_ITERS );
    struct sockaddr_in6 *temp = init_sockets(&my_conf, 0);
    set_host_list(my_conf.hosts, my_conf.num_hosts);

    struct timeval st, et;
    uint64_t start = getns();
    if (NUM_THREADS > 1)
        basic_op_threads(temp);
    else
        basic_ops(temp);
    double elapsed = getns() - start;
    printf(KGRN "Finished\n"RESET);
    printf("Total Time: "KRED"%.2f"RESET" us "KRED"%.2f"RESET" ms "KRED"%.2f"RESET" seconds\n",
             elapsed/1000, elapsed/(1000*1000), elapsed/(1000*1000*1000));
    printSendLat();
    free(temp);
    //close_sockets();
    return EXIT_SUCCESS;
}