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
LINK_FLAGS := -lvulkan -pthread
COMPILE_FLAGS := -Wall -Werror -Wpedantic -Wextra -Wno-sequence-point -Wconversion -MMD -MP
CINCLUDE := -Iinclude
CDEF = 

ifeq ($(JEMALLOC), YES)
	CDEF += -DUSE_JEMALLOC
	LINK_FLAGS += -ljemalloc
endif
ifeq ($(HIGH_PRECISION), YES)
	CDEF += -DUSE_FLOAT_PRECISION
endif
ifeq ($(DECODE_PARALLEL), YES)
	CDEF += -DJPEG_DECODE_PARALLEL
endif

REQUIRED_DIRS := 

BUILD_BASE_DIR ?= build
BUILD_DIR ?= $(BUILD_BASE_DIR)/$(MODE)
BUILD_FLAG_DIR ?= $(BUILD_DIR)/build_flags
REQUIRED_DIRS += $(BUILD_BASE_DIR) $(BUILD_DIR) $(BUILD_DIR)/image $(BUILD_FLAG_DIR)

BUILD_FLAGS :=

BIN_DIR ?= bin
REQUIRED_DIRS += $(BIN_DIR)

BUILD_OBJS :=
SHADER_SPV_FILES :=

$(REQUIRED_DIRS) |:
	$(MKDIR_CMD) -p $@

define compile_objc
BUILD_OBJS += $(1)
$(1): $(2) | $(REQUIRED_DIRS)
	$(OBJCC) $(COMPILE_FLAGS) $(CDEF) $(CINCLUDE) -c -o $(1) $(2)
endef

define compile_c
BUILD_OBJS += $(1)
$(1): $(2) | $(REQUIRED_DIRS)
	$(CC) $(OPT_FLAGS) $(CSTD) $(CDEF) $(COMPILE_FLAGS) $(CINCLUDE) -c -o $(1) $(2)
endef

define compile_glsl
SHADER_SPV_FILES += $(1)
$(1): $(2) | $(REQUIRED_DIRS)
	glslangValidator --quiet -V -o $(1) $(2)
endef

ifeq ($(MODE), debug)
	CDEF += -DDEBUG
	COMPILE_FLAGS += -g
	COMPILE_FLAGS += -fno-omit-frame-pointer -fno-inline
	OPT_FLAGS := -O1

	ifeq ($(PLATFORM), linux)
		COMPILE_FLAGS += -fsanitize=address
	endif
else ifeq ($(MODE), debugrelease)
	CDEF += -DDEBUG -DRELEASE
	COMPILE_FLAGS += -g
	COMPILE_FLAGS += -fno-omit-frame-pointer -fno-inline
	OPT_FLAGS := -O3 -ffast-math
else ifeq ($(MODE), release)
	CDEF += -DRELEASE
	OPT_FLAGS := -O3 -ffast-math 
	LINK_FLAGS += -flto=full 
else
$(error Invalid build mode: $(MODE) (valid options are { release | debug }))
endif

ifeq ($(PLATFORM), linux)
	RM_CMD := $(RM) -r
	MKDIR_CMD := mkdir
	CP_CMD := cp

	LINK_FLAGS += -lxcb -lxcb-util -lm
	CDEF += -DVK_USE_PLATFORM_XCB_KHR
	COMPILE_FLAGS += -mssse3 # required for hand-written simd code

	ifeq ($(USEAVX),YES)
		COMPILE_FLAGS += -mavx2 # for some additional speed-up with O3
	endif

$(eval $(call compile_c, $(BUILD_DIR)/main.o, src/main/main_linux.c))
else ifeq ($(PLATFORM), macos)
	RM_CMD := $(RM) -r
	MKDIR_CMD := mkdir
	CP_CMD := cp

	LINK_FLAGS += -framework Appkit -framework Metal -framework MetalKit -framework QuartzCore

	# default MoltenVK installation path
	CINCLUDE += -I/opt/vulkansdk/macOS/include
	LINK_FLAGS += -L/opt/vulkansdk/macOS/lib

	# default homebrew paths
	CINCLUDE += -I/opt/homebrew/include
	LINK_FLAGS += -L/opt/homebrew/lib

	CDEF += -DVK_USE_PLATFORM_METAL_EXT
	CINCLUDE += -I/opt/vulkansdk/macOS/include

