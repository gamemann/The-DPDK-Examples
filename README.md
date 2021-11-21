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

## Credits
* [Christian Deacon](https://github.com/gamemann)