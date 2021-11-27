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
#include <rte_hash.h>
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

    // Create rate limits table.
    struct rte_hash_parameters hparams =
    {
        .name = "rate_limits",
        .key_len = sizeof(__u32),
        .entries = MAX_TABLE_SIZE,
        .hash_func = rte_jhash,
        .socket_id = rte_socket_id()
    };
     
    void *rl_tbl = rte_hash_create(&hparams);

    if (rl_tbl == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failed to create rate limits table.\n");
    }

    int pos = 0;

    for (int i = 1; i < 1000; i++)
    {
        // Check with overflow.
        if (i > MAX_TABLE_SIZE)
        {
            if (pos > (MAX_TABLE_SIZE - 1))
            {
                pos = 0;
            }

            int *key;

            rte_hash_get_key_with_position(rl_tbl, pos, (void **)&key);

            pos++;

            rte_hash_del_key(rl_tbl, key);

            printf("Found key to del at %d.\n", *key);
        }

        int val = 1;

        int ret = rte_hash_add_key_data(rl_tbl, &i, &val);

        if (ret < 0)
        {
            printf("Failed to insert at %d.\n", i);
        }
        else
        {
            printf("Successfully inserted at %d.\n", i);
        }
    }

    // Cleanup EAL.
    ret = dpdkc_eal_cleanup();

    dpdkc_check_ret(&ret);

    return 0;
}