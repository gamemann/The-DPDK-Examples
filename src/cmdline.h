#ifndef CMDLINE_HEADER
#define CMDLINE_HEADER

#include <linux/types.h>

struct cmdline
{
    __u16 queues;
    unsigned int promisc : 1;
    unsigned int stats : 1;
};

int parsecmdline(struct cmdline *cmd, int argc, char **argv);
#endif