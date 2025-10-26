#
# Build all userspace libs
#

.PHONY: all clean stdlib vimixutils tomlc17

all: stdlib vimixutils tomlc17

stdlib:
	@$(MAKE) -C libc all;

vimixutils:
	@$(MAKE) -C libvimixutils all;

tomlc17:
	@$(MAKE) -C tomlc17 all;

clean:
	@$(MAKE) -C libc clean;
	@$(MAKE) -C libvimixutils clean;
	@$(MAKE) -C tomlc17 clean;
