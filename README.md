# The DPDK Examples (WIP)
## Description
A small repository I will be using to store my progress and test programs from [the DPDK](https://www.dpdk.org/), a kernel bypass library very useful for fast packet processing.

This repository uses my DPDK Common [project](https://github.com/gamemann/The-DPDK-Common) in an effort to make things simpler.

**WARNING** - This project is still a work in-progress. Files will be incomplete.

## Requirements
* [The DPDK](https://dpdk.org) - Intel's Data Plane Development Kit which acts as a kernel bypass library which allows for fast network packet processing (one of the fastest libraries out there for packet processing).
* [The DPDK Common](https://github.com/gamemann/The-DPDK-Common) - A project written by me aimed to make my DPDK projects simpler to setup/run.

## Building The DPDK
If you want to build the DPDK using default options, the following should work assuming you have the requirements such as `ninja` and `meson`.

```
git clone https://github.com/DPDK/dpdk.git
cd dpdk/
meson build
cd build
ninja
sudo ninja install
sudo ldconfig
```

All needed header files from the DPDK will be stored inside of `/usr/local/include/`.

You may receive `ninja` and `meson` using the following.

```
sudo apt update
sudo apt install python3 python3-pip
sudo pip3 install meson # Pip3 is used because 'apt' has an outdated version of Meson usually.
sudo apt install ninja-build
```

## Building The Source Files
You may use `git` and `make` to build the source files inside of this repository.

```
git clone --recursive https://github.com/gamemann/The-DPDK-Examples.git
cd The-DPDK-Examples/
make
```

Executables will be built inside of the `build/` directory by default.

## EAL Parameters
All DPDK applications in this repository supports DPDK's EAL paramters. These may be found [here](http://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html).

This is useful for specifying the amount of l-cores and ports to configure for example.

## Examples
### Drop UDP Port 8080 (Tested And Working)
In this DPDK application, any packets arriving on UDP destination port 8080 will be dropped. Otherwise, if the packet's ethernet header type is IPv4 or VLAN, it will swap the source/destination MAC and IP addresses along with the UDP source/destination ports then send the packet out the TX path (basically forwarding the packet from where it came).

In additional to EAL parameters, the following is available specifically for this application.

```
-p --portmask => The port mask to configure (e.g. 0xFFFF).
-P --portmap => The port map to configure (in '(x, y),(b,z)' format).
-q --queues => The amount of RX and TX queues to setup per port (default and recommended value is 1).
-x --promisc => Whether to enable promiscuous on all enabled ports.
-s --stats => If specified, will print real-time packet counter stats to stdout.
```

Here's an example:

```
./dropudp8080 -l 0-1 -n 1 -- -q 1 -p 0xff -s
```

### Simple Layer 3 Forward (Tested And Working)
In this DPDK application, a simple routing hash table is created with the key being the destination IP address and the value being the MAC address to forward to.

Routes are read from the `/etc/l3fwd/routes.txt` file in the following format.

```
<ip address> <mac address in xx:xx:xx:xx:xx:xx>
```

The following is an example.

```
10.50.0.4 ae:21:14:4b:3a:6d
10.50.0.5 d6:45:f3:b1:a4:3d
```

When a packet is processed, we ensure it is an IPv4 or VLAN packet (we offset the packet data by four bytes in this case so we can process the rest of the packet without issues). Afterwards, we perform a lookup with the destination IP being the key on the route hash table. If the lookup is successful, the source MAC address is replaced with the destination MAC address (packets will be going out the same port they arrive since we create a TX buffer and queue) and the destination MAC address is replaced with the MAC address the IP was assigned to from the routes file mentioned above. Otherwise, the packet is dropped and the packet dropped counter is incremented.

In additional to EAL parameters, the following is available specifically for this application.

```
-p --portmask => The port mask to configure (e.g. 0xFFFF).
-P --portmap => The port map to configure (in '(x, y),(b,z)' format).
-q --queues => The amount of RX and TX queues to setup per port (default and recommended value is 1).
-x --promisc => Whether to enable promiscuous on all enabled ports.
-s --stats => If specified, will print real-time packet counter stats to stdout.
```

Here's an example:

```
./simple_l3fwd -l 0-1 -n 1 -- -q 1 -p 0xff -s
```

### Rate Limit (Tested And Working)
In this application, if a source IP equals or exceeds the packets per second or bytes per second specified in the command line, the packets are dropped. Otherwise, the ethernet and IP addresses are swapped along with the TCP/UDP ports and the packet is forwarded back out the TX path.

Packet stats are also included with the `-s` flag.

The following command line options are supported.

```
-p --portmask => The port mask to configure (e.g. 0xFFFF).
-P --portmap => The port map to configure (in '(x, y),(b,z)' format).
-q --queues => The amount of RX and TX queues to setup per port (default and recommended value is 1).
-x --promisc => Whether to enable promiscuous on all enabled ports.
-s --stats => If specified, will print real-time packet counter stats to stdout.
--pps => The packets per second to limit each source IP to.
--bps => The bytes per second to limit each source IP to.
```

Here's an example:

```
./ratelimit -l 0-1 -n 1 -- -q 1 -p 0xff -s
```

### Least Recently Used Test (Tested And Working)
This is a small application that implements a manual LRU method for hash tables. For a while I've been trying to get LRU maps to work from [these](http://code.dpdk.org/dpdk/latest/source/lib/table) libraries. However, I had zero success in actually getting the map initialized.

Therefore, I decided to keep using [these](http://code.dpdk.org/dpdk/latest/source/lib/hash) libaries instead and implement my own LRU fuctionality. I basically use the `rte_hash_get_key_with_position()` function to retrieve the key to delete. However, it appears the new key is inserted at the index that was deleted so you have to keep incrementing the position value up to the max entries of the table. With that said, once the position value exceeds the maximum table entries, you need to set it back to 0.

No command line options are needed, but EAL parameters are still supported. Though, they won't make a difference.

Here's an example:

```
./ratelimit
```

## Credits
* [Christian Deacon](https://github.com/gamemann)