#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include <dpdk_common.h>

#include "cmdline.h"

/* Global Variables */

static void forwardpacket(struct rte_mbuf *pckt, unsigned portid)
{
    unsigned dst_port;
    struct rte_eth_dev_tx_buffer *buffer;

    dst_port = dst_ports[portid];
    buffer = tx_buffer[dst_port];
    
    rte_eth_tx_buffer(dst_port, 0, buffer, pckt);
}

static void pcktloop(void)
{
    struct rte_mbuf *pckts_burst[MAX_PCKT_BURST];
    struct rte_mbuf *pckt;
    int sent;
    unsigned lcore_id = rte_lcore_id();
    unsigned i, j, portid, nb_rx;
    struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];
    const __u64 draintsc = (rte_get_tsc_hz() + US_PER_S -1) / US_PER_S * BURST_TX_DRAIN_US;
    struct rte_eth_dev_tx_buffer *buffer;

    __u64 prevtsc = 0, difftsc, curtsc;

    if (qconf->num_rx_ports == 0)
    {
        RTE_LOG(INFO, USER1, "lcore %u has nothing to do.\n", lcore_id);

        return;
    }

    RTE_LOG(INFO, USER1, "Looping with lcore %u.\n", lcore_id);

    for (i = 0; i < qconf->num_rx_ports; i++)
    {
        portid = qconf->rx_port_list[i];
    }

    while (!quit)
    {
        curtsc = rte_rdtsc();

        difftsc = curtsc - prevtsc;

        if (unlikely(difftsc > draintsc))
        {
            for (i = 0; i < qconf->num_rx_ports; i++)
            {
                portid = dst_ports[qconf->rx_port_list[i]];
                buffer = tx_buffer[portid];

                rte_eth_tx_buffer_flush(portid, 0, buffer);
            }

            prevtsc = curtsc;
        }

        // Read all packets from RX queue.
        for (i = 0; i < qconf->num_rx_ports; i++)
        {
            portid = qconf->rx_port_list[i];
            nb_rx = rte_eth_rx_burst(portid, 0, pckts_burst, MAX_PCKT_BURST);

            for (j = 0; j < nb_rx; j++)
            {
                pckt = pckts_burst[j];
                rte_prefetch0(rte_pktmbuf_mtod(pckt, void *));
                forwardpacket(pckt, portid);
            }
        }
    }
}

static int launch_lcore(__rte_unused void *tmp)
{
    pcktloop();
}

static void sign_hdl(int tmp)
{
    quit = 1;
}

int main(int argc, char **argv)
{
    // Initialiize result variables.
    struct dpdkc_ret ret =
    {
        .err_num = 0,
        .gen_msg = NULL,
        .port_id = -1,
        .rx_id = -1,
        .tx_id = -1,
        .data = NULL
    };

    // Initialize EAL and check.
    ret = dpdkc_eal_init(argc, argv);

    dpdkc_check_ret(&ret);

    // Retrieve number of arguments to adjust.
    int arg_adj = *((int *)ret.data);

    // Calculate difference in arguments due to EAL init.
    argc -= arg_adj;
    argv += arg_adj;

    // Setup signal.
    quit = 0;
    signal(SIGINT, sign_hdl);
    signal(SIGTERM, sign_hdl);

    // Parse application-specific arguments.
    struct cmdline cmd = {0};
    parsecmdline(&cmd, argc, argv);

    // Retrieve the amount of ethernet ports and check.
    ret = dpdkc_get_nb_ports();

    dpdkc_check_ret(&ret);

    nb_ports = *((unsigned short *)ret.data);

    // Check port pairs.
    ret = dpdkc_check_port_pairs();

    dpdkc_check_ret(&ret);

    // Make sure port mask is valid.
    ret = dpdkc_ports_are_valid();

    dpdkc_check_ret(&ret);

    // Reset destination ports.
    dpdkc_reset_dst_ports();

    // Populate our destination ports.
    dpdkc_populate_dst_ports();

    // Initialize the port and queue combination for each l-core.
    ret = dpdkc_ports_queues_mapping();

    dpdkc_check_ret(&ret);

    // Determine number of mbufs to have.
    ret = dpdkc_create_mbuf();

    dpdkc_check_ret(&ret);

    // Initialize each port.
    ret = dpdkc_ports_queues_init(cmd.promisc, 1, 1);

    // Check for error and fail with it if there is.
    dpdkc_check_ret(&ret);

    // Check for available ports.
    ret = dpdkc_ports_available();

    dpdkc_check_ret(&ret);

    // Check port link status for all ports.
    dpdkc_check_link_status();

    // Launch the application on each l-core.
    dpdkc_launch_and_run(launch_lcore);

    // Stop all ports.
    ret = dpdkc_port_stop_and_remove();

    dpdkc_check_ret(&ret);

    // Cleanup EAL.
    ret = dpdkc_eal_cleanup();

    dpdkc_check_ret(&ret);

    return 0;
}