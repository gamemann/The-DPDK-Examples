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

    dst_port = dstports[portid];
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
                portid = dstports[qconf->rxportlist[i]];
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

static int launchlcore(__rte_unused void *tmp)
{
    pcktloop();
}

static void signhdl(int tmp)
{
    quit = 1;
}

int main(int argc, char **argv)
{
    struct lcore_queue_conf *qconf;
    int ret;
    __u16 nbports;
    __u16 nbportsavailable = 0;
    __u16 portid, lastport;
    unsigned int lcoreid, rxlcoreid;
    unsigned nbportsinmask = 0;
    unsigned int nblcores = 0;
    unsigned int nbmbufs;

    // Initialize.
    ret = rte_eal_init(argc, argv);

    if (ret < 0)
    {
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments.\n");
    }

    argc -= ret;
    argv += ret;

    quit = 0;

    signal(SIGINT, signhdl);
    signal(SIGTERM, signhdl);

    // Application-specify arguments.
    struct cmdline cmd = {0};
    parsecmdline(&cmd, argc, argv);

    // Retrieve the amount of ethernet ports and check.
    nbports = rte_eth_dev_count_avail();

    if (nbports == 0)
    {
        rte_exit(EXIT_FAILURE, "No ethernet ports available.\n");
    }

    // Check port pairs.
    if (port_pair_params != NULL)
    {
        if (check_port_pair_config() < 0)
        {
            rte_exit(EXIT_FAILURE, "Port map config is invalid.\n");
        }
    }

    // Make sure port mask is valid.
    if (enabled_portmask & ~((1 << nbports) - 1))
    {
        rte_exit(EXIT_FAILURE, "Invalid port mask. Try 0x%x.\n", (1 << nbports) - 1);
    }

    // Reset destination ports.
    for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
    {
        dstports[portid] = 0;
    }

    lastport = 0;

    // Populate our destination ports.
    if (port_pair_params != NULL)
    {
        __u16 index, p;

        for (index = 0; index < (nb_port_pair_params << 1); index++)
        {
            p = index & 1;
            portid = port_pair_params[index >> 1].port[p];
            dstports[portid] = port_pair_params[index >> 1].port[p ^ 1];
        }
    }
    else
    {
        RTE_ETH_FOREACH_DEV(portid)
        {
            if ((enabled_portmask & (1 << portid)) == 0)
            {
                continue;
            }

            if (nbportsinmask % 2)
            {
                dstports[portid] = lastport;
                dstports[lastport] = portid;
            }
            else
            {
                lastport = portid;
            }

            nbportsinmask++;
        }

        if (nbportsinmask % 2)
        {
            fprintf(stdout, "WARNING - Odd number of ports in port mask.\n");
            dstports[lastport] = lastport;
        }
    }

    rxlcoreid = 0;
    qconf = NULL;

    // Initialize the port and queue combination for each l-core.
    RTE_ETH_FOREACH_DEV(portid)
    {
        // Skip any ports not available.
        if ((enabled_portmask & (1 << portid)) == 0)
        {
            continue;
        }

        // Retrieve the l-core ID.
        while (rte_lcore_is_enabled(rxlcoreid) == 0 || lcore_queue_conf[rxlcoreid].numrxport == rxqueuepl)
        {
            rxlcoreid++;

            if (rxlcoreid >= RTE_MAX_LCORE)
            {
                rte_exit(EXIT_FAILURE, "Not enough cores to support the amount of RX queues/ports.\n");
            }
        }

        if (qconf != &lcore_queue_conf[rxlcoreid])
        {
            qconf = &lcore_queue_conf[rxlcoreid];
            nblcores++;
        }

        qconf->rxportlist[qconf->numrxport] = portid;
        qconf->numrxport++;

        fprintf(stdout, "Setting up l-core #%u with RX port %u and TX port %u.\n", rxlcoreid, portid, dstports[portid]);
    }

    // Determine number of mbufs to have.
    nbmbufs = RTE_MAX(nbports * (nb_rxd + nb_txd + nblcores & MEMPOOL_CACHE_SIZE), 8192U);

    // Create mbuf pool.
    pcktmbuf_pool = rte_pktmbuf_pool_create("pckt_pool", nbmbufs, MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (pcktmbuf_pool == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failure to create mbuf pool.\n");
    }

    // Initialize each port.
    RTE_ETH_FOREACH_DEV(portid)
    {
        // Initialize queue/port conifgs and device info.
        struct rte_eth_rxconf rxqconf;
        struct rte_eth_txconf txqconf;
        struct rte_eth_conf localportconf = port_conf;
        struct rte_eth_dev_info devinfo;

        // Skip any ports not available.
        if ((enabled_portmask & (1 << portid)) == 0)
        {
            fprintf(stdout, "Skipping port #%u initialize due to it being disabled.\n", portid);
            
            continue;
        }

        nbportsavailable++;

        // Initialize the port itself.
        fprintf(stdout, "Initializing port #%u...\n", portid);
        fflush(stdout);

        // Attempt to receive device information for this specific port and check.
        ret = rte_eth_dev_info_get(portid, &devinfo);

        if (ret != 0)
        {
            rte_exit(EXIT_FAILURE, "Error getting device information on port ID %u. Error => %s.\n", portid, strerror(-ret));
        }

        // Check for TX mbuf fast free support on this specific device.
        if (devinfo.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
        {
            localportconf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
        }

        // Configure the queue for this port (we only initialize one RX and TX queue per port).
        ret = rte_eth_dev_configure(portid, 1, 1, &localportconf);

        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Cannot configure device for port #%u. Error => %s (%d).\n", portid, strerror(-ret), ret);
        }

        // Retrieve MAC address of device and store in array.
        ret = rte_eth_macaddr_get(portid, &portseth[portid]);

        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Failed to retrieve MAC address for port #%u. Error => %s (%d).\n", portid, strerror(-ret), ret);
        }

        // Initialize the RX queue.
        fflush(stdout);
        rxqconf = devinfo.default_rxconf;
        rxqconf.offloads = localportconf.rxmode.offloads;

        // Setup the RX queue and check.
        ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd, rte_eth_dev_socket_id(portid), &rxqconf, pcktmbuf_pool);

        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Failed to setup RX queue for port #%u. Error => %s (%d).\n", portid, strerror(-ret), ret);
        }

        // Initialize the TX queue.
        fflush(stdout);
        txqconf = devinfo.default_txconf;
        txqconf.offloads = localportconf.txmode.offloads;

        // Setup the TX queue and check.
        ret = rte_eth_tx_queue_setup(portid, 0, nb_txd, rte_eth_dev_socket_id(portid), &txqconf);

        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Failed to setup TX queue for port #%u. Error => %s (%d).\n", portid, strerror(-ret), ret);
        }

        // Initialize TX buffers.
        tx_buffer[portid] = rte_zmalloc_socket("tx_buffer", RTE_ETH_TX_BUFFER_SIZE(MAX_PCKT_BURST), 0, rte_eth_dev_socket_id(portid));

        if (tx_buffer[portid] == NULL)
        {
            rte_exit(EXIT_FAILURE, "Failed to allocate TX buffer for port #%u.\n", portid);
        }

        rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PCKT_BURST);

        ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid], rte_eth_tx_buffer_count_callback, NULL);

        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Failed to set callback for TX buffer on port #%u.\n", portid);
        }

        // We'll want to disable ptype parsing.
        ret = rte_eth_dev_set_ptypes(portid, RTE_PTYPE_UNKNOWN, NULL, 0);

        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Failed to disable PType parsing for port #%u.\n", portid);
        }

        // Start the device itself.
        ret = rte_eth_dev_start(portid);

        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Failed to start Ethernet device for port #%u.\n", portid);
        }

        // Check for promiscuous mode.
        if (cmd.promisc)
        {
            ret = rte_eth_promiscuous_enable(portid);

            if (ret < 0)
            {
                rte_exit(EXIT_FAILURE, "Failed to enable promiscuous mode for port #%u. Error => %s (%d).\n", portid, strerror(-ret), ret);
            }
        }

        fprintf(stdout, "Port #%u setup successfully. MAC Address => " RTE_ETHER_ADDR_PRT_FMT ".\n", portid, RTE_ETHER_ADDR_BYTES(&portseth[portid]));
    }

    // Check for available ports.
    if (!nbportsavailable)
    {
        rte_exit(EXIT_FAILURE, "No available ports found. Please set port mask.\n");
    }

    // Check port link status for all ports.
    check_all_ports_link_status(enabled_portmask);

    ret = 0;


    // Launch the application on each l-core.
    rte_eal_mp_remote_launch(launchlcore, NULL, CALL_MAIN);

    RTE_LCORE_FOREACH_WORKER(lcoreid)
    {
        if (rte_eal_wait_lcore(lcoreid) < 0)
        {
            ret = -1;

            break;
        }
    }

    // Stop all ports.
    RTE_ETH_FOREACH_DEV(portid)
    {
        // Skip disabled ports.
        if ((enabled_portmask & (1 << portid)) == 0)
        {
            continue;
        }

        fprintf(stdout, "Closing port #%u.\n", portid);

        // Stop the port and check.
        ret = rte_eth_dev_stop(portid);

        if (ret != 0)
        {
            fprintf(stderr, "Failed to close port #%u. Error => %s (%d).\n", portid, strerror(-ret), ret);
        }

        // Finally, close the port.
        rte_eth_dev_close(portid);
    }

    // Cleanup EAL.
    rte_eal_cleanup();

    return ret;
}