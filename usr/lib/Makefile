#
# Build all userspace libs
#
include ../MakefileCommonUser.mk
USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I../../$d)

# for all libs:
LIB_BUILD_DIR := ../../$(BUILD_DIR)/usr/lib

LIBC_BUILD_DIR := $(LIB_BUILD_DIR)/libc

STD_LIBC := \
	$(LIBC_BUILD_DIR)/crt0.o \
	$(LIBC_BUILD_DIR)/dirent_impl.o \
	$(LIBC_BUILD_DIR)/stdio_impl.o \
	$(LIBC_BUILD_DIR)/stdio_printf.o \
	$(LIBC_BUILD_DIR)/stdlib_ctype.o \
	$(LIBC_BUILD_DIR)/stdlib_impl.o \
	$(LIBC_BUILD_DIR)/stdlib_malloc.o \
	$(LIBC_BUILD_DIR)/string_impl.o \
	$(LIBC_BUILD_DIR)/sys_stat_impl.o \
	$(LIBC_BUILD_DIR)/usys.o 

ifeq ($(BITWIDTH), 32)
STD_LIBC += $(LIBC_BUILD_DIR)/div64.o
endif

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/stdlib.a # statically linked stdlib 

CFLAGS += -fPIC

$(LIB_BUILD_DIR)/stdlib.a : $(STD_LIBC)
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/stdlib.a $(STD_LIBC)

$(LIB_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) -c $< -o $@

# automatically generated
$(LIBC_BUILD_DIR)/usys.S : libc/usys.pl
	@printf "$(TASK_COLOR)Generate $(@) from libc/usys.pl\n$(NO_COLOR)"
	@perl libc/usys.pl -$(ARCH) > $(LIBC_BUILD_DIR)/usys.S

$(LIBC_BUILD_DIR)/usys.o : $(LIBC_BUILD_DIR)/usys.S
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -c -o $(LIBC_BUILD_DIR)/usys.o $(LIBC_BUILD_DIR)/usys.S $(USER_SPACE_INC_PARAMS)


directories: # temp build directories
	@mkdir -p $(LIBC_BUILD_DIR)/libc

clean:
	-@rm -rf $(LIB_BUILD_DIR)
	@mkdir -p $(LIB_BUILD_DIR)

