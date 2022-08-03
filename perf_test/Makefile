OUTPUT_DIR := build
OBJ_DIR := $(OUTPUT_DIR)/obj

ifeq ($(OS),Windows_NT)
# Windows-specific vars; if you did a custom install of MinGW, or if you are using some
# other compiler, then you might need to change some of these values
    #BIN_DIR := C:/MinGW/bin
    BIN_DIR := C:/msys64/mingw64/bin
    GCC := $(BIN_DIR)/gcc
    LD := $(BIN_DIR)/ld
    MKDIR := mkdir -p
    RMDIR := rm -rf
else
    ifeq ($(shell uname -s),Linux)
# Linux-specific vars
        GCC := gcc
        LD := ld
        MKDIR := mkdir -p
        RMDIR := rm -rf
    endif
endif

OUTPUT_DIR := build
TEST_PROG := $(OUTPUT_DIR)/perf_test

SRC_FILES := perf_test.c ../hashtable.c
INCLUDES := -Iunity/src -I../
CFLAGS := -Wall -pedantic -std=c99 -DHASHTABLE_SIZE_T_UINT16

.PHONY: clean test

default: test

all: clean test

debug: CFLAGS += -g -O0
debug: $(TEST_PROG)

test: $(TEST_PROG)

$(TEST_PROG): $(OUTPUT_DIR)
	$(GCC) $(CFLAGS) $(SRC_FILES) $(INCLUDES) -o $(TEST_PROG)

$(OUTPUT_DIR):
	$(MKDIR) $(OUTPUT_DIR)

clean:
	@$(RMDIR) $(OUTPUT_DIR)
	@echo "Outputs removed"