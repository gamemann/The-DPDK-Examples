BUILDDIR=build
COMMONDIR=The-DPDK-Common
SRCDIR=src

COMMONOBJ=$(COMMONDIR)/objs/static/dpdk_common.o
COMMONSRC=$(COMMONDIR)/src

CMDLINEOBJ=cmdline.o
CMDLINESRC=cmdline.c

OBJS=$(COMMONOBJ) $(BUILDDIR)/$(CMDLINEOBJ)

SIMPLEL3FWDSRC := simple_l3fwd.c
SIMPLEL3FWDOUT := simple_l3fwd

DROPUDP8080SRC := dropudp8080.c
DROPUDP8080OUT := dropudp8080

RATELIMITSRC := ratelimit.c
RATELIMITOUT := ratelimit

LRUTESTSRC := lrutest.c
LRUTESTOUT := lruout

LRUTABLETESTSRC := lru_table_test.c
LRUTABLETESTOUT := lru_table_test

GLOBALFLAGS := -pthread

PKGCONF ?= pkg-config

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
# Add flag to allow experimental API as l2fwd uses rte_ethdev_set_ptype API
CFLAGS += -DALLOW_EXPERIMENTAL_API
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC = $(shell $(PKGCONF) --static --libs libdpdk)

ifeq ($(MAKECMDGOALS),static)
# check for broken pkg-config
ifeq ($(shell echo $(LDFLAGS_STATIC) | grep 'whole-archive.*l:lib.*no-whole-archive'),)
$(warning "pkg-config output list does not contain drivers between 'whole-archive'/'no-whole-archive' flags.")
$(error "Cannot generate statically-linked binaries with this version of pkg-config")
endif
endif

all: main
build:
	@mkdir -p $(BUILDDIR)
commonbuild:
	$(MAKE) -C $(COMMONDIR)
cmdlinebuild: Makefile $(PC_FILE) | build
	$(CC) -I $(COMMONDIR)/$(SRCDIR) -c $(CFLAGS) -o $(BUILDDIR)/$(CMDLINEOBJ) $(LDFLAGS) $(LDFLAGS_STATIC) $(SRCDIR)/$(CMDLINESRC)
main: commonbuild cmdlinebuild $(OBJS) Makefile $(PC_FILE) | build tbl
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(GLOBALFLAGS) $(CFLAGS) $(SRCDIR)/$(SIMPLEL3FWDSRC) -o $(BUILDDIR)/$(SIMPLEL3FWDOUT) $(LDFLAGS) $(OBJS) $(LDFLAGS_STATIC)
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(GLOBALFLAGS) $(CFLAGS) $(SRCDIR)/$(DROPUDP8080SRC) -o $(BUILDDIR)/$(DROPUDP8080OUT) $(LDFLAGS) $(OBJS) $(LDFLAGS_STATIC)
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(GLOBALFLAGS) $(CFLAGS) $(SRCDIR)/$(RATELIMITSRC) -o $(BUILDDIR)/$(RATELIMITOUT) $(LDFLAGS) $(OBJS) $(LDFLAGS_STATIC)
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(GLOBALFLAGS) $(CFLAGS) $(SRCDIR)/$(LRUTESTSRC) -o $(BUILDDIR)/$(LRUTESTOUT) $(LDFLAGS) $(OBJS) $(LDFLAGS_STATIC)

tbl:
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(GLOBALFLAGS) $(CFLAGS) $(SRCDIR)/$(LRUTABLETESTSRC) -o $(BUILDDIR)/$(LRUTABLETESTOUT) $(LDFLAGS) $(OBJS) $(LDFLAGS_STATIC)

install:
	cp $(BUILDDIR)/$(SIMPLEL3FWDOUT) /usr/bin/$(SIMPLEL3FWDOUT)
	cp $(BUILDDIR)/$(DROPUDP8080OUT) /usr/bin/$(DROPUDP8080OUT)
	cp $(BUILDDIR)/$(RATELIMITOUT) /usr/bin/$(RATELIMITOUT)
clean:
	rm -f $(BUILDDIR)/*
	$(MAKE) -C $(COMMONDIR) clean
.PHONY: main clean