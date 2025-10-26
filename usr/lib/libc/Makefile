#
# Build all userspace libs
#
ROOT := ../../..
include ../../MakefileCommonUser.mk

# for all libs:
LIB_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr/lib

# for libc:
LIBC_BUILD_DIR := $(LIB_BUILD_DIR)/libc

STD_LIBC := \
	$(LIBC_BUILD_DIR)/crt0.o \
	$(LIBC_BUILD_DIR)/dirent_impl.o \
	$(LIBC_BUILD_DIR)/stdio_impl.o \
	$(LIBC_BUILD_DIR)/stdio_printf.o \
	$(LIBC_BUILD_DIR)/stdlib_ctype.o \
	$(LIBC_BUILD_DIR)/stdlib_grp.o \
	$(LIBC_BUILD_DIR)/stdlib_impl.o \
	$(LIBC_BUILD_DIR)/stdlib_malloc.o \
	$(LIBC_BUILD_DIR)/stdlib_pwd.o \
	$(LIBC_BUILD_DIR)/stdlib_shadow.o \
	$(LIBC_BUILD_DIR)/string_impl.o \
	$(LIBC_BUILD_DIR)/termios.o \
	$(LIBC_BUILD_DIR)/usys.o 

ifeq ($(BITWIDTH), 32)
STD_LIBC += $(LIBC_BUILD_DIR)/div64.o
endif

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/stdlib.a # statically linked lib
stdlib: $(LIB_BUILD_DIR)/stdlib.a

CFLAGS += -fPIC

# build libc
$(LIB_BUILD_DIR)/stdlib.a : $(STD_LIBC)
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/stdlib.a $(STD_LIBC)

# generic rule to compile .c files to .o files on target
$(LIBC_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

# automatically generated
$(LIBC_BUILD_DIR)/usys.S : usys.pl
	@printf "$(TASK_COLOR)Generate $(@) from libc/usys.pl\n$(NO_COLOR)"
	@perl usys.pl -$(ARCH) > $(LIBC_BUILD_DIR)/usys.S

$(LIBC_BUILD_DIR)/usys.o : $(LIBC_BUILD_DIR)/usys.S
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -c -o $(LIBC_BUILD_DIR)/usys.o $(LIBC_BUILD_DIR)/usys.S $(USER_SPACE_INC_PARAMS)

directories: # temp build directories
	@mkdir -p $(LIBC_BUILD_DIR)

clean:
	-@rm -rf $(LIB_BUILD_DIR)
