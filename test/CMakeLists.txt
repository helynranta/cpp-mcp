cmake_minimum_required(VERSION 3.10)

# Set test project
set(TEST_PROJECT_NAME "mcp_tests")
project(${TEST_PROJECT_NAME})

# Set C++ standard to C++23 for latest features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# For older versions of CMake, this option needs to be set
# IMPORTANT: Must be set BEFORE add_subdirectory to take effect
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Use local Google Test
set(GTEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/googletest)
add_subdirectory(${GTEST_ROOT} googletest-build)

# Include header directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)
include_directories(${GTEST_ROOT}/googletest/include)
include_directories(${GTEST_ROOT}/googlemock/include)

# Add test source files
set(TEST_SOURCES
    mcp_test.cpp
    jsonrpc_validation_test.cpp
    lifecycle_compliance_test.cpp
    streamable_http_transport_test.cpp
    http_security_test.cpp
    tool_safety_test.cpp
    mcp_conformance_test.cpp
)

# Create test executable
add_executable(${TEST_PROJECT_NAME} ${TEST_SOURCES})

# Link directories
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../build/src)

if(WIN32)
    set(MCP_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build/src/mcp.lib")
else()
    set(MCP_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build/src/libmcp.a")
endif()

# Link Google Test and MCP library
target_link_libraries(${TEST_PROJECT_NAME} PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    mcp
    Threads::Threads
)

# If OpenSSL is found, link OpenSSL libraries
if(OPENSSL_FOUND)
    target_link_libraries(${TEST_PROJECT_NAME} PRIVATE ${OPENSSL_LIBRARIES})
endif()

if(APPLE)
    set_target_properties(${TEST_PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-no_warn_duplicate_libraries")
endif()

# Enable testing
enable_testing()

# Add test
add_test(
    NAME ${TEST_PROJECT_NAME}
    COMMAND ${TEST_PROJECT_NAME}
)

# Add custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS ${TEST_PROJECT_NAME}
)
