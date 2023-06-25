MODE ?= debug

# Set compilation flags based on build mode
ifeq ($(MODE), debug)
	OPT_FLAGS := -g -O0
else ifeq ($(MODE), release)
	OPT_FLAGS := -O2 -flto=full
else
$(error Invalid build mode: $(MODE) (valid options are { release | debug }))
endif

CC = clang
CSTD = -std=gnu17
LINKS = -lxcb -lvulkan -lxcb-util
FLAGS = $(OPT_FLAGS) -Wall -Werror -Wpedantic -Wextra
CINCLUDE = -Iinclude

COMPILE = $(CC) $(CSTD) $(FLAGS) $(CINCLUDE)

.PHONY: default
default: run

%.o: src/%.c
	$(COMPILE) -c -o $@ $<

BUILD_OBJS = main.o app.o app_mesh.o
build: $(BUILD_OBJS)
	$(COMPILE) $(LINKS) -o main $(BUILD_OBJS)

shaders/vert.spv: shaders/shader.vert
	glslangValidator shaders/shader.vert -V -o shaders/vert.spv
shaders/frag.spv: shaders/shader.frag
	glslangValidator shaders/shader.frag -V -o shaders/frag.spv

run: build shaders/vert.spv shaders/frag.spv
	./main

.PHONY: clean fresh
clean:
	$(RM) *.o main shaders/*.spv
fresh:
	$(MAKE) clean
	$(MAKE) build