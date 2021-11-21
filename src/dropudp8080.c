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
#include <rte_ip.h>
#include <rte_udp.h>

#include "cmdline.h"

/* Helpful defines */
#ifndef htons
#define htons(o) cpu_to_be16(o)
#endif

#define ETHTYPE_IPV4 0x0800
#define PROTOCOL_UDP 0x11

/**
 * Swaps the source and destination ethernet MAC addresses.
 * 
 * @param eth A pointer to the ethernet header (struct rte_ether_hdr).
 * 
 * @return Void
**/
static void swap_eth(struct rte_ether_hdr *eth)
{
    struct rte_ether_addr tmp;
    rte_ether_addr_copy(&eth->src_addr, &tmp);

    rte_ether_addr_copy(&eth->dst_addr, &eth->src_addr);
    rte_ether_addr_copy(&tmp, &eth->dst_addr);
}

/**
 * Swaps the source and destination IP addresses.
 * 
 * @param iph A pointer to the IPv4 header (struct rte_ipv4_hdr).
 * 
 * @return Void
**/
static void swap_iph(struct rte_ipv4_hdr *iph)
{
    rte_be32_t tmp;
    memcpy(&tmp, &iph->src_addr, sizeof(tmp));

    memcpy(&iph->src_addr, &iph->dst_addr, sizeof(iph->src_addr));
    memcpy(&iph->dst_addr, &tmp, sizeof(iph->dst_addr));
}

/**
 * Swaps the source and destination UDP ports.
 * 
 * @param udph A pointer to the UDP header (struct rte_udp_hdr).
 * 
 * @return Void
**/
static void swap_udph(struct rte_udp_hdr *udph)
{
    rte_be16_t tmp;
    memcpy(&tmp, &udph->src_port, sizeof(tmp));

    memcpy(&udph->src_port, &udph->dst_port, sizeof(udph->src_port));
    memcpy(&udph->dst_port, &tmp, sizeof(udph->dst_port));
}

/**
 * Inspects a packet and checks against UDP destination port 8080.
 * 
 * @param pckt A pointer to the rte_mbuf container the packet data.
 * @param portid The port ID we're inspecting from.
 * 
 * @return Void
**/
static void inspect_pckt(struct rte_mbuf *pckt, unsigned portid)
{
    // Data points to the start of packet data within the mbuf.
    void *data = pckt->buf_addr;

    // Initialize ethernet header.
    struct rte_ether_hdr *eth = data;

    // Make sure we're dealing with IPv4.
    if (eth->ether_type != htons(ETHTYPE_IPV4))
    {
        return;
    }

    // Initialize IPv4 header.
    struct rte_ipv4_hdr *iph = data + sizeof(struct rte_ether_hdr);

    // Check to make sure we're dealing with UDP.
    if (iph->next_proto_id != PROTOCOL_UDP)
    {
        return;
    }

    // Initialize UDP header.
    struct rte_udp_hdr *udph = data + sizeof(struct rte_ether_hdr) + (iph->ihl * 4);

    // Check destination port.
    if (udph->dst_port == htons(8080))
    {
        // Drop packet.
        return;
    }

    // Swap MAC addresses.
    swap_eth(eth);

    // Swap IP addresses.
    swap_iph(iph);

    // Swap UDP ports.
    swap_udph(udph);

    // Recalculate IP header checksum.
    iph->hdr_checksum = 0;
    rte_ipv4_cksum(iph);

    // Recalulate UDP header checksum.
    udph->dgram_cksum = 0;
    rte_ipv4_udptcp_cksum(iph, udph);

    // Otherwise, forward packet.
    unsigned dst_port;
    struct rte_eth_dev_tx_buffer *buffer;

    // Retrieve what port we're going out of and TX buffer to use.
    dst_port = dst_ports[portid];
    buffer = tx_buffer[dst_port];
    
    rte_eth_tx_buffer(dst_port, 0, buffer, pckt);
}

/**
 * Called on all l-cores and retrieves all packets to that RX queue.
 * 
 * @return Void
**/
static void pckt_loop(void)
{
    // An array of packets witin burst.
    struct rte_mbuf *pckts_burst[MAX_PCKT_BURST];

    // Single mbuf we'll use to inspect.
    struct rte_mbuf *pckt;

    // Retrieve the l-core ID.
    unsigned lcore_id = rte_lcore_id();

    // Iteration variables.
    unsigned i;
    unsigned j;

    // the port ID and number of packets from RX queue.
    unsigned port_id;
    unsigned nb_rx;

    // The specific RX queue config for the l-core.
    struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];

    // For TX draining.
    const __u64 draintsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

    // Pointer to TX buffer.
    struct rte_eth_dev_tx_buffer *buffer;

    // Create timer variables.
    __u64 prevtsc = 0;
    __u64 difftsc;
    __u64 curtsc;

    // If we have no RX ports under this l-core, return because the l-core has nothing else to do.
    if (qconf->num_rx_ports == 0)
    {
        RTE_LOG(INFO, USER1, "lcore %u has nothing to do.\n", lcore_id);

        return;
    }

    // Log message.
    RTE_LOG(INFO, USER1, "Looping with lcore %u.\n", lcore_id);

    // 
    for (i = 0; i < qconf->num_rx_ports; i++)
    {
        port_id = qconf->rx_port_list[i];
    }

    // Create while loop relying on quit variable.
    while (!quit)
    {
        // Get current timestamp.
        curtsc = rte_rdtsc();

        // Calculate the difference.
        difftsc = curtsc - prevtsc;

        // Check if we need to flush the TX buffer.
        if (unlikely(difftsc > draintsc))
        {
            // Loop through all RX ports.
            for (i = 0; i < qconf->num_rx_ports; i++)
            {
                // Retrieve correct port_id and buffer.
                port_id = dst_ports[qconf->rx_port_list[i]];
                buffer = tx_buffer[port_id];

                // Flush buffer.
                rte_eth_tx_buffer_flush(port_id, 0, buffer);
            }

            // Assign prevtsc.
            prevtsc = curtsc;
        }

        // Read all packets from RX queue.
        for (i = 0; i < qconf->num_rx_ports; i++)
        {
            // Retrieve correct port ID.
            port_id = qconf->rx_port_list[i];

            // Burst RX which will assign nb_rx to the amount of packets we have from the RX queue.
            nb_rx = rte_eth_rx_burst(port_id, 0, pckts_burst, MAX_PCKT_BURST);

            // Loop through the amount of packets we have from the RX queue.
            for (j = 0; j < nb_rx; j++)
            {
                // Assign the individual packet mbuf.
                pckt = pckts_burst[j];

                // Prefetch the packet.
                rte_prefetch0(rte_pktmbuf_mtod(pckt, void *));

                // Lastly, inspect the packet.
                inspect_pckt(pckt, port_id);
            }
        }
    }
}

/**
 * Called when an l-core is started.
 * 
 * @param tmp An unused variable.
 * 
 * @return Void
**/
static int launch_lcore(__rte_unused void *tmp)
{
    pckt_loop();
}

/**
 * The signal callback/handler.
 * 
 * @param tmp An unused variable.
 * 
 * @return Void
**/
static void sign_hdl(int tmp)
{
    quit = 1;
}

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

    // Initialize mbuf pool.
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