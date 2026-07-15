cmake_minimum_required(VERSION 3.25)

project(MCPTests)

# C++23 required - no backwards compatibility with older standards
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

# Find Boost.Test from vcpkg
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# Include header directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)

# Enable testing
enable_testing()

# Create a shared object library for test main
# This provides BOOST_TEST_MODULE for all test executables
add_library(test_main OBJECT test_main.cpp)
target_link_libraries(test_main PUBLIC Boost::unit_test_framework)

# Helper function to create individual test executables
function(add_mcp_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} $<TARGET_OBJECTS:test_main>)
    
    target_link_libraries(${TEST_NAME} PRIVATE
        Boost::unit_test_framework
        mcp
        Threads::Threads
    )
    
    # If OpenSSL is found, link OpenSSL libraries
    if(OPENSSL_FOUND)
        target_link_libraries(${TEST_NAME} PRIVATE ${OPENSSL_LIBRARIES})
    endif()
    
    # Add the test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Create individual test executables for parallel execution
# Each test file becomes its own executable, allowing CTest to run them in parallel
add_mcp_test(mcp_test mcp_test.cpp)
add_mcp_test(jsonrpc_validation_test jsonrpc_validation_test.cpp)
add_mcp_test(lifecycle_compliance_test lifecycle_compliance_test.cpp)
add_mcp_test(streamable_http_transport_test streamable_http_transport_test.cpp)
add_mcp_test(http_security_test http_security_test.cpp)
add_mcp_test(tool_safety_test tool_safety_test.cpp)
add_mcp_test(boost_integration_test boost_integration_test.cpp)
add_mcp_test(beast_sse_proof_of_concept beast_sse_proof_of_concept.cpp)
add_mcp_test(http_abstraction_test http_abstraction_test.cpp)
add_mcp_test(beast_adapter_test beast_adapter_test.cpp)
if(MCP_BUILD_CLIENT)
    add_mcp_test(sse_client_beast_test sse_client_beast_test.cpp)
endif()
add_mcp_test(session_management_test session_management_test.cpp)
if(MCP_BUILD_CLIENT)
    add_mcp_test(streamable_http_client_test streamable_http_client_test.cpp)
endif()
add_mcp_test(batch_rejection_test batch_rejection_test.cpp)
add_mcp_test(protocol_version_header_test protocol_version_header_test.cpp)
add_mcp_test(structured_tool_output_test structured_tool_output_test.cpp)
add_mcp_test(elicitation_test elicitation_test.cpp)
add_mcp_test(elicitation_integration_test elicitation_integration_test.cpp)
add_mcp_test(completion_test completion_test.cpp)
add_mcp_test(error_result_compliance_test error_result_compliance_test.cpp)
add_mcp_test(structured_tool_handler_test structured_tool_handler_test.cpp)
add_mcp_test(protocol_negotiation_test protocol_negotiation_test.cpp)
add_mcp_test(stdio_server_test stdio_server_test.cpp)

if(NOT TARGET stdio_server_example)
    add_executable(stdio_server_example ../examples/stdio_server/stdio_server_example.cpp)
    target_link_libraries(stdio_server_example PRIVATE mcp)
endif()

add_test(
    NAME stdio_server_subprocess_test
    COMMAND ${CMAKE_COMMAND}
        -DSTDIO_SERVER_EXAMPLE=$<TARGET_FILE:stdio_server_example>
        -DREQUEST_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/stdio_server_session.jsonl
        -P ${CMAKE_CURRENT_SOURCE_DIR}/stdio_server_subprocess_test.cmake
)

# Add custom target to run all tests in parallel
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --parallel --verbose
    DEPENDS 
        mcp_test
        jsonrpc_validation_test
        lifecycle_compliance_test
        streamable_http_transport_test
        http_security_test
        tool_safety_test
        boost_integration_test
        beast_sse_proof_of_concept
        http_abstraction_test
        beast_adapter_test
        session_management_test
        batch_rejection_test
        protocol_version_header_test
        structured_tool_output_test
        elicitation_test
        elicitation_integration_test
        completion_test
        error_result_compliance_test
        structured_tool_handler_test
        protocol_negotiation_test
        stdio_server_test
)

if(MCP_BUILD_CLIENT)
    add_dependencies(run_tests sse_client_beast_test streamable_http_client_test)
endif()
