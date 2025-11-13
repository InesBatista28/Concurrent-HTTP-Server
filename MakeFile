# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -lrt -g
TARGET = concurrent-http-server
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Default target
all: $(BINDIR)/$(TARGET)

# Create binary
$(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(CFLAGS)

# Create object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Build and run server
run: all
	./$(BINDIR)/$(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Run tests
test: all
	@echo "Running functional tests..."
	@# Add your test commands here
	@echo "Running concurrency tests..."
	ab -n 10000 -c 100 http://localhost:8080/
	@echo "Running synchronization tests..."
	valgrind --tool=helgrind ./$(BINDIR)/$(TARGET) &
	sleep 2
	pkill -f $(TARGET)
	@echo "Running stress tests..."
	valgrind --leak-check=full ./$(BINDIR)/$(TARGET) &
	sleep 300
	pkill -f $(TARGET)

# Install dependencies (if needed)
deps:
	@echo "Installing required tools..."
	sudo apt-get update
	sudo apt-get install -y apache2-utils valgrind curl

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: all

# Release build
release: CFLAGS += -O2
release: all

# Static analysis
analyze:
	cppcheck --enable=all $(SRCDIR)

# Format code
format:
	find $(SRCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

# Create distribution package
dist: clean
	mkdir -p dist
	tar -czf dist/$(TARGET)-$(shell date +%Y%m%d).tar.gz \
		--exclude='.*' \
		--exclude='dist' \
		--exclude='obj' \
		--exclude='bin' \
		.

.PHONY: all run clean test deps debug release analyze format dist