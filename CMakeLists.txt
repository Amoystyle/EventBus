cmake_minimum_required(VERSION 3.10)
project(EventBus VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Enable optimization for Release builds
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Header-only library
add_library(EventBus INTERFACE)
target_include_directories(EventBus INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Simple test executable
add_executable(simple_test simple_test.cpp)
target_link_libraries(simple_test EventBus)

# Complete test executable (with threading support)
add_executable(complete_test test_full.cpp)
target_link_libraries(complete_test EventBus)

# Usage example executable
add_executable(usage_example example_simple.cpp)
target_link_libraries(usage_example EventBus)

# Link threading library on Unix systems
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(complete_test Threads::Threads)
endif()

# Installation (optional)
install(FILES eventbus.hpp 
        DESTINATION include
        COMPONENT headers)

install(TARGETS simple_test complete_test usage_example
        RUNTIME DESTINATION bin
        COMPONENT executables)

# Testing support
enable_testing()

add_test(NAME SimpleTest 
         COMMAND simple_test)

add_test(NAME CompleteTest 
         COMMAND complete_test)

add_test(NAME UsageExample 
         COMMAND usage_example)

# Print configuration info
message(STATUS "EventBus Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")

# Custom targets for convenience
add_custom_target(run_simple
    COMMAND simple_test
    DEPENDS simple_test
    COMMENT "Running simple test"
)

add_custom_target(run_complete
    COMMAND complete_test
    DEPENDS complete_test
    COMMENT "Running complete test"
)

add_custom_target(run_example
    COMMAND usage_example
    DEPENDS usage_example
    COMMENT "Running usage example"
)

add_custom_target(run_all
    DEPENDS run_simple run_complete run_example
    COMMENT "Running all tests and examples"
)
