MODE ?= debugrelease
USEAVX ?= NO
HIGH_PRECISION ?= NO
DECODE_PARALLEL ?= NO
JEMALLOC ?= NO

.PHONY: default
default: all

OS ?= $(shell uname -s)
ifeq ($(OS),Linux)
	DEFAULT_PLATFORM := linux
else ifeq ($(OS),Darwin)
	DEFAULT_PLATFORM := macos
else
$(error invalid OS $(OS))
endif

PLATFORM ?= $(DEFAULT_PLATFORM)

CC := clang
OBJCC := clang
CSTD := -std=gnu2x
LINKS := -lvulkan -pthread
COMPILE_FLAGS := -Wall -Werror -Wpedantic -Wextra -Wno-sequence-point -Wconversion
CINCLUDE := -Iinclude
CDEF = 

ifeq ($(JEMALLOC), YES)
	CDEF += -DUSE_JEMALLOC
	LINKS += -ljemalloc
endif
ifeq ($(HIGH_PRECISION), YES)
	CDEF += -DUSE_FLOAT_PRECISION
endif
ifeq ($(DECODE_PARALLEL), YES)
	CDEF += -DJPEG_DECODE_PARALLEL
endif

BUILD_DIR ?= build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

BIN_DIR ?= bin
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

SHADER_SRC_FILES := vertshader.vert fragshader.frag
C_SRC_FILES := app.c app_mesh.c image.c huffman.c
OBJC_SRC_FILES :=

ifeq ($(MODE), debug)
	CDEF += -DDEBUG
	COMPILE_FLAGS += -g

	ifeq ($(PLATFORM), linux)
		COMPILE_FLAGS += -fsanitize=address
	endif

	COMPILE_FLAGS += -fno-omit-frame-pointer -fno-inline
else ifeq ($(MODE), debugrelease)
	CDEF += -DDEBUG -DRELEASE
	COMPILE_FLAGS += -fno-omit-frame-pointer -fno-inline
	COMPILE_FLAGS += -g

	ifeq ($(PLATFORM), linux)
		# COMPILE_FLAGS += -fsanitize=address
	endif
	
	OPT_FLAGS := -O3 -ffast-math
else ifeq ($(MODE), release)
	CDEF += -DRELEASE
	OPT_FLAGS := -O3 -ffast-math 
	LINKS += -flto=full 
else
$(error Invalid build mode: $(MODE) (valid options are { release | debug }))
endif

ifeq ($(PLATFORM), linux)
	LINKS += -lxcb -lxcb-util -lm
	CDEF += -DVK_USE_PLATFORM_XCB_KHR
	COMPILE_FLAGS += -mssse3 # required for hand-written simd code

	ifeq ($(USEAVX),YES)
		COMPILE_FLAGS += -mavx2 # for some additional speed-up with O3
	endif

	C_SRC_FILES += main_linux.c
else ifeq ($(PLATFORM), macos)
	LINKS += -framework Appkit -framework Metal -framework MetalKit -framework QuartzCore

	# default MoltenVK installation path
	CINCLUDE += -I/opt/vulkansdk/macOS/include
	LINKS += -L/opt/vulkansdk/macOS/lib

	# default homebrew paths
	CINCLUDE += -I/opt/homebrew/include
	LINKS += -L/opt/homebrew/lib

	CDEF += -DVK_USE_PLATFORM_METAL_EXT
	CINCLUDE += -I/opt/vulkansdk/macOS/include

	OBJC_SRC_FILES += main_macos.m
else
$(error Invalid platform: $(PLATFORMz) (valid options are { linux | macos }))
endif

$(BUILD_DIR)/%.m.o: src/%.m $(BUILD_DIR)
	$(OBJCC) $(COMPILE_FLAGS) $(CDEF) $(CINCLUDE) -c -o $@ $<

CCOMPILE := $(CC) $(OPT_FLAGS) $(CSTD) $(CDEF) $(COMPILE_FLAGS) $(CINCLUDE)

$(BUILD_DIR)/%.c.o: src/%.c $(BUILD_DIR)
	$(CCOMPILE) -c -o $@ $<

$(BIN_DIR)/%.spv: shaders/%.* $(BIN_DIR)
	glslangValidator --quiet -V -o $@ $<

SHADER_SPV_FILES := $(patsubst %,$(BIN_DIR)/%.spv,$(basename $(SHADER_SRC_FILES)))

C_BUILD_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(C_SRC_FILES))
OBJC_BUILD_OBJS := $(patsubst %.m,$(BUILD_DIR)/%.m.o,$(OBJC_SRC_FILES))

BUILD_OBJS := $(C_BUILD_OBJS) $(OBJC_BUILD_OBJS)

MAIN_FILE := $(BIN_DIR)/main

all: $(SHADER_SPV_FILES) $(BUILD_OBJS) $(BIN_DIR)
	$(CC) $(OPT_FLAGS) $(CSTD) $(CDEF) $(CINCLUDE) $(LINKS) $(COMPILE_FLAGS) -o $(MAIN_FILE) $(BUILD_OBJS)

.PHONY: profile disasm-image count-loc

PROFILE_CACHEGRIND_OUT_FILE := cachegrind.out
PROFILE_CACHEGRIND_ANNOTATION_FILE := cachegrind.out.annotation

# valgrind is only available on linux
profile:
	ifeq ($(OS),Linux)
		make -Bj fresh MODE=debugrelease
		valgrind --tool=cachegrind --cachegrind-out-file=$(PROFILE_CACHEGRIND_OUT_FILE) ./$(MAIN_FILE)
		callgrind_annotate $(PROFILE_CACHEGRIND_OUT_FILE) > $(PROFILE_CACHEGRIND_ANNOTATION_FILE)
		less $(PROFILE_CACHEGRIND_ANNOTATION_FILE)
	else
	$(error valgrind is only available on Linux, but you are using $(PLATFORM))
	endif

disasm-image:
	make -Bj fresh MODE=debugrelease
	llvm-objdump -d -S image.c.o > image.asm ; less image.asm

count-loc:
	cloc . --include-lang=c,objective-c,glsl,make,c/c++\ header

.PHONY: clean fresh doc
doc:
	doxygen Doxyfile
clean:
	$(RM) $(BUILD_DIR)/* $(BIN_DIR)/* $(PROFILE_CACHEGRIND_OUT_FILE) $(PROFILE_CACHEGRIND_ANNOTATION_FILE)
fresh:
	make clean
	make main
