#
# Build all userspace libs
#
ROOT := ../../..
include ../../MakefileCommonUser.mk

# for all libs:
LIB_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr/lib

# for lomlc17:
TOML17_BUILD_DIR := $(LIB_BUILD_DIR)/tomlc17

TOMLC17 := $(TOML17_BUILD_DIR)/tomlc17.o

# for tomlc17:
TOMLC17_BUILD_DIR := $(LIB_BUILD_DIR)/tomlc17

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/tomlc17.a 

CFLAGS += -fPIC

# build tomlc17
$(LIB_BUILD_DIR)/tomlc17.a : $(TOMLC17)
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/tomlc17.a $(TOMLC17)

# generic rule to compile .c files to .o files on target
$(TOMLC17_BUILD_DIR)/%.o: src/%.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

directories: # temp build directories
	@mkdir -p $(TOMLC17_BUILD_DIR)

clean:
	-@rm -rf $(TOMLC17_BUILD_DIR)

