cmake_minimum_required(VERSION 3.10)

# Set test project
set(TEST_PROJECT_NAME "mcp_tests")
set(CATCH2_TEST_PROJECT_NAME "mcp_catch2_tests")
project(${TEST_PROJECT_NAME})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Try to find Catch2 via vcpkg
find_package(Catch2 3 QUIET)

# Use local Google Test
set(GTEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/googletest)
add_subdirectory(${GTEST_ROOT} googletest-build)

# For older versions of CMake, this option needs to be set
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Include header directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)
include_directories(${GTEST_ROOT}/googletest/include)
include_directories(${GTEST_ROOT}/googlemock/include)

# ===== GoogleTest Tests =====
# Add test source files
set(TEST_SOURCES
    mcp_test.cpp
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

# ===== Catch2 Tests =====
if(Catch2_FOUND)
    message(STATUS "Catch2 found - building Catch2 tests")
    
    # Add Catch2 test source files
    set(CATCH2_TEST_SOURCES
        catch2_tests.cpp
    )
    
    # Create Catch2 test executable
    add_executable(${CATCH2_TEST_PROJECT_NAME} ${CATCH2_TEST_SOURCES})
    
    # Link Catch2 and MCP library
    target_link_libraries(${CATCH2_TEST_PROJECT_NAME} PRIVATE
        Catch2::Catch2WithMain
        mcp
        Threads::Threads
    )
    
    # If OpenSSL is found, link OpenSSL libraries
    if(OPENSSL_FOUND)
        target_link_libraries(${CATCH2_TEST_PROJECT_NAME} PRIVATE ${OPENSSL_LIBRARIES})
    endif()
    
    if(APPLE)
        set_target_properties(${CATCH2_TEST_PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-no_warn_duplicate_libraries")
    endif()
    
    # Add Catch2 test
    add_test(
        NAME ${CATCH2_TEST_PROJECT_NAME}
        COMMAND ${CATCH2_TEST_PROJECT_NAME}
    )
else()
    message(STATUS "Catch2 not found - skipping Catch2 tests. Install via vcpkg to enable.")
endif()

# Enable testing
enable_testing()

# Add GoogleTest test
add_test(
    NAME ${TEST_PROJECT_NAME}
    COMMAND ${TEST_PROJECT_NAME}
)

# Add custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS ${TEST_PROJECT_NAME}
)

if(Catch2_FOUND)
    add_dependencies(run_tests ${CATCH2_TEST_PROJECT_NAME})
endif()
