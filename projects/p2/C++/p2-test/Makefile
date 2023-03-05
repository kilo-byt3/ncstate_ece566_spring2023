
DIRS = SimpleTests Benchmarks

ifdef QUIET
VERB:=-s
else
VERB:=
endif

.PHONY: all install clean test $(addsuffix -install,$(DIRS)) $(addsuffix -clean,$(DIRS)) $(addsuffix -test,$(DIRS)) $(DIRS) stats compare

all: SimpleTests Benchmarks

install: $(addsuffix -install,$(DIRS))

test: $(addsuffix -test,$(DIRS))

ftest: $(addsuffix -ftest,$(DIRS))

clean: $(addsuffix -clean,$(DIRS))

stats: all
	/ece566/wolfbench/stats.py `find . -name *.stats`

profile: $(addsuffix -profile,$(DIRS))

compare: $(addsuffix -compare,$(DIRS))

$(DIRS):
	make $(VERB) -C $@ all

$(addsuffix -install,$(DIRS)):
	make $(VERB) -C $(subst -install,,$@) install

$(addsuffix -clean,$(DIRS)):
	make $(VERB) -C $(subst -clean,,$@) cleanall

$(addsuffix -test,$(DIRS)):
	@make $(VERB) -C $(subst -test,,$@) test

$(addsuffix -ftest,$(DIRS)):
	make $(VERB) -C $(subst -ftest,,$@) ftest

$(addsuffix -compare,$(DIRS)):
	@make $(VERB) -C $(subst -compare,,$@) compare

$(addsuffix -profile,$(DIRS)):
	make $(VERB) -C $(subst -profile,,$@) profile
