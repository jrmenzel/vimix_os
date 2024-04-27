#
# Build all userspace libs
#
include ../../MakefileCommon.mk

# for all libs:
LIB_BUILD_DIR=../../$(BUILD_DIR)/usr/lib

LIBC_BUILD_DIR=$(LIB_BUILD_DIR)/libc

STD_LIBC=\
	$(LIBC_BUILD_DIR)/crt0.o \
	$(LIBC_BUILD_DIR)/stdio_impl.o \
	$(LIBC_BUILD_DIR)/stdio_printf.o \
	$(LIBC_BUILD_DIR)/stdlib_impl.o \
	$(LIBC_BUILD_DIR)/stdlib_malloc.o \
	$(LIBC_BUILD_DIR)/string_impl.o \
	$(LIBC_BUILD_DIR)/sys_stat_impl.o \
	$(LIBC_BUILD_DIR)/usys.o 

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/stdlib.a

CFLAGS += -fPIC

$(LIB_BUILD_DIR)/stdlib.a : $(STD_LIBC)
	$(AR) -r -o $(LIB_BUILD_DIR)/stdlib.a $(STD_LIBC)

$(LIB_BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) -c $< -o $@

# automatically generated
$(LIBC_BUILD_DIR)/usys.S : libc/usys.pl
	perl libc/usys.pl > $(LIBC_BUILD_DIR)/usys.S

$(LIBC_BUILD_DIR)/usys.o : $(LIBC_BUILD_DIR)/usys.S
	$(CC) $(CFLAGS) -c -o $(LIBC_BUILD_DIR)/usys.o $(LIBC_BUILD_DIR)/usys.S $(USER_SPACE_INC_PARAMS)


directories:
	mkdir -p $(LIBC_BUILD_DIR)/libc

clean:
	-@rm -rf $(LIB_BUILD_DIR)
	mkdir -p $(LIB_BUILD_DIR)

