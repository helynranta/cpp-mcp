/**
 * @file server_example.cpp
 * @brief MCP server example with multiple tools
 *
 * This example demonstrates how to create an MCP server, register tools and resources,
 * and handle client requests. Follows the 2025-06-18 protocol specification.
 *
 * Demonstrates:
 * - Creating and configuring an MCP server with Boost.Beast
 * - Registering multiple tools (time, calculator, echo, hello)
 * - Setting server capabilities
 * - Starting the server in blocking mode
 *
 * Source code: https://github.com/helynranta/cpp-mcp/blob/main/examples/server_example.cpp
 * Related APIs:
 * - MCP Server: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_server.h
 * - Tool Builder: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_tool.h
 */
#include "mcp_resource.h"
#include "mcp_server.h"
#include "mcp_tool.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <thread>

// Global server pointer for signal handling
static mcp::server* g_server = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal " << signal << ", stopping server..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
    }
}

// Tool handler for getting current time
mcp::json get_time_handler(const mcp::json& params, const std::string& /* session_id */) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::string time_str = std::ctime(&time_t_now);
    // Remove trailing newline
    if (!time_str.empty() && time_str[time_str.length() - 1] == '\n') {
        time_str.erase(time_str.length() - 1);
    }

    // MCP 2025-06-18: Return structured output with schema
    mcp::json structured_data = {
        {"timestamp", std::to_string(time_t_now)}, {"formatted", time_str}, {"milliseconds", milliseconds.count()}};

    return {{"content", mcp::json::array({{{"type", "text"}, {"text", time_str}}})},
            {"structuredContent", structured_data},
            {"isError", false}};
}

// Echo tool handler
mcp::json echo_handler(const mcp::json& params, const std::string& /* session_id */) {
    std::string original_text = params.contains("text") ? params["text"].get<std::string>() : "";
    std::string processed_text = original_text;
    bool was_uppercased = false;
    bool was_reversed = false;

    if (params.contains("uppercase") && params["uppercase"].get<bool>()) {
        std::transform(processed_text.begin(), processed_text.end(), processed_text.begin(), ::toupper);
        was_uppercased = true;
    }

    if (params.contains("reverse") && params["reverse"].get<bool>()) {
        std::reverse(processed_text.begin(), processed_text.end());
        was_reversed = true;
    }

    // MCP 2025-06-18: Return structured output with schema
    mcp::json structured_data = {{"original", original_text},
                                 {"processed", processed_text},
                                 {"transformations", {{"uppercase", was_uppercased}, {"reverse", was_reversed}}}};

    return {{"content", mcp::json::array({{{"type", "text"}, {"text", processed_text}}})},
            {"structuredContent", structured_data},
            {"isError", false}};
}

// Calculator tool handler
mcp::json calculator_handler(const mcp::json& params, const std::string& /* session_id */) {
    if (!params.contains("operation")) {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'operation' parameter");
    }

    std::string operation = params["operation"];
    double result = 0.0;

    if (operation == "add") {
        if (!params.contains("a") || !params.contains("b")) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'a' or 'b' parameter");
        }
        result = params["a"].get<double>() + params["b"].get<double>();
    } else if (operation == "subtract") {
        if (!params.contains("a") || !params.contains("b")) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'a' or 'b' parameter");
        }
        result = params["a"].get<double>() - params["b"].get<double>();
    } else if (operation == "multiply") {
        if (!params.contains("a") || !params.contains("b")) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'a' or 'b' parameter");
        }
        result = params["a"].get<double>() * params["b"].get<double>();
    } else if (operation == "divide") {
        if (!params.contains("a") || !params.contains("b")) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'a' or 'b' parameter");
        }
        if (params["b"].get<double>() == 0.0) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Division by zero not allowed");
        }
        result = params["a"].get<double>() / params["b"].get<double>();
    } else {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "Unknown operation: " + operation);
    }

    // MCP 2025-06-18: Return structured output with schema
    mcp::json structured_data = {{"result", result},
                                 {"operation", operation},
                                 {"operands", {{"a", params["a"].get<double>()}, {"b", params["b"].get<double>()}}}};

    return {{"content", mcp::json::array({{{"type", "text"}, {"text", std::to_string(result)}}})},
            {"structuredContent", structured_data},
            {"isError", false}};
}

// Custom API endpoint handler
mcp::json hello_handler(const mcp::json& params, const std::string& /* session_id */) {
    std::string name = params.contains("name") ? params["name"].get<std::string>() : "World";
    std::string greeting = "Hello, " + name + "!";

    // MCP 2025-06-18: Return structured output with schema
    mcp::json structured_data = {{"greeting", greeting}, {"name", name}};

    return {{"content", mcp::json::array({{{"type", "text"}, {"text", greeting}}})},
            {"structuredContent", structured_data},
            {"isError", false}};
}

