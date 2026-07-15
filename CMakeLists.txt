cmake_minimum_required(VERSION 3.25)
project(MCP VERSION 2025.11.25 LANGUAGES CXX)

# Verify CMake version for C++ module support and modern features
if(CMAKE_VERSION VERSION_LESS "3.25")
    message(FATAL_ERROR "CMake 3.25 or higher is required for robust C++ module support and modern CMake features. "
                        "Current version: ${CMAKE_VERSION}. "
                        "Please upgrade CMake from https://cmake.org/download/")
endif()

set(CMAKE_WARN_UNUSED_CLI YES)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows-only project
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

if(MSVC)
    # Define Windows version to suppress Boost.Asio warnings
    # Windows 10 (0x0A00) or later required for Boost.Beast
    add_compile_definitions(_WIN32_WINNT=0x0A00)

    # MSVC compiler settings
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/bigobj>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/bigobj>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/EHsc>")
endif()

# C++23 required - no backwards compatibility with older standards
# Minimum compiler version: MSVC 2019+ (MSVC 2022+ recommended for C++20 modules)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)
# Find Boost components: Beast (header-only), System, and throw_exception (compiled)
# Beast is header-only but depends on compiled Boost components for error handling
find_package(Boost REQUIRED COMPONENTS system throw_exception)
find_package(nlohmann_json CONFIG REQUIRED)
# Note: Boost.Beast is header-only and included in Boost::headers
# Boost::system provides compiled components for error codes

option(MCP_SSL "Enable SSL support" OFF)
option(MCP_BUILD_SERVER "Build MCP server implementation" ON)
option(MCP_BUILD_CLIENT "Build MCP client implementations" ON)
option(MCP_BUILD_EXAMPLES "Build examples" ON)
option(MCP_BUILD_TESTS "Build the tests" OFF)

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
if(MCP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Add test directory
if(MCP_BUILD_TESTS)
    if(NOT MCP_BUILD_SERVER)
        message(FATAL_ERROR "MCP_BUILD_TESTS requires MCP_BUILD_SERVER=ON")
    endif()
    enable_testing()
    add_subdirectory(test)
endif()

# ── CMake package config (install support) ──────────────────────────────────
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Generate mcpConfig.cmake from the template in cmake/
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mcpConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mcpConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mcp"
)

# Generate mcpConfigVersion.cmake
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mcpConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the config files so that find_package(mcp) works after install
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mcpConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mcpConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mcp"
)
