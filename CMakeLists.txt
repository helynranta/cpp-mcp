cmake_minimum_required(VERSION 3.28)
project(MCP VERSION 2025.11.25 LANGUAGES CXX)

# Verify CMake version for C++ module support and import std; feature
if(CMAKE_VERSION VERSION_LESS "3.28")
    message(FATAL_ERROR "CMake 3.28 or higher is required for C++ module support and experimental import std; feature. "
                        "Current version: ${CMAKE_VERSION}. "
                        "Please upgrade CMake from https://cmake.org/download/")
endif()

set(CMAKE_WARN_UNUSED_CLI YES)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows-only project
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
# Define Windows version to suppress Boost.Asio warnings
# Windows 10 (0x0A00) or later required for Boost.Beast
add_compile_definitions(_WIN32_WINNT=0x0A00)

# MSVC compiler settings
add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
add_compile_options("$<$<COMPILE_LANGUAGE:C>:/bigobj>")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/bigobj>")

# C++23 required - no backwards compatibility with older standards
# Minimum compiler version: MSVC 2019+ (MSVC 2022+ recommended for C++20 modules)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable C++ Modules support (CMake 3.28+ for import std; support)
# This enables CMake's native module scanning and dependency tracking
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Find required packages
find_package(Threads REQUIRED)
# Find Boost components: Beast (header-only) and System (compiled)
# Beast is header-only but depends on System for error handling
find_package(Boost REQUIRED COMPONENTS system)
# Note: Boost.Beast is header-only and included in Boost::headers
# Boost::system provides compiled components for error codes

option(MCP_SSL "Enable SSL support" OFF)

if(MCP_SSL)
    find_package(OpenSSL 3.0.0 COMPONENTS Crypto SSL REQUIRED)
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "OpenSSL include directory: ${OPENSSL_INCLUDE_DIR}")
    add_compile_definitions(MCP_SSL)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

# Add MCP library
add_subdirectory(src)

# Add examples
add_subdirectory(examples)

# Add test directory
option(MCP_BUILD_TESTS "Build the tests" OFF)
if(MCP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
