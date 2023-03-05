include ../Makefile.defs

SRC_DIR=/ece566/wolfbench/P1Tests
INSTALL_DIR=/ece566/projects/p2/C++/p2-test/install/bin

vpath %.c $(SRC_DIR)
vpath %.cpp $(SRC_DIR)
vpath %.p1 $(SRC_DIR)

ece466 = \
	test_00 \
	test_01 \
	test_02 \
	test_03 \
	test_04 \
        test_05 \
	test_06 \
	test_07 \
	test_08 \
	test_09

ece566 = \
        test_10 \
        test_11 \
        test_12 \
        test_13 \
        test_14

#programs = $(ece466)
programs = $(ece466) $(ece566)

.PHONY: all install

all: $(addsuffix -build,$(programs))

install: all $(addsuffix -install,$(programs))

test: all $(addsuffix -test,$(programs))
	/ece566/wolfbench/summarize.py $(addsuffix -test,$(programs))

cleanall:
	@rm -Rf $(programs) $(addsuffix -test,$(programs)) $(addsuffix .s,$(programs))

include /ece566/wolfbench/Makefile.P1
include ../Makefile.config

