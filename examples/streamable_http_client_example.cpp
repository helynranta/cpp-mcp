/**
 * @file streamable_http_client_example.cpp
 * @brief Example demonstrating the Streamable HTTP client
 *
 * This example shows how to use the Streamable HTTP client (MCP 2025-03-26 specification)
 * as an alternative to the SSE client. It demonstrates:
 * - Using the unified /mcp endpoint
 * - Session management with Mcp-Session-Id header
 * - Calling tools and listing resources
 * - Explicit session termination
 */

#include "mcp_server.h"
#include "mcp_sse_client.h"
#include "mcp_streamable_http_client.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace mcp;

void demonstrate_sse_client() {
    std::cout << "\n========================================\n";
    std::cout << "Demonstrating SSE Client (Legacy)\n";
    std::cout << "========================================\n";

    // Create SSE client (uses /sse endpoint)
    sse_client client("http://localhost:8080");

    if (!client.initialize("SSE Example Client", "1.0.0")) {
        std::cerr << "Failed to initialize SSE client\n";
        return;
    }

    std::cout << "✓ SSE client initialized successfully\n";

    // Ping server
    if (client.ping()) {
        std::cout << "✓ Server ping successful\n";
    }

    // Get server capabilities
    json capabilities = client.get_server_capabilities();
    std::cout << "✓ Server capabilities: " << capabilities.dump(2) << "\n";

    // List tools
    auto tools = client.get_tools();
    std::cout << "✓ Available tools: " << tools.size() << "\n";
    for (const auto& tool : tools) {
        std::cout << "  - " << tool.name << ": " << tool.description << "\n";
    }

    // Call a tool
    if (!tools.empty()) {
        std::cout << "\n✓ Calling tool: " << tools[0].name << "\n";
        json result = client.call_tool(tools[0].name, {{"message", "Hello from SSE client!"}});
        std::cout << "  Result: " << result.dump(2) << "\n";
    }

    std::cout << "\n✓ SSE client demonstration complete\n";
}

void demonstrate_streamable_http_client() {
    std::cout << "\n========================================\n";
    std::cout << "Demonstrating Streamable HTTP Client (MCP 2025-03-26)\n";
    std::cout << "========================================\n";

    // Create Streamable HTTP client (uses /mcp endpoint)
    streamable_http_client client("http://localhost:8080");

    if (!client.initialize("Streamable HTTP Example Client", "1.0.0")) {
        std::cerr << "Failed to initialize Streamable HTTP client\n";
        return;
    }

    std::cout << "✓ Streamable HTTP client initialized successfully\n";
    std::cout << "  Using unified /mcp endpoint with Mcp-Session-Id header\n";

    // Ping server
    if (client.ping()) {
        std::cout << "✓ Server ping successful\n";
    }

    // Get server capabilities
    json capabilities = client.get_server_capabilities();
    std::cout << "✓ Server capabilities: " << capabilities.dump(2) << "\n";

    // List tools
    auto tools = client.get_tools();
    std::cout << "✓ Available tools: " << tools.size() << "\n";
    for (const auto& tool : tools) {
        std::cout << "  - " << tool.name << ": " << tool.description << "\n";
    }

    // Call a tool
    if (!tools.empty()) {
        std::cout << "\n✓ Calling tool: " << tools[0].name << "\n";
        json result = client.call_tool(tools[0].name, {{"message", "Hello from Streamable HTTP client!"}});
        std::cout << "  Result: " << result.dump(2) << "\n";
    }

    // Demonstrate explicit session termination (unique to Streamable HTTP)
    std::cout << "\n✓ Explicitly closing session (Streamable HTTP feature)\n";
    client.close_session();

    std::cout << "\n✓ Streamable HTTP client demonstration complete\n";
}

int main() {
    std::cout << "MCP Transport Comparison Example\n";
    std::cout << "=================================\n";
    std::cout << "This example demonstrates both SSE and Streamable HTTP transports\n";

    // Start server
    server::configuration config;
    config.host = "localhost";
    config.port = 8080;
    config.name = "Transport Example Server";
    config.version = "1.0.0";

    server mcp_server(config);

    // Set server capabilities
    json server_capabilities = {{"tools", {{"listChanged", true}}}};
    mcp_server.set_capabilities(server_capabilities);

    // Register a test tool
    tool echo_tool = tool_builder("echo")
                         .with_description("Echo a message back to the client")
                         .with_string_param("message", "The message to echo", "")
                         .build();

    mcp_server.register_tool(echo_tool, [](const json& params, const std::string&) -> json {
        std::string msg = params.value("message", "");
        return {{"echo", msg}, {"timestamp", std::time(nullptr)}};
    });

    // Start server in background
    std::cout << "\nStarting MCP server on port 8080...\n";
    mcp_server.start(false);

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    try {
        // Demonstrate legacy SSE client
        demonstrate_sse_client();

        // Wait a bit between demonstrations
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Demonstrate new Streamable HTTP client
        demonstrate_streamable_http_client();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    // Stop server
    std::cout << "\nStopping server...\n";
    mcp_server.stop();

    std::cout << "\n========================================\n";
    std::cout << "Key Differences:\n";
    std::cout << "========================================\n";
    std::cout << "SSE Client (Legacy):\n";
    std::cout << "  - Uses /sse endpoint for connection\n";
    std::cout << "  - Uses /message endpoint for requests\n";
    std::cout << "  - Session ID in query parameters\n";
    std::cout << "\n";
    std::cout << "Streamable HTTP Client (MCP 2025-03-26):\n";
    std::cout << "  - Uses unified /mcp endpoint\n";
    std::cout << "  - Session ID in Mcp-Session-Id header\n";
    std::cout << "  - Includes Accept header (application/json, text/event-stream)\n";
    std::cout << "  - Supports explicit session termination via DELETE\n";
    std::cout << "  - Returns HTTP 202 Accepted for async requests\n";
    std::cout << "\n";

    return 0;
}
