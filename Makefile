# FreeBSD Routing Library - Makefile
# Alternative build system for environments without CMake

# Compiler settings
CC = clang
CFLAGS = -std=c99 -Wall -Wextra -Werror -DUSERLAND_RADIX
CFLAGS_DEBUG = -O0 -g -DDEBUG
CFLAGS_RELEASE = -O2 -DNDEBUG

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    PLATFORM_CFLAGS = -D__APPLE_USE_RFC_3542
    PLATFORM_LIBS =
else
    # Linux/Unix
    PLATFORM_CFLAGS =
    PLATFORM_LIBS = -lpthread
endif

# Build type (default: debug)
BUILD_TYPE ?= debug
ifeq ($(BUILD_TYPE),release)
    CFLAGS += $(CFLAGS_RELEASE)
else
    CFLAGS += $(CFLAGS_DEBUG)
endif

# Add platform-specific flags
CFLAGS += $(PLATFORM_CFLAGS)

# Directories
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
LIB_DIR = $(BUILD_DIR)/lib

# Include paths
INCLUDES = -I$(SRC_DIR)/include \
           -I$(SRC_DIR)/kernel_compat \
           -I$(SRC_DIR)/freebsd \
           -I$(SRC_DIR)/test

# Source files
KERNEL_COMPAT_SOURCES = $(SRC_DIR)/kernel_compat/kernel_compat.c
FREEBSD_SOURCES = $(SRC_DIR)/freebsd/radix.c
TEST_FRAMEWORK_SOURCES = $(SRC_DIR)/test/test_framework.c
ROUTE_LIB_STUBS_SOURCES = $(SRC_DIR)/lib/route_lib_stubs.c
TEST_SOURCES = $(SRC_DIR)/test/test_main.c \
               $(SRC_DIR)/test/test_radix.c \
               $(SRC_DIR)/test/test_route_table.c \
               $(SRC_DIR)/test/test_freebsd_integration.c

