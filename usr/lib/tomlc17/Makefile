#
# Build all userspace libs
#
ROOT := ../../..
include ../../MakefileCommonUser.mk
USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I../../../$d)
USER_SPACE_INC_PARAMS_HOST := $(foreach d, $(USER_SPACE_INC_HOST), -I$d)

# for all libs:
LIB_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr/lib
LIB_BUILD_DIR_HOST := $(ROOT)/$(BUILD_DIR_HOST)/usr/lib

# for lomlc17:
TOML17_BUILD_DIR := $(LIB_BUILD_DIR)/tomlc17

TOMLC17 := $(TOML17_BUILD_DIR)/tomlc17.o

# for tomlc17:
TOMLC17_BUILD_DIR := $(LIB_BUILD_DIR)/tomlc17
TOMLC17_BUILD_DIR_HOST := $(LIB_BUILD_DIR_HOST)/tomlc17

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/tomlc17.a $(LIB_BUILD_DIR_HOST)/tomlc17.a

CFLAGS += -fPIC

# build tomlc17
$(LIB_BUILD_DIR)/tomlc17.a : $(TOMLC17) #$(LIB_PATH)/stdlib.a
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/tomlc17.a $(TOMLC17)

# generic rule to compile .c files to .o files on target
$(TOMLC17_BUILD_DIR)/%.o: src/%.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

# build tomlc17 on host
$(LIB_BUILD_DIR_HOST)/tomlc17.a : $(TOMLC17)
	@printf "$(TASK_COLOR)Create library (host): $(@)\n$(NO_COLOR)"
	@$(AR_HOST) -r -o $(LIB_BUILD_DIR_HOST)/tomlc17.a $^

# generic rule to compile .c files to .o files on host
$(TOMLC17_BUILD_DIR_HOST)/%.o: src/%.c
	@printf "$(TASK_COLOR)Compile on host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $(USER_SPACE_INC_PARAMS_HOST) -c $< -o $@

#$(LIB_PATH)/stdlib.a: 
#	@$(MAKE) -C ../lib stdlib;

directories: # temp build directories
	@mkdir -p $(TOMLC17_BUILD_DIR)
	@mkdir -p $(TOMLC17_BUILD_DIR_HOST)

clean:
	-@rm -rf $(TOMLC17_BUILD_DIR)
	-@rm -rf $(TOMLC17_BUILD_DIR_HOST)

