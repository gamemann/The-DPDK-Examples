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

### Simple Forwarding Program
*Not created yet.*

## Credits
* [Christian Deacon](https://github.com/gamemann)