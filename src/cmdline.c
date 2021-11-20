#include <stdio.h>
#include <getopt.h>

#include <dpdk_common.h>

#include "cmdline.h"

int parsecmdline(struct cmdline *cmd, int argc, char **argv)
{
    int c = -1, ret;

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
                enabled_portmask = parse_portmask(optarg);

                if (enabled_portmask == 0)
                {
                    fprintf(stderr, "Invalid portmask specified with -p or --portmask.\n");

                    return -1;
                }

                break;

            case 'P':
                ret = parse_port_pair_config(optarg);

                if (ret)
                {
                    fprintf(stderr, "Invalid portmap config.\n");

                    return -1;
                }

                break;

            case 'q':
                rxqueuepl = parse_queues(optarg);
                cmd->queues = rxqueuepl;

                if (rxqueuepl == 0)
                {
                    fprintf(stderr, "Invalid queue number argument with -q or --queues.\n");

                    return -1;
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