#include <stdio.h>
#include <getopt.h>

#include <dpdk_common.h>

#include "cmdline.h"

int parsecmdline(struct cmdline *cmd, int argc, char **argv)
{
    int c = -1, ret;
    struct dpdkc_error cret =
    {
        .err_num = 0,
        .gen_msg = NULL,
        .port_id = -1,
        .rx_id = -1,
        .tx_id = -1
    };

    static const struct option lopts[] =
    {
        {"portmask", required_argument, NULL, 'p'},
        {"portmap", required_argument, NULL, 'P'},
        {"queues", required_argument, NULL, 'q'},
        {"promisc", no_argument, NULL, 'x'},
        {NULL, 0, NULL, 0}
    };

    while ((c = getopt_long(argc, argv, "p:P:q:x", lopts, NULL)) != EOF)
    {
        switch (c)
        {
            case 'p':
                enabled_port_mask = dpdkc_parse_arg_port_mask(optarg);

                if (enabled_port_mask == 0)
                {
                    rte_exit(EXIT_FAILURE, "Invalid portmask specified with -p or --portmask.\n");
                }

                break;

            case 'P':
                cret = dpdkc_parse_arg_port_pair_config(optarg);

                dpdkc_check_error(&cret);

                break;

            case 'q':
                rx_queue_pl = dpdkc_parse_arg_queues(optarg);
                cmd->queues = rx_queue_pl;

                if (rx_queue_pl == 0)
                {
                    rte_exit(EXIT_FAILURE, "Invalid queue number argument with -q or --queues.\n");
                }

                break;

            case 'x':
                cmd->promisc = 1;

                break;

            case '?':
                fprintf(stdout, "Missing argument.\n");

                break;
        }
    }
}