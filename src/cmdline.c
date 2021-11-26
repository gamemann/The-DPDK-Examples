#include <stdio.h>
#include <getopt.h>

#include <dpdk_common.h>

#include "cmdline.h"

int parsecmdline(struct cmdline *cmd, int argc, char **argv)
{
    int c = -1;
    struct dpdkc_ret ret = dpdkc_ret_init();

    static const struct option lopts[] =
    {
        {"portmask", required_argument, NULL, 'p'},
        {"portmap", required_argument, NULL, 'P'},
        {"queues", required_argument, NULL, 'q'},
        {"promisc", no_argument, NULL, 'x'},
        {"stats", no_argument, NULL, 's'},
        {"pps", required_argument, NULL, 1},
        {"bps", required_argument, NULL, 2},
        {NULL, 0, NULL, 0}
    };

    while ((c = getopt_long(argc, argv, "p:P:q:xs", lopts, NULL)) != EOF)
    {
        switch (c)
        {
            case 'p':
                ret = dpdkc_parse_arg_port_mask(optarg);

                enabled_port_mask = (unsigned int)ret.data;

                if (enabled_port_mask == 0)
                {
                    rte_exit(EXIT_FAILURE, "Invalid portmask specified with -p or --portmask.\n");
                }

                break;

            case 'P':
                ret = dpdkc_parse_arg_port_pair_config(optarg);

                dpdkc_check_ret(&ret);

                break;

            case 'q':
                ret = dpdkc_parse_arg_queues(optarg);

                rx_queue_pl = (unsigned short)ret.data;
                
                if (rx_queue_pl == 0)
                {
                    rte_exit(EXIT_FAILURE, "Invalid queue number argument with -q or --queues.\n");
                }

                cmd->queues = rx_queue_pl;

                break;

            case 'x':
                cmd->promisc = 1;

                break;

            case 's':
                cmd->stats = 1;

                break;

            /* For rate limit application */
            case 1:
            {
                char *val = strdup(optarg);
                cmd->pps = strtoull((const char *)val, (char **)val, 0);

                break;
            }

            case 2:
            {
                char *val = strdup(optarg);
                cmd->bps = strtoull((const char *)val, (char **)val, 0);

                break;
            }
            
            case '?':
                fprintf(stdout, "Missing argument.\n");

                break;
        }
    }
}