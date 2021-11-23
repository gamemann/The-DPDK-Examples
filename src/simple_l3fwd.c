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
#include <rte_ip.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include <arpa/inet.h>

#include "cmdline.h"

/* Helpful defines */
#ifndef htons
#define htons(o) cpu_to_be16(o)
#endif

#define ETH_P_IP 0x0800
#define ETH_P_8021Q	0x8100
#define PROTOCOL_UDP 0x11

//#define DEBUG

__u64 pckts_forwarded = 0;
__u64 pckts_dropped = 0;

/**
 * Reads a file in "<ip> <mac address>" format and inserts into the routing table.
 * 
 * @param file Path to file to open and scan.
 * @param route_tbl A pointer to the route hash table (please ensure to check the table pointer before passing).
 * 
 * @return The amount of routes added or -1 on error.
**/
static unsigned int scan_route_table_and_add(const char *file, struct rte_hash *route_tbl)
{
    // This represents the amount of routes we've added.
    unsigned int routes = 0;
    int i = 0;

    FILE *fp = fopen(file, "r");

    if (!fp)
    {
        return -1;
    }

    // Variables needed for looping through each line.
    char *line = NULL;
    ssize_t len;

    // Go through each line.
    while (getline(&line, &len, fp) != -1)
    {
        char ip[64];
        char dmac[64];

        // Increment I so we have an index.
        i++;

        // Represents the part of the data we've split.
        char *ptr = NULL;

        ptr = strtok(line, " ");

        // Check to see if we found a match.
        if (ptr == NULL)
        {
            printf("WARNING - Route #%d failed due to trying to pick IP address.\n", i);
            continue;
        }

        // Copy the first part to IP.
        strcpy(ip, ptr);

        // Move onto the next.
        ptr = strtok(NULL, " ");

        // Check.
        if (ptr == NULL)
        {
            printf("WARNING - Route #%d failed due to trying to pick MAC address.\n", i);

            continue;
        }

        // Copy MAC address.
        strcpy(dmac, ptr);

        // Convert IP address to unsigned 32-bit integer in host network byte order.
        struct in_addr ipaddr;

        // If inet_aton() returns 0, it failed.
        if (inet_aton(ip, &ipaddr) == 0)
        {
            printf("WARNING - Route #%d failed due to IP address not parsing properly (%s).\n", i, ip);

            continue;
        }

        // Now convert MAC address.
        struct rte_ether_addr dmacval[6];

        sscanf(dmac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &dmacval->addr_bytes[0], &dmacval->addr_bytes[1], &dmacval->addr_bytes[2], &dmacval->addr_bytes[3], &dmacval->addr_bytes[4], &dmacval->addr_bytes[5]);

#ifdef DEBUG
        printf("Inserting into route table %s (%u) => %hhx:%hhx:%hhx:%hhx:%hhx:%hhx.\n", ip, (__u32)ipaddr.s_addr, dmacval->addr_bytes[0], dmacval->addr_bytes[1], dmacval->addr_bytes[2], dmacval->addr_bytes[3], dmacval->addr_bytes[4], dmacval->addr_bytes[5]);
#endif

        // Now insert into the map, check, and increment routes if successful.
        int ret = rte_hash_add_key_data(route_tbl, &ipaddr, &dmacval);

        if (ret == 0)
        {
            routes++;
        }
        else
        {
            printf("WARNING - Route #%d failed due to map insert fail.\n", i);
        }
    }

    // Close the file.
    fclose(fp);

    return routes;
}

