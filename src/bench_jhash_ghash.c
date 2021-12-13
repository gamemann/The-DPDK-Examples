#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/types.h>
#include <time.h>

#include <dpdk_common.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include <glib.h>

#define MAX_TABLE_SIZE 100000

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

        if (ret < 0)
        {
            printf("Failed to insert at %u.\n", key);
        }
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

        if (ret < 0)
        {
            printf("Failed to lookup at key %u.\n", key);
        }
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

        if (!g_hash_table_insert(ghash_tbl, key, val))
        {
            printf("Failed to insert data for GHash table at %u.\n", *key);
        }
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

        if (val == NULL)
        {
            printf("Failed to lookup for GHash table at %u.\n", *key);
        }
    }

    // End benchmark.
    end = clock();

    // Get elapsed.
    ela = end - start;

    printf("GHash Lookup => %lu.\n", ela);

    // Cleanup EAL.
    ret = dpdkc_eal_cleanup();

    dpdkc_check_ret(&ret);

    return EXIT_SUCCESS;
}