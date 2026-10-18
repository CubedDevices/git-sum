#
# Top-level convenience makefile for the Linux git-sum build.
#
# Usage:
#   make
#   make CONF=Release
#   make clean
#

CONF ?= Debug

.PHONY: all clean

all:
	$(MAKE) -C git-sum CONF=$(CONF)

clean:
	$(MAKE) -C git-sum CONF=$(CONF) clean
