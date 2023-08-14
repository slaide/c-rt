MODE ?= debug
USEAVX ?= NO
HIGH_PRECISION ?= NO
DECODE_PARALLEL ?= NO
JEMALLOC ?= NO

.PHONY: default
default: run

OS ?= $(shell uname -s)
ifeq ($(OS),Linux)
	PLATFORM ?= linux
else ifeq ($(OS),Darwin)
	PLATFORM ?= macos
else
$(error invalid OS $(OS))
endif

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

BUILD_OBJS := app.c.o app_mesh.c.o image.c.o huffman.c.o

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

	BUILD_OBJS += main_linux.c.o
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

	BUILD_OBJS += main_macos.m.o

%.m.o: src/%.m
	$(OBJCC) $(COMPILE_FLAGS) $(CDEF) $(CINCLUDE) -c -o $@ $<

else
$(error Invalid platform: $(PLATFORMz) (valid options are { linux | macos }))
endif

CCOMPILE := $(CC) $(OPT_FLAGS) $(CSTD) $(CDEF) $(COMPILE_FLAGS) $(CINCLUDE)

%.c.o: src/%.c
	$(CCOMPILE) -c -o $@ $<

shaders/%.spv: shaders/shader.%
	glslangValidator --quiet -V -o $@ $<

SHADER_FILES := shaders/vert.spv shaders/frag.spv
main: $(BUILD_OBJS) $(SHADER_FILES)
	$(CC) $(OPT_FLAGS) $(CSTD) $(CDEF) $(CINCLUDE) $(LINKS) $(COMPILE_FLAGS) -o $@ $(BUILD_OBJS)

run: main
	./main

.PHONY: profile disasm-image count-loc

PROFILE_CACHEGRIND_OUT_FILE := cachegrind.out
PROFILE_CACHEGRIND_ANNOTATION_FILE := cachegrind.out.annotation

# valgrind is only available on linux
profile:
	ifeq ($(OS),Linux)
		make -Bj fresh MODE=debugrelease
		valgrind --tool=cachegrind --cachegrind-out-file=$(PROFILE_CACHEGRIND_OUT_FILE) ./main
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
	$(RM) *.o main shaders/*.spv $(PROFILE_CACHEGRIND_OUT_FILE) $(PROFILE_CACHEGRIND_ANNOTATION_FILE)
fresh:
	make clean
	make main
