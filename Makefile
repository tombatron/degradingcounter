REDIS_SRC = ../redis/src # The source directory containing the redismodule.h header file.
CC = gcc # We're using gcc as our compiler.

# -Wall: Most commonly used compiler warnings.
# -fPIC: "Position Independent Code". The library will be able to be loaded into any memory location. A must for shared libraries.
# -std=c99: Using the C99 standard. Why? Why not.
# -O2: Optimized code for speed and performance without exploding the build time.
CFLAGS = -Wall -fPIC -std=c99 -O2


LDFLAGS = -shared # Tells the linker to create a shared library.

TARGET = degrading-counter.so # The name of the output library.

SRC = $(wildcard *.c) # Capture all of the C files for compilation.

DESTDIR ?= ./module # Optional, we'll copy the compiled binary to a separate location. Using this for local development.

# Run the `build` and `install` tasks.
all: build install

# Alias for the task of building the library.
build: $(TARGET)

# Task for running the compiler and linker to generate an installable module.
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -I$(REDIS_SRC) $(SRC) -o $(TARGET) $(LDFLAGS)

# Task for pushing the module to a directory that Redis can see.
install: $(TARGET)
	mkdir -p $(DESTDIR)
	cp $(TARGET) $(DESTDIR)

# Task for removing the compiled binary.
clean:
	rm -f $(TARGET)