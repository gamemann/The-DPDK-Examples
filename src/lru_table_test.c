#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <dpdk_common.h>
#include <rte_table_hash.h>
#include <rte_jhash.h>

#define MAX_TABLE_SIZE 100

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

    // Create table.
    struct rte_table_ops lru = rte_table_hash_lru_ops;

    struct rte_table_hash_params params = {0};
    params.f_hash = (rte_table_hash_op_hash)rte_jhash;
    params.key_size = sizeof(__u64);
    params.n_buckets = 1024;
    params.n_keys = 1;
    params.name = "lru_table";
    params.seed = 0;

    void *tbl = lru.f_create(&params, rte_socket_id(), sizeof(__u32));

    if (tbl == NULL)
    {
        printf("Table is NULL.\n\n");
    }
    else
    {
        printf("Table is GOOD!\n\n");
    }

    int key_found = 0;
    void **ptr;

    for (int i = 0; i < 1000; i++)
    {
        __u64 key = i;
        __u32 val = i * 2;
        if (lru.f_add(tbl, &key, &val, &key_found, ptr) != 0)
        {
            printf("Failed to insert key at %llu.\n", key);
        }
    }

    // Cleanup EAL.
    ret = dpdkc_eal_cleanup();

    dpdkc_check_ret(&ret);

    return 0;
}