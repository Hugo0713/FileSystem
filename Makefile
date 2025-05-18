SUB_DIR = ./disk ./fs

all:
	@for n in $(SUB_DIR); do $(MAKE) -C $$n || exit 1; done

test:
	@for n in $(SUB_DIR); do $(MAKE) -C $$n test || exit 1; done

clean:
	@for n in $(SUB_DIR); do $(MAKE) -C $$n clean || exit 1; done

.PHONY: all test clean