int main(int argc, char* argv[]) {
    // Ensure file directory exists
    std::filesystem::create_directories("./files");

    // Parse command-line arguments
    std::string host = "localhost";
    int port = 8888;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port PORT    Port to listen on (default: 8888)\n"
                      << "  --host HOST    Host to bind to (default: localhost)\n"
                      << "  --help, -h     Show this help message\n";
            return 0;
        }
    }

    // Create and configure server
    mcp::server::configuration srv_conf;
    srv_conf.host = host;
    srv_conf.port = port;
    // srv_conf.threadpool_size = 1;
    // srv_conf.ssl.server_cert_path = "./server.cert.pem";
    // srv_conf.ssl.server_private_key_path = "./server.key.pem";

    mcp::server server(srv_conf);
    g_server = &server;

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.set_server_info("ExampleServer", "1.0.0");

    // Set server capabilities
    // mcp::json capabilities = {
    //     {"tools", {{"listChanged", true}}},
    //     {"resources", {{"subscribe", false}, {"listChanged", true}}}
    // };
    mcp::json capabilities = {{"tools", mcp::json::object()}};
    server.set_capabilities(capabilities);

    // Register tools with MCP 2025-06-18 output schemas

    // Time tool with output schema
    mcp::json time_output_schema = {
        {"type", "object"},
        {"properties",
         {{"timestamp", {{"type", "string"}, {"description", "Unix timestamp as string"}}},
          {"formatted", {{"type", "string"}, {"description", "Human-readable formatted time"}}},
          {"milliseconds", {{"type", "number"}, {"description", "Milliseconds component"}}}}},
        {"required", mcp::json::array({"timestamp", "formatted", "milliseconds"})}};

    mcp::tool time_tool = mcp::tool_builder("get_time")
                              .with_title("Current Time")
                              .with_description("Get current time with timestamp and formatted string")
                              .with_output_schema(time_output_schema)
                              .with_read_only(true)
                              .with_latency(100)
                              .build();

    // Echo tool with output schema
    mcp::json echo_output_schema = {
        {"type", "object"},
        {"properties",
         {{"original", {{"type", "string"}, {"description", "Original input text"}}},
          {"processed", {{"type", "string"}, {"description", "Processed output text"}}},
          {"transformations",
           {{"type", "object"},
            {"properties",
             {{"uppercase", {{"type", "boolean"}, {"description", "Whether uppercase transformation was applied"}}},
              {"reverse", {{"type", "boolean"}, {"description", "Whether reverse transformation was applied"}}}}}}}}},
        {"required", mcp::json::array({"original", "processed", "transformations"})}};

    mcp::tool echo_tool = mcp::tool_builder("echo")
                              .with_title("Text Echo and Transform")
                              .with_title("Text Echo and Transform")
                              .with_description("Echo input with optional transformations")
                              .with_string_param("text", "Text to echo")
                              .with_boolean_param("uppercase", "Convert to uppercase", false)
                              .with_boolean_param("reverse", "Reverse the text", false)
                              .with_output_schema(echo_output_schema)
                              .with_read_only(true)
                              .with_latency(50)
                              .build();

    // Calculator tool with output schema
    mcp::json calc_output_schema = {{"type", "object"},
                                    {"properties",
                                     {{"result", {{"type", "number"}, {"description", "Calculation result"}}},
                                      {"operation", {{"type", "string"}, {"description", "Operation performed"}}},
                                      {"operands",
                                       {{"type", "object"},
                                        {"properties",
                                         {{"a", {{"type", "number"}, {"description", "First operand"}}},
                                          {"b", {{"type", "number"}, {"description", "Second operand"}}}}}}}}},
                                    {"required", mcp::json::array({"result", "operation", "operands"})}};

    mcp::tool calc_tool = mcp::tool_builder("calculator")
                              .with_title("Basic Calculator")
                              .with_description("Perform basic calculations")
                              .with_string_param("operation", "Operation to perform (add, subtract, multiply, divide)")
                              .with_number_param("a", "First operand")
                              .with_number_param("b", "Second operand")
                              .with_output_schema(calc_output_schema)
                              .with_read_only(true)
                              .with_latency(100)
                              .build();

    // Hello tool with output schema
    mcp::json hello_output_schema = {{"type", "object"},
                                     {"properties",
                                      {{"greeting", {{"type", "string"}, {"description", "The greeting message"}}},
                                       {"name", {{"type", "string"}, {"description", "Name that was greeted"}}}}},
                                     {"required", mcp::json::array({"greeting", "name"})}};

    mcp::tool hello_tool = mcp::tool_builder("hello")
                               .with_title("Greeting Tool")
                               .with_description("Say hello")
                               .with_string_param("name", "Name to say hello to", "World")
                               .with_output_schema(hello_output_schema)
                               .with_read_only(true)
                               .with_latency(50)
                               .build();

    server.register_tool(time_tool, get_time_handler);
    server.register_tool(echo_tool, echo_handler);
    server.register_tool(calc_tool, calculator_handler);
    server.register_tool(hello_tool, hello_handler);

    // // Register resources
    // auto file_resource = std::make_shared<mcp::file_resource>("./Makefile");
    // server.register_resource("file://./Makefile", file_resource);

    // Start server
    std::cout << "Starting MCP server at " << srv_conf.host << ":" << srv_conf.port << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    // Use non-blocking mode so the server runs in a background thread
    // This allows the process to respond properly when run with nohup in CI
    if (!server.start(false)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    // Keep the main thread alive while server runs
    // The signal handlers will call server.stop() and break this loop
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Server stopped" << std::endl;
    return 0;
}
