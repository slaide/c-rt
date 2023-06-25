CC = clang
CSTD = -std=gnu17
LINKS = -lxcb -lvulkan -lxcb-util
FLAGS = -O0 -g -Wall -Werror -Wpedantic -Wextra

COMPILE = $(CC) $(CSTD) $(FLAGS)

.PHONY: default
default: run

main.o: main.c
	$(COMPILE) -c -o main.o main.c

build: main.o
	$(COMPILE) $(LINKS) -o main main.o

shaders/vert.spv: shaders/shader.vert
	glslangValidator shaders/shader.vert -V -o shaders/vert.spv
shaders/frag.spv: shaders/shader.frag
	glslangValidator shaders/shader.frag -V -o shaders/frag.spv

run: build shaders/vert.spv shaders/frag.spv
	./main

.PHONY: clean fresh
clean:
	$(RM) *.o main
fresh:
	$(MAKE) clean
	$(MAKE) build