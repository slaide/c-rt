MODE ?= debug
PLATFORM ?= linux

.PHONY: default
default: run

CC = clang
OBJCC = clang
CSTD = -std=gnu2x
LINKS = -lvulkan
FLAGS = $(OPT_FLAGS) -Wall -Werror -Wpedantic -Wextra
CINCLUDE = -Iinclude
CDEF = 

BUILD_OBJS = app.c.o app_mesh.c.o image.c.o

ifeq ($(MODE), debug)
	OPT_FLAGS := -g -O0
else ifeq ($(MODE), release)
	OPT_FLAGS := -O2 -flto=full
else
$(error Invalid build mode: $(MODE) (valid options are { release | debug }))
endif

ifeq ($(PLATFORM), linux)
	LINKS += -lxcb -lxcb-util
	CDEF += -DVK_USE_PLATFORM_XCB_KHR

	BUILD_OBJS += main_linux.c.o
else ifeq ($(PLATFORM), macos)
	LINKS += -framework Appkit -framework Metal -framework MetalKit -framework QuartzCore
	CDEF += -DVK_USE_PLATFORM_METAL_EXT
	CINCLUDE += -I/opt/vulkansdk/macOS/include

	BUILD_OBJS += main_macos.m.o

%.m.o: src/%.m
	$(OBJCC) $(FLAGS) $(CDEF) $(CINCLUDE) -c -o $@ $<

else

$(error Invalid platform: $(MODE) (valid options are { linux | macos }))

endif

CCOMPILE = $(CC) $(CSTD) $(CDEF) $(FLAGS) $(CINCLUDE)

%.c.o: src/%.c
	$(CCOMPILE) -c -o $@ $<

build: $(BUILD_OBJS) shaders/vert.spv shaders/frag.spv
	$(CCOMPILE) $(LINKS) -o main $(BUILD_OBJS)

shaders/vert.spv: shaders/shader.vert
	glslangValidator shaders/shader.vert -V -o shaders/vert.spv
shaders/frag.spv: shaders/shader.frag
	glslangValidator shaders/shader.frag -V -o shaders/frag.spv

run: build
	./main


.PHONY: clean fresh doc
doc:
	doxygen Doxyfile
clean:
	$(RM) *.o main shaders/*.spv
fresh:
	$(MAKE) clean
	$(MAKE) build