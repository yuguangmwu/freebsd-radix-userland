cmake_minimum_required(VERSION 3.12)
project(FreeBSDRouteLib VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")

# Platform-specific settings
if(APPLE)
    # macOS specific settings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__APPLE_USE_RFC_3542")
    # Find BSD socket libraries
    find_library(SOCKET_LIBRARY NAMES socket)
    if(SOCKET_LIBRARY)
        list(APPEND EXTRA_LIBS ${SOCKET_LIBRARY})
    endif()
elseif(UNIX)
    # Linux/Unix specific settings
    list(APPEND EXTRA_LIBS pthread)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/kernel_compat
    ${CMAKE_SOURCE_DIR}/src/freebsd
)

# Kernel compatibility layer
set(KERNEL_COMPAT_SOURCES
    src/kernel_compat/kernel_compat.c
)

# FreeBSD routing sources (adapted for userland)
set(FREEBSD_SOURCES
    src/freebsd/radix.c
    # Note: Other FreeBSD files will need adaptation before compilation
    # src/freebsd/route.c
    # src/freebsd/route_ctl.c
    # src/freebsd/nhop.c
    # src/freebsd/fib_algo.c
    # src/freebsd/route_tables.c
    # src/freebsd/route_helpers.c
)

# Create kernel compatibility static library
add_library(kernel_compat STATIC ${KERNEL_COMPAT_SOURCES})
target_include_directories(kernel_compat PUBLIC src/kernel_compat)

# Create FreeBSD routing static library
add_library(freebsd_route STATIC ${FREEBSD_SOURCES})
target_link_libraries(freebsd_route kernel_compat)
target_include_directories(freebsd_route PUBLIC src/include)

# Test framework
set(TEST_FRAMEWORK_SOURCES
    src/test/test_framework.c
)

add_library(test_framework STATIC ${TEST_FRAMEWORK_SOURCES})
target_include_directories(test_framework PUBLIC src/test)

# Test sources
set(TEST_SOURCES
    src/test/test_main.c
    src/test/test_radix.c
    src/test/test_route_table.c
)

# Create test executable
add_executable(route_tests ${TEST_SOURCES})
target_link_libraries(route_tests
    test_framework
    freebsd_route
    kernel_compat
    ${EXTRA_LIBS}
)

# Example/demo program
add_executable(route_demo
    src/examples/route_demo.c
)
target_link_libraries(route_demo
    freebsd_route
    kernel_compat
    ${EXTRA_LIBS}
)

# Install targets
install(TARGETS freebsd_route kernel_compat
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    src/include/route_lib.h
    src/include/freebsd_route_adapter.h
    DESTINATION include/freebsd_route
)

install(TARGETS route_tests route_demo
    RUNTIME DESTINATION bin
)

# Testing configuration
enable_testing()

# Add individual test cases (can be run separately)
add_test(NAME radix_tests
    COMMAND route_tests --suite radix)

add_test(NAME route_table_tests
    COMMAND route_tests --suite route_table)

# Add all tests
add_test(NAME all_tests
    COMMAND route_tests)

# Custom targets for convenience
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS route_tests
    COMMENT "Running all tests"
)

add_custom_target(check-verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS route_tests
    COMMENT "Running all tests with verbose output"
)

# Documentation target (if Doxygen is available)
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_SOURCE_DIR}/docs/Doxyfile.in
                   ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif()

# Package configuration
set(CPACK_PACKAGE_NAME "freebsd-route-lib")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "FreeBSD Routing Library - Userland Port")
set(CPACK_PACKAGE_CONTACT "developer@example.com")

if(APPLE)
    set(CPACK_GENERATOR "TGZ;ZIP")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

include(CPack)

# Display configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  C compiler:     ${CMAKE_C_COMPILER}")
message(STATUS "  C flags:        ${CMAKE_C_FLAGS}")
message(STATUS "  Extra libs:     ${EXTRA_LIBS}")
message(STATUS "  Platform:       ${CMAKE_SYSTEM_NAME}")
message(STATUS "")