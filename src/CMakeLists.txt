include(GNUInstallDirs)

set(TARGET mcp)

add_library(${TARGET} STATIC
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

# Expose include directories to consumers via target properties
target_include_directories(${TARGET}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/common>
)

target_link_libraries(${TARGET} PUBLIC ${CMAKE_THREAD_LIBS_INIT})

# Link Boost libraries
target_link_libraries(${TARGET} PUBLIC Boost::system)
target_link_libraries(${TARGET} PUBLIC nlohmann_json::nlohmann_json)

# If OpenSSL is found, link the OpenSSL libraries
if(OPENSSL_FOUND)
    target_link_libraries(${TARGET} PUBLIC ${OPENSSL_LIBRARIES})
endif()

# Install the library and export the targets
install(TARGETS ${TARGET}
    EXPORT mcpTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install public headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Install common headers (bundled third-party helpers) under include/common
install(DIRECTORY ${CMAKE_SOURCE_DIR}/common/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/common
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Export the targets file for downstream find_package() support
install(EXPORT mcpTargets
    FILE mcpTargets.cmake
    NAMESPACE mcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp
)