$(eval $(call compile_objc, $(BUILD_DIR)/main.o, src/main/main_macos.m))
else
$(error Invalid platform: $(PLATFORM) (valid options are { linux | macos }))
endif

# Usage of the function
$(eval $(call compile_c, $(BUILD_DIR)/app.o, src/app.c))
$(eval $(call compile_c, $(BUILD_DIR)/app_mesh.o, src/app_mesh.c))
$(eval $(call compile_c, $(BUILD_DIR)/huffman.o, src/huffman.c))

$(eval $(call compile_c, $(BUILD_DIR)/image/jpeg.o, src/image/jpeg.c))
$(eval $(call compile_c, $(BUILD_DIR)/image/png.o, src/image/png.c))

$(eval $(call compile_glsl, $(BIN_DIR)/vertshader.spv, shaders/vertshader.vert))
$(eval $(call compile_glsl, $(BIN_DIR)/fragshader.spv, shaders/fragshader.frag))

# add files included by pre-processor in all .c files to their makefile build target dependencies
-include $(BUILD_OBJS:.o=.d)

define add_build_flag
BUILD_FLAGS += $(BUILD_FLAG_DIR)/$(strip $(1))

ifneq ($$(shell ls $(BUILD_FLAG_DIR)/$(strip $(1))* 2> /dev/null), $(BUILD_FLAG_DIR)/$(strip $(1)))
$$(shell touch $(BUILD_FLAG_DIR)/$(strip $(1)))
endif

ifneq ($$(shell cat $(BUILD_FLAG_DIR)/$(strip $(1))),$($(strip $(1))))
$$(shell echo $$($(strip $(1))) > $(BUILD_FLAG_DIR)/$(strip $(1)))
endif
endef

$(eval $(call add_build_flag,HIGH_PRECISION))
$(eval $(call add_build_flag,DECODE_PARALLEL))
$(eval $(call add_build_flag,USEAVX))
$(eval $(call add_build_flag,JEMALLOC))

$(BUILD_OBJS): $(BUILD_FLAGS)

# link 'main' for compile mode
$(BUILD_DIR)/main: $(BUILD_OBJS)
	$(CC) $(OPT_FLAGS) $(CSTD) $(CDEF) $(CINCLUDE) $(LINK_FLAGS) $(COMPILE_FLAGS) -o $@ $^

MAIN_FILE := $(BIN_DIR)/main

# actual main in target output directory should be recreated always because there is no easy way to detect if the present main has been generated with the same build mode
.PHONY: $(MAIN_FILE)
$(MAIN_FILE): $(BUILD_DIR)/main
	$(CP_CMD) $< $@

.PHONY: all
all: $(MAIN_FILE) $(SHADER_SPV_FILES)

.PHONY: profile disasm-jpeg count-loc

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

disasm-jpeg:
	make -Bj fresh MODE=debugrelease
	llvm-objdump -d -S jpeg.c.o > jpeg.asm ; less jpeg.asm

count-loc:
	cloc . --include-lang=c,objective-c,glsl,make,c/c++\ header

$(BIN_DIR)/libjpeg_test: src/libjpeg_test.c
	clang -std=gnu2x -O3 -ffast-math -flto=full -o $(BIN_DIR)/libjpeg_test -I/opt/homebrew/include -L/opt/homebrew/lib -ljpeg src/libjpeg_test.c

.PHONY: test
test: all $(BIN_DIR)/libjpeg_test
	cd $(BIN_DIR) ; \
	for image_file in images/*; do \
		./libjpeg_test "$$image_file" ; \
	done

	cd $(BIN_DIR) ; \
	./main $$(ls images/*.jp*g)

main: main.c

.PHONY: clean fresh doc
doc:
	doxygen Doxyfile
clean:
	$(RM_CMD) $(BUILD_BASE_DIR) $(BIN_DIR)/main* $(BIN_DIR)/*.spv $(PROFILE_CACHEGRIND_OUT_FILE) $(PROFILE_CACHEGRIND_ANNOTATION_FILE)
fresh:
	make clean
	make main
