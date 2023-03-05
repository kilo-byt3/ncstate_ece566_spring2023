include ../Makefile.defs

SRC_DIR=/ece566/wolfbench/SimpleTests
INSTALL_DIR=/ece566/projects/p2/C++/p2-test/install/bin

vpath %.c $(SRC_DIR)
vpath %.cpp $(SRC_DIR)

programs = hello 

exes = $(addsuffix $(EXTRA_SUFFIX),$(programs))

.PHONY: all install

all: $(exes)

install: all $(addsuffix -install,$(exes))

test: all $(addsuffix -test,$(exes))

ftest: all $(addsuffix -ftest,$(exes))

profile: all $(addsuffix -profile,$(exes))

compare: 

include /ece566/wolfbench/Makefile.single
include ../Makefile.config
