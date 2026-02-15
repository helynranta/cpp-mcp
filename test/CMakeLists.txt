cmake_minimum_required(VERSION 3.10)

# Set test project
set(TEST_PROJECT_NAME "mcp_tests")
project(${TEST_PROJECT_NAME})

# Set C++ standard to C++23 for latest features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Find GTest from vcpkg
find_package(GTest REQUIRED)

# Include header directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)

# Add test source files
set(TEST_SOURCES
    mcp_test.cpp
    jsonrpc_validation_test.cpp
    lifecycle_compliance_test.cpp
    streamable_http_transport_test.cpp
    http_security_test.cpp
    tool_safety_test.cpp
    boost_integration_test.cpp
    beast_sse_proof_of_concept.cpp
    http_abstraction_test.cpp
    beast_adapter_test.cpp
    sse_client_beast_test.cpp
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
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
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
