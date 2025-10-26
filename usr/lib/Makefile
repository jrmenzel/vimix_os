#
# Build all userspace libs
#

.PHONY: all clean

ifeq ($(TARGET), host)

all:
	@$(MAKE) TARGET=$(TARGET) -C libvimixutils all;
	@$(MAKE) TARGET=$(TARGET) -C tomlc17 all;

else

all:
	@$(MAKE) TARGET=$(TARGET) -C libc all;
	@$(MAKE) TARGET=$(TARGET) -C libvimixutils all;
	@$(MAKE) TARGET=$(TARGET) -C tomlc17 all;

endif
