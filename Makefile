
APP = simple_l2fwd
BUILDDIR=build
COMMONDIR=The-DPDK-Common
SRCDIR=src

COMMONOBJ=$(COMMONDIR)/objs/dpdk_common.o
COMMONSRC=$(COMMONDIR)/src

CMDLINEOBJ=cmdline.o
CMDLINESRC=cmdline.c

OBJS=$(COMMONOBJ) $(BUILDDIR)/$(CMDLINEOBJ)

SIMPLE_L2FWD := $(SRCDIR)/simple_l2fwd.c
DROPUDP8080 := $(SRCDIR)/dropudp8080.c

PKGCONF ?= pkg-config

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

all: shared
.PHONY: shared static
shared: $(BUILDDIR)/$(APP)-shared
	ln -sf $(APP)-shared $(BUILDDIR)/$(APP)
static: $(BUILDDIR)/$(APP)-static
	ln -sf $(APP)-static $(BUILDDIR)/$(APP)

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

build:
	@mkdir -p $@
commonbuild:
	$(MAKE) -C $(COMMONDIR)
cmdlinebuild: Makefile $(PC_FILE) | build
	$(CC) -I $(COMMONDIR)/$(SRCDIR) -c $(CFLAGS) -o $(BUILDDIR)/$(CMDLINEOBJ) $(LDFLAGS) $(LDFLAGS_SHARED) $(SRCDIR)/$(CMDLINESRC)
$(BUILDDIR)/$(APP)-shared: commonbuild cmdlinebuild $(OBJS) $(SIMPLE_L2FWD) $(DROPUDP8080) Makefile $(PC_FILE) | build
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(CFLAGS) $(SIMPLE_L2FWD) -o $@ $(LDFLAGS) $(OBJS) $(LDFLAGS_SHARED)
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(CFLAGS) $(DROPUDP8080) -o $@ $(LDFLAGS) $(OBJS) $(LDFLAGS_SHARED)
$(BUILDDIR)/$(APP)-static: commonbuild cmdlinebuild $(OBJS) $(SIMPLE_L2FWD) $(DROPUDP8080) Makefile $(PC_FILE) | build
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(CFLAGS) $(SIMPLE_L2FWD) -o $@ $(LDFLAGS) $(OBJS) $(LDFLAGS_STATIC)
	$(CC) -I $(COMMONDIR)/$(SRCDIR) $(CFLAGS) $(DROPUDP8080) -o $@ $(LDFLAGS) $(OBJS) $(LDFLAGS_STATIC)
.PHONY: clean
clean:
	rm -f $(BUILDDIR)/$(APP) $(BUILDDIR)/$(APP)-static $(BUILDDIR)/$(APP)-shared
	$(MAKE) -C $(COMMONDIR) clean
	test -d $(BUILDDIR) && rmdir -p $(BUILDDIR) || true