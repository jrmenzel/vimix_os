#
# Build all userspace libs
#
ROOT := ../..
include ../MakefileCommonUser.mk
USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I../../$d)
USER_SPACE_INC_PARAMS_HOST := $(foreach d, $(USER_SPACE_INC_HOST), -I$d)

# for all libs:
LIB_BUILD_DIR := ../../$(BUILD_DIR)/usr/lib
LIB_BUILD_DIR_HOST := ../../$(BUILD_DIR_HOST)/usr/lib

# for libc:
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
	$(LIBC_BUILD_DIR)/termios.o \
	$(LIBC_BUILD_DIR)/usys.o 

ifeq ($(BITWIDTH), 32)
STD_LIBC += $(LIBC_BUILD_DIR)/div64.o
endif

# for vimixutils:
LIB_VIMIXUTILS_BUILD_DIR := $(LIB_BUILD_DIR)/libvimixutils
LIB_VIMIXUTILS_BUILD_DIR_HOST := $(LIB_BUILD_DIR_HOST)/libvimixutils

LIB_VIMIXUTILS_O := \
	path.o

LIB_VIMIXUTILS := $(foreach a, $(LIB_VIMIXUTILS_O), $(LIB_VIMIXUTILS_BUILD_DIR)/$a)
LIB_VIMIXUTILS_HOST := $(foreach a, $(LIB_VIMIXUTILS_O), $(LIB_VIMIXUTILS_BUILD_DIR_HOST)/$a)

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/stdlib.a $(LIB_BUILD_DIR)/vimixutils.a # statically linked libs
stdlib: $(LIB_BUILD_DIR)/stdlib.a
vimixutils: $(LIB_BUILD_DIR)/vimixutils.a $(LIB_BUILD_DIR_HOST)/vimixutils.a

CFLAGS += -fPIC

# build libc
$(LIB_BUILD_DIR)/stdlib.a : $(STD_LIBC)
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/stdlib.a $(STD_LIBC)

# build libvimixutils
$(LIB_BUILD_DIR)/vimixutils.a : $(LIB_VIMIXUTILS) 
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/vimixutils.a $^

# generic rule to compile .c files to .o files on target
$(LIB_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) -c $< -o $@

# build libvimixutils on host
$(LIB_BUILD_DIR_HOST)/vimixutils.a : $(LIB_VIMIXUTILS_HOST)
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR_HOST) -r -o $(LIB_BUILD_DIR_HOST)/vimixutils.a $<

# generic rule to compile .c files to .o files on host
$(LIB_BUILD_DIR_HOST)/%.o: %.c
	@printf "$(TASK_COLOR)Compile on host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -I. $(USER_SPACE_INC_PARAMS_HOST) -c $< -o $@

# automatically generated
$(LIBC_BUILD_DIR)/usys.S : libc/usys.pl
	@printf "$(TASK_COLOR)Generate $(@) from libc/usys.pl\n$(NO_COLOR)"
	@perl libc/usys.pl -$(ARCH) > $(LIBC_BUILD_DIR)/usys.S

$(LIBC_BUILD_DIR)/usys.o : $(LIBC_BUILD_DIR)/usys.S
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -c -o $(LIBC_BUILD_DIR)/usys.o $(LIBC_BUILD_DIR)/usys.S $(USER_SPACE_INC_PARAMS)


directories: # temp build directories
	@mkdir -p $(LIBC_BUILD_DIR)
	@mkdir -p $(LIB_VIMIXUTILS_BUILD_DIR)
	@mkdir -p $(LIB_VIMIXUTILS_BUILD_DIR_HOST)

clean:
	-@rm -rf $(LIB_BUILD_DIR)
	@mkdir -p $(LIB_BUILD_DIR)
	-@rm -rf $(LIB_BUILD_DIR_HOST)
	@mkdir -p $(LIB_BUILD_DIR_HOST)

