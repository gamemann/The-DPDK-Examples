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

    if (qconf->numrxport == 0)
    {
        RTE_LOG(INFO, USER1, "lcore %u has nothing to do.\n", lcore_id);

        return;
    }

    RTE_LOG(INFO, USER1, "Looping with lcore %u.\n", lcore_id);

    for (i = 0; i < qconf->numrxport; i++)
    {
        portid = qconf->rxportlist[i];
    }

    while (!quit)
    {
        curtsc = rte_rdtsc();

        difftsc = curtsc - prevtsc;

        if (unlikely(difftsc > draintsc))
        {
            for (i = 0; i < qconf->numrxport; i++)
            {
                portid = dst_ports[qconf->rxportlist[i]];
                buffer = tx_buffer[portid];

                rte_eth_tx_buffer_flush(portid, 0, buffer);
            }

            prevtsc = curtsc;
        }

        // Read all packets from RX queue.
        for (i = 0; i < qconf->numrxport; i++)
        {
            portid = qconf->rxportlist[i];
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
    int ret = -1;
    struct dpdkc_error cret =
    {
        .err_num = 0,
        .gen_msg = NULL,
        .port_id = -1,
        .rx_id = -1,
        .tx_id = -1
    };

    if ((ret = dpdkc_eal_init(argc, argv)) < 0)
    {
        rte_exit(EXIT_FAILURE, "Failed to initialize EAL. Error => %s (%d).\n", strerror(-ret), ret);
    }

    // Calculate difference in arguments due to EAL init.
    argc -= ret;
    argv += ret;

    // Setup signal.
    quit = 0;
    signal(SIGINT, sign_hdl);
    signal(SIGTERM, sign_hdl);

    // Parse application-specific arguments.
    struct cmdline cmd = {0};
    parsecmdline(&cmd, argc, argv);

    // Retrieve the amount of ethernet ports and check.
    if ((nb_ports = dpdkc_get_nb_ports()) == 0)
    {
        rte_exit(EXIT_FAILURE, "No ethernet ports available.\n");
    }

    // Check port pairs.
    if ((ret = dpdkc_check_port_pairs()) < 0)
    {
        rte_exit(EXIT_FAILURE, "Port pairs are invalid.\n");
    }

    // Make sure port mask is valid.
    if ((ret = dpdkc_ports_are_valid()) < 0)
    {
        rte_exit(EXIT_FAILURE, "Invalid port mask. Try 0x%x.\n", (1 << nb_ports) - 1);
    }

    // Reset destination ports.
    dpdkc_reset_dst_ports();

    // Populate our destination ports.
    dpdkc_populate_dst_ports();

    // Initialize the port and queue combination for each l-core.
    if ((ret = dpdkc_ports_queues_mapping()) < 0)
    {
        rte_exit(EXIT_FAILURE, "Not enough cores to support l-cores.\n");
    }

    // Determine number of mbufs to have.
    if ((ret = dpdkc_create_mbuf()) < 0)
    {
        rte_exit(EXIT_FAILURE, "Failed to allocate packet's mbuf. Error => %s (%d).\n", strerror(-ret), ret);
    }

    // Initialize each port.
    cret = dpdkc_ports_queues_init(cmd.promisc, 1, 1);

    // Check for error and fail with it if there is.
    dpdkc_check_error(&cret);

    // Check for available ports.
    if (!dpdkc_ports_available())
    {
        rte_exit(EXIT_FAILURE, "No available ports found. Please set port mask.\n");
    }

    // Check port link status for all ports.
    dpdkc_check_link_status();

    // Launch the application on each l-core.
    dpdkc_launch_and_run(launch_lcore);

    // Stop all ports.
    if ((ret = dpdkc_port_stop_and_remove()) != 0)
    {
        rte_exit(EXIT_FAILURE, "Failed to stop ports. Error => %s (%d).\n", strerror(-ret), ret);
    }

    // Cleanup EAL.
    if ((ret = dpdkc_eal_cleanup()) != 0)
    {
        rte_exit(EXIT_FAILURE, "Failed to cleanup EAL. Error => %s (%d).\n", strerror(-ret), ret);
    }

    return 0;
}