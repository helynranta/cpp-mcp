cmake_minimum_required(VERSION 3.25)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

function(add_mcp_example TARGET SOURCE)
    add_executable(${TARGET} ${SOURCE})
    target_link_libraries(${TARGET} PRIVATE mcp)
    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    if(OPENSSL_FOUND)
        target_link_libraries(${TARGET} PRIVATE ${OPENSSL_LIBRARIES})
    endif()
endfunction()

if(MCP_BUILD_CLIENT)
    add_mcp_example(sse_client_example sse_client_example.cpp)
    add_mcp_example(stdio_client_example stdio_client_example.cpp)
endif()

if(MCP_BUILD_SERVER)
    add_mcp_example(server_example server_example.cpp)
    add_mcp_example(session_state_example session_state_example.cpp)
    add_mcp_example(structured_tool_example structured_tool_example.cpp)
    add_mcp_example(elicitation_example elicitation_example.cpp)
    add_mcp_example(completion_example completion_example.cpp)
endif()

if(MCP_BUILD_SERVER AND MCP_BUILD_CLIENT)
    # agent_example - uses Beast client for external LLM API calls
    add_mcp_example(agent_example agent_example.cpp)
    add_mcp_example(progress_example progress_example.cpp)
    # streamable_http_client_example - demonstrates Streamable HTTP vs SSE transports
    add_mcp_example(streamable_http_client_example streamable_http_client_example.cpp)
endif()

# http_example - demonstrates Boost.Beast HTTP client/server APIs
add_mcp_example(http_example http_example.cpp)
