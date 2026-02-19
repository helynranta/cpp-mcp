set(TARGET mcp)

# Create library with both traditional sources and C++ modules
add_library(${TARGET} STATIC
    # Traditional header/source files (to be migrated)
    ../include/mcp_client.h
    mcp_message.cpp
    ../include/mcp_message.h
    mcp_resource.cpp
    ../include/mcp_resource.h
    mcp_server.cpp
    ../include/mcp_server.h
    mcp_tool.cpp
    ../include/mcp_tool.h
    mcp_stdio_client.cpp
    ../include/mcp_stdio_client.h
    mcp_sse_client.cpp
    ../include/mcp_sse_client.h
    mcp_streamable_http_client.cpp
    ../include/mcp_streamable_http_client.h
    mcp_progress.cpp
    ../include/mcp_progress.h
)

# Add C++ Module interface files using FILE_SET
# This tells CMake to scan these files for module dependencies
target_sources(${TARGET}
    PUBLIC
        FILE_SET CXX_MODULES FILES
            modules/mcp.core.cppm
            modules/mcp.logger.cppm
)

target_link_libraries(${TARGET} PUBLIC ${CMAKE_THREAD_LIBS_INIT})

# Link Boost libraries
target_link_libraries(${TARGET} PUBLIC Boost::system)

# If OpenSSL is found, link the OpenSSL libraries
if(OPENSSL_FOUND)
    target_link_libraries(${TARGET} PUBLIC ${OPENSSL_LIBRARIES})
endif()
