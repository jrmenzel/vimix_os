#
# Build all userspace libs
#

.PHONY: all clean

ifeq ($(TARGET_OR_HOST), host)

all:
	@$(MAKE) TARGET_OR_HOST=$(TARGET_OR_HOST) -C libvimixutils all;
	@$(MAKE) TARGET_OR_HOST=$(TARGET_OR_HOST) -C tomlc17 all;

else

all:
	@$(MAKE) TARGET_OR_HOST=$(TARGET_OR_HOST) -C libc all;
	@$(MAKE) TARGET_OR_HOST=$(TARGET_OR_HOST) -C libvimixutils all;
	@$(MAKE) TARGET_OR_HOST=$(TARGET_OR_HOST) -C tomlc17 all;

endif