/**
 * Does lookup on hash map and forwards if need to be (otherwise drops).
 * 
 * @param pckt A pointer to the rte_mbuf container the packet data.
 * @param portid The port ID we're inspecting from.
 * @param route_tbl A pointer to the route hash table (struct rte_hash).
 * 
 * @return Void
**/
static void fwd_pckt(struct rte_mbuf *pckt, unsigned portid, struct rte_hash *route_tbl)
{
    // Data points to the start of packet data within the mbuf.
    void *data = pckt->buf_addr + pckt->data_off;

    // The offset.
    unsigned int offset = 0;

    // Initialize ethernet header.
    struct rte_ether_hdr *eth = data;

    offset += sizeof(struct rte_ether_hdr);

    // Make sure we're dealing with IPv4 or a VLAN.
    if (eth->ether_type != htons(ETH_P_IP) && eth->ether_type != htons(ETH_P_8021Q))
    {
        rte_pktmbuf_free(pckt);

        return;
    }

    // Handle VLAN.
    if (eth->ether_type == htons(ETH_P_8021Q))
    {
        // VLAN header length is four bytes, so increase offset by that amount.
        offset += 4;
    }

    // Initialize IPv4 header.
    struct rte_ipv4_hdr *iph = data + offset;

    // Perform lookup on route table.
    struct rte_ether_addr *dmac;

    int is_routable = rte_hash_lookup_data(route_tbl, &iph->dst_addr, (void **)&dmac);

    // If we find no match, drop the packet.
    if (is_routable < 0)
    {
        // Increment dropped packet counter.
        pckts_dropped++;

        // Free the packet's mbuf and return.
        rte_pktmbuf_free(pckt);

        return;
    }

    // Now copy the port we're going out from as the source MAC and the correct destination from the route lookup.
    rte_ether_addr_copy(&eth->dst_addr, &eth->src_addr);
    rte_ether_addr_copy(dmac, &eth->dst_addr);

#ifdef DEBUG
    printf("Packet forwarding from %hhx:%hhx:%hhx:%hhx:%hhx:%hhx => %hhx:%hhx:%hhx:%hhx:%hhx:%hhx.\n", eth->src_addr.addr_bytes[0], eth->src_addr.addr_bytes[1], eth->src_addr.addr_bytes[2], eth->src_addr.addr_bytes[3], eth->src_addr.addr_bytes[4], eth->src_addr.addr_bytes[5], eth->dst_addr.addr_bytes[0], eth->dst_addr.addr_bytes[1], eth->dst_addr.addr_bytes[2], eth->dst_addr.addr_bytes[3], eth->dst_addr.addr_bytes[4], eth->dst_addr.addr_bytes[5]);
#endif
    
    // Otherwise, forward packet.
    unsigned dst_port;
    struct rte_eth_dev_tx_buffer *buffer;

    // Retrieve what port we're going out of and TX buffer to use.
    dst_port = dst_ports[portid];
    buffer = tx_buffer[dst_port];
    
    rte_eth_tx_buffer(dst_port, 0, buffer, pckt);

    // Increment packets TX count.
    pckts_forwarded++;
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

    // Retreive route table.
    void *route_tbl = rte_hash_find_existing("route_table");

    if (route_tbl == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failed to find routing table on port %d.\n", port_id);
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

                // Lastly, forward the packet.
                fwd_pckt(pckt, port_id, route_tbl);
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
 * The stats thread handler.
 * 
 * @param tmp An unused variable.
 * 
 * @return Void
**/
void *hndl_stats(void *tmp)
{
    // Last updated variables.
    __u64 last_fwd = 0;
    __u64 last_drop = 0;

    // Run until program exits.
    while (!quit)
    {
        // Retrieve current PPS.
        __u64 fwd_pps = pckts_forwarded - last_fwd;
        __u64 drop_pps = pckts_dropped - last_drop;

        // Flush stdout and print stats.
        fflush(stdout);
        printf("\rForward => %llu. Drop => %llu.", fwd_pps, drop_pps);

        // Update last variables.
        last_fwd = pckts_forwarded;
        last_drop = pckts_dropped;

        // Sleep for a second to avoid unnecessary CPU cycles.
        sleep(1);
    }
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
    struct dpdkc_ret ret = dpdkc_ret_init();

    // Initialize EAL and check.
    ret = dpdkc_eal_init(argc, argv);

    dpdkc_check_ret(&ret);

    // Retrieve number of arguments to adjust.
    int arg_adj = (int)ret.data;

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

    nb_ports = (unsigned short)ret.data;

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

    struct ckey
    {
        __u32 srcip;
        __u16 srcport;
        __u32 dstip;
        __u16 dstport;
    } ckey;

    // Create hash table for route lookups.
    struct rte_hash_parameters hparams =
    {
        .name = "route_table",
        .entries = 1024,
        .hash_func = rte_jhash,
        .key_len = sizeof(__u32),
        .reserved = 0,
        .socket_id = rte_socket_id()
    };

    void *route_tbl = rte_hash_create(&hparams);

    if (route_tbl == NULL)
    {
        rte_exit(EXIT_FAILURE, "Failed to create hash table.\n");
    }

    // Now scan the route table and insert into the hash map.
    unsigned int routes = scan_route_table_and_add("/etc/l3fwd/routes.txt", route_tbl);

    if (routes < 0)
    {
        printf("WARNING - Did not add any routes due to error opening routes file => /etc/l3fwd/routes.txt.\n");
    }
    else
    {
        printf("Added %u routes to table!\n", routes);
    }

    // If stats is enabled, create a separate thread that flushes stdout and prints stats.
    if (cmd.stats)
    {
        pthread_t pid;

        pthread_create(&pid, NULL, hndl_stats, NULL);
    }

    // Launch the application on each l-core.
    dpdkc_launch_and_run(launch_lcore);

    // Stop all ports.
    ret = dpdkc_port_stop_and_remove();

    dpdkc_check_ret(&ret);

    // Cleanup EAL.
    ret = dpdkc_eal_cleanup();

    dpdkc_check_ret(&ret);

    printf("Total Packets Forwarded => %llu.\nTotal Packets Dropped => %llu.\n\n", pckts_forwarded, pckts_dropped);

    return 0;
}