# Object files
KERNEL_COMPAT_OBJS = $(KERNEL_COMPAT_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
FREEBSD_OBJS = $(FREEBSD_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_FRAMEWORK_OBJS = $(TEST_FRAMEWORK_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
ROUTE_LIB_STUBS_OBJS = $(ROUTE_LIB_STUBS_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_OBJS = $(TEST_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Libraries
KERNEL_COMPAT_LIB = $(LIB_DIR)/libkernel_compat.a
FREEBSD_ROUTE_LIB = $(LIB_DIR)/libfreebsd_route.a
TEST_FRAMEWORK_LIB = $(LIB_DIR)/libtest_framework.a
ROUTE_LIB_STUBS_LIB = $(LIB_DIR)/libroute_stubs.a

# Executables
TEST_EXE = $(BIN_DIR)/route_tests
DEMO_EXE = $(BIN_DIR)/route_demo
SCALE_TEST_EXE = $(BIN_DIR)/test_radix_scale

# Default target
all: $(TEST_EXE)

# Create directories
$(OBJ_DIR) $(BIN_DIR) $(LIB_DIR):
	@mkdir -p $@

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build kernel compatibility library
$(KERNEL_COMPAT_LIB): $(KERNEL_COMPAT_OBJS) | $(LIB_DIR)
	ar rcs $@ $^

# Build FreeBSD routing library
$(FREEBSD_ROUTE_LIB): $(FREEBSD_OBJS) $(KERNEL_COMPAT_LIB) | $(LIB_DIR)
	ar rcs $@ $(FREEBSD_OBJS)

# Build test framework library
$(TEST_FRAMEWORK_LIB): $(TEST_FRAMEWORK_OBJS) | $(LIB_DIR)
	ar rcs $@ $^

# Build route lib stubs library
$(ROUTE_LIB_STUBS_LIB): $(ROUTE_LIB_STUBS_OBJS) | $(LIB_DIR)
	ar rcs $@ $^

# Build test executable
$(TEST_EXE): $(TEST_OBJS) $(TEST_FRAMEWORK_LIB) $(FREEBSD_ROUTE_LIB) $(KERNEL_COMPAT_LIB) $(ROUTE_LIB_STUBS_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(PLATFORM_LIBS)

# Build demo executable
$(DEMO_EXE): $(OBJ_DIR)/examples/route_demo.o $(FREEBSD_ROUTE_LIB) $(KERNEL_COMPAT_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(PLATFORM_LIBS)

# Build scale test executable
$(SCALE_TEST_EXE): $(OBJ_DIR)/test/test_radix_scale.o $(TEST_FRAMEWORK_LIB) $(FREEBSD_ROUTE_LIB) $(KERNEL_COMPAT_LIB) $(ROUTE_LIB_STUBS_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(PLATFORM_LIBS)

# Phony targets
.PHONY: all clean test test-verbose test-freebsd test-freebsd-quick test-radix-scale test_radix_scale install uninstall help

# Run tests
test: $(TEST_EXE)
	@echo "Running tests..."
	./$(TEST_EXE)

test-verbose: $(TEST_EXE)
	@echo "Running tests with verbose output..."
	./$(TEST_EXE) --verbose

# Comprehensive FreeBSD Test Suite
FREEBSD_COMPREHENSIVE_TEST = $(BIN_DIR)/test_freebsd_comprehensive
$(FREEBSD_COMPREHENSIVE_TEST): $(SRC_DIR)/test/test_freebsd_comprehensive.c $(KERNEL_COMPAT_LIB) $(FREEBSD_ROUTE_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -pthread \
		$(SRC_DIR)/test/test_freebsd_comprehensive.c \
		$(KERNEL_COMPAT_LIB) $(FREEBSD_ROUTE_LIB) \
		$(PLATFORM_LIBS) -o $@

# Working FreeBSD Components Test
FREEBSD_COMPONENTS_TEST = $(BIN_DIR)/test_freebsd_components
$(FREEBSD_COMPONENTS_TEST): $(SRC_DIR)/test/test_freebsd_components.c $(KERNEL_COMPAT_LIB) $(FREEBSD_ROUTE_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -pthread \
		$(SRC_DIR)/test/test_freebsd_components.c \
		$(KERNEL_COMPAT_LIB) $(FREEBSD_ROUTE_LIB) \
		$(PLATFORM_LIBS) -o $@

test-freebsd: $(FREEBSD_COMPONENTS_TEST)
	@echo "🚀 Running Comprehensive FreeBSD Components Test..."
	@echo "   Testing all .c files under src/freebsd/ with Level 2 rmlock threading"
	./$(FREEBSD_COMPONENTS_TEST)

test-freebsd-quick: $(FREEBSD_COMPONENTS_TEST)
	@echo "⚡ Running Quick FreeBSD Components Test..."
	QUICK_TEST=1 ./$(FREEBSD_COMPONENTS_TEST)

# Scale testing targets
test-radix-scale: $(SCALE_TEST_EXE)
	@echo "🚀 Running Radix Scale Test (10M routes)..."
	@echo "   This will test with 10 million route operations"
	./$(SCALE_TEST_EXE)

test_radix_scale: $(SCALE_TEST_EXE)
	@echo "🚀 Running Radix Scale Test (10M routes)..."
	@echo "   This will test with 10 million route operations"
	./$(SCALE_TEST_EXE)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Install (basic installation)
PREFIX ?= /usr/local
install: $(TEST_EXE) $(DEMO_EXE)
	install -d $(PREFIX)/bin
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include/freebsd_route
	install -m 755 $(TEST_EXE) $(PREFIX)/bin/
	install -m 755 $(DEMO_EXE) $(PREFIX)/bin/
	install -m 644 $(FREEBSD_ROUTE_LIB) $(PREFIX)/lib/
	install -m 644 $(KERNEL_COMPAT_LIB) $(PREFIX)/lib/
	install -m 644 $(SRC_DIR)/include/*.h $(PREFIX)/include/freebsd_route/

# Uninstall
uninstall:
	rm -f $(PREFIX)/bin/route_tests
	rm -f $(PREFIX)/bin/route_demo
	rm -f $(PREFIX)/lib/libfreebsd_route.a
	rm -f $(PREFIX)/lib/libkernel_compat.a
	rm -rf $(PREFIX)/include/freebsd_route

# Help
help:
	@echo "FreeBSD Routing Library Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all           - Build all libraries and executables (default)"
	@echo "  test          - Build and run tests"
	@echo "  test-verbose  - Build and run tests with verbose output"
	@echo "  clean         - Remove build artifacts"
	@echo "  install       - Install libraries and executables"
	@echo "  uninstall     - Remove installed files"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Build options:"
	@echo "  BUILD_TYPE=debug|release  - Set build type (default: debug)"
	@echo "  PREFIX=path               - Set install prefix (default: /usr/local)"
	@echo ""
	@echo "Examples:"
	@echo "  make BUILD_TYPE=release   - Build optimized version"
	@echo "  make install PREFIX=~/.local - Install to user directory"

# Show configuration
config:
	@echo "Build Configuration:"
	@echo "  Platform:    $(UNAME_S)"
	@echo "  Compiler:    $(CC)"
	@echo "  Build type:  $(BUILD_TYPE)"
	@echo "  CFLAGS:      $(CFLAGS)"
	@echo "  INCLUDES:    $(INCLUDES)"
	@echo "  LIBS:        $(PLATFORM_LIBS)"
	@echo "  Install:     $(PREFIX)"