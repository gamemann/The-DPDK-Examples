#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/types.h>
#include <time.h>

#define USE_HASH_TABLES

#include <dpdk_common.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include <rte_table.h>
#include <rte_table_hash.h>
#include <rte_table_hash_func.h>

#include <glib.h>

#define MAX_TABLE_SIZE 100000
#define MAX_TABLE_LRU_SIZE 50000

//#define DEBUG

struct lru_key
{
    __u32 src;
    __u32 dst;
};

/**
 * The main function call.
 * 
 * @param argc The amount of arguments.
 * @param argv A pointer to the arguments array.
 * 
 * @return Return code.
**/
int main(int argc, char **argv)
{
    // Initialiize result variables.
    struct dpdkc_ret ret = dpdkc_ret_init();

    // Initialize EAL and check.
    ret = dpdkc_eal_init(argc, argv);

    dpdkc_check_ret(&ret);

    // Iterator.
    unsigned int i;

    // Benchmark variables.
    clock_t start;
    clock_t end;
    clock_t ela;

    // Create DPDK JHash table parameters.
    struct rte_hash_parameters hparams =
    {
        .name = "jhash_tbl",
        .key_len = sizeof(__u32),
        .entries = MAX_TABLE_SIZE,
        .hash_func = rte_jhash,
        .socket_id = rte_socket_id()
    };
    
    // Attempt to create DPDK JHash table and check.
    void *jhash_tbl = rte_hash_create(&hparams);

    if (jhash_tbl == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failed to create DPDK JHash table.\n");
    }

    // Start benchmark.
    start = clock();

    // Insert data.
    for (i = 0; i < MAX_TABLE_SIZE; i++)
    {
        __u32 key = i;
        int val = 1;

        int ret = rte_hash_add_key_data(jhash_tbl, &key, &val);

#ifdef DEBUG
        if (ret < 0)
        {
            printf("Failed to insert at %u.\n", key);
        }
#endif
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("JHash Insert => %lu.\n", ela);

    // Start benchmark.
    start = clock();

    // Lookup.
    for (i = 0; i < MAX_TABLE_SIZE; i++)
    {
        __u32 key = i;
        int *val;

        int ret = rte_hash_lookup_data(jhash_tbl, &key, (void **)&val);

#ifdef DEBUG
        if (ret < 0)
        {
            printf("Failed to lookup at key %u.\n", key);
        }
#endif
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("JHash Lookup => %lu.\n", ela);

    // Create GLib table.
    GHashTable *ghash_tbl = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, g_free);

    if (ghash_tbl == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failed to create GHash table.\n");
    }

    // Begin benchmark.
    start = clock();

    // Insert data.
    for (i = 0; i < MAX_TABLE_SIZE; i++)
    {
        // Allocate key.
        __u32 *key = g_new0(__u32, 1);
        *key = i;

        // Allocate value.
        int *val = g_new0(int, 1);
        *val = 1;

        int ret = g_hash_table_insert(ghash_tbl, key, val);

#ifdef DEBUG
        if (!ret)
        {

            printf("Failed to insert data for GHash table at %u.\n", *key);
        }
#endif
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("GHash Insert => %lu.\n", ela);

    // Begin benchmark.
    start = clock();

    // Lookup data.
    for (i = 0; i < MAX_TABLE_SIZE; i++)
    {
        // Allocate key.
        __u32 *key = g_new0(__u32, 1);
        *key = i;

        int *val = g_hash_table_lookup(ghash_tbl, key);

#ifdef DEBUG
        if (val == NULL)
        {
            printf("Failed to lookup for GHash table at %u.\n", *key);
        }
#endif
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("GHash Lookup => %lu.\n", ela);

    // Create DPDK JHash LRU table parameters.
    struct rte_hash_parameters hparams_lru =
    {
        .name = "jhash_lru_tbl",
        .key_len = sizeof(struct lru_key),
        .entries = MAX_TABLE_LRU_SIZE,
        .hash_func = rte_jhash,
        .socket_id = rte_socket_id()
    };
    
    // Attempt to create DPDK JHash LRU table and check.
    void *jhash_lru_tbl = rte_hash_create(&hparams_lru);

    if (jhash_lru_tbl == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failed to create DPDK JHash LRU table.\n");
    }

    // Start benchmark.
    start = clock();

    // Insert data.
    for (i = 0; i < MAX_TABLE_LRU_SIZE * 2; i++)
    {
        struct lru_key key;
        key.src = i * 1000;
        key.dst = i * 10000;

        int val = 1;

        if (check_and_del_lru_from_hash_table(jhash_lru_tbl, MAX_TABLE_LRU_SIZE) != 0)
        {
#ifdef DEBUG
            printf("Failed to free exceeded LRU table.\n");
#endif

            continue;
        }

        int ret = rte_hash_add_key_data(jhash_lru_tbl, &key, &val);

#ifdef DEBUG
        if (ret < 0)
        {
            printf("Failed to insert at %u/%u.\n", key.src, key.dst);
        }
#endif
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("JHash LRU Insert => %lu.\n", ela);

    // Start benchmark.
    start = clock();

    // Lookup.
    for (i = MAX_TABLE_LRU_SIZE; i < MAX_TABLE_LRU_SIZE * 2; i++)
    {
        struct lru_key key;
        key.src = i * 1000;
        key.dst = i * 10000;

        int *val;

        int ret = rte_hash_lookup_data(jhash_lru_tbl, &key, (void **)&val);

#ifdef DEBUG
        if (ret < 0)
        {
            printf("Failed to lookup at key %u/%u.\n", key.src, key.dst);
        }
#endif
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("JHash LRU Lookup => %lu.\n", ela);

    // Create DPDK built-in LRU table parameters.
    struct rte_table_hash_params hparams_bi_lru = {0};
    hparams_bi_lru.name = "jhash_lru_bi_tbl";
    hparams_bi_lru.key_size = sizeof(struct lru_key);
    hparams_bi_lru.n_keys = MAX_TABLE_LRU_SIZE;
    hparams_bi_lru.n_buckets = 1;
    hparams_bi_lru.f_hash = (rte_table_hash_op_hash)rte_table_hash_crc_key64;
    hparams_bi_lru.seed = 0;

    struct rte_table_ops lru = rte_table_hash_lru_ops;
    
    // Attempt to create DPDK JHash LRU table and check.
    void *jhash_lru_bi_tbl = lru.f_create(&hparams_bi_lru, rte_socket_id(), sizeof(int));

    if (jhash_lru_bi_tbl == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failed to create DPDK JHash built-in LRU table.\n");
    }

    // Start benchmark.
    start = clock();

    int key_found = 0;

    // Insert data.
    for (i = 0; i < MAX_TABLE_LRU_SIZE * 2; i++)
    {
        struct lru_key key;
        key.src = i * 1000;
        key.dst = i * 10000;

        int val = 1;

        void *ent_ptr = (void *)&val;
        
        int ret = lru.f_add(jhash_lru_bi_tbl, &key, &val, &key_found, (void **)&ent_ptr);

#ifdef DEBUG
        if (ret != 0)
        {
            printf("Failed to insert at %u/%u.\n", key.src, key.dst);
        }
#endif
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("JHash LRU built-in Insert => %lu.\n", ela);

    // Cleanup EAL.
    ret = dpdkc_eal_cleanup();

    dpdkc_check_ret(&ret);

    return EXIT_SUCCESS;
}