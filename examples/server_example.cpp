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
#include "mcp_progress.h"
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

    // Set server capabilities needed for conformance coverage
    server.set_capabilities({{"logging", mcp::json::object()},
                             {"prompts", {{"listChanged", true}}},
                             {"resources", {{"listChanged", true}}},
                             {"tools", {{"listChanged", true}}},
                             {"completion", mcp::json::object()}});

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

    // Conformance tools
    const std::string png_base64 =
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/xcAAn8B9pQ5e0cAAAAASUVORK5CYII=";
    const std::string wav_base64 = "UklGRiQAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQAAAAA=";

    server.register_tool(
        mcp::tool_builder("test_simple_text").with_description("Conformance: simple text response").build(),
        [](const mcp::json&, const std::string&) -> mcp::json {
            return {{"content",
                     mcp::json::array({{{"type", "text"}, {"text", "This is a simple text response for testing."}}})}};
        });

    server.register_tool(
        mcp::tool_builder("test_image_content").with_description("Conformance: image content").build(),
        [png_base64](const mcp::json&, const std::string&) -> mcp::json {
            return {
                {"content", mcp::json::array({{{"type", "image"}, {"data", png_base64}, {"mimeType", "image/png"}}})}};
        });

    server.register_tool(
        mcp::tool_builder("test_audio_content").with_description("Conformance: audio content").build(),
        [wav_base64](const mcp::json&, const std::string&) -> mcp::json {
            return {
                {"content", mcp::json::array({{{"type", "audio"}, {"data", wav_base64}, {"mimeType", "audio/wav"}}})}};
        });

    server.register_tool(
        mcp::tool_builder("test_embedded_resource").with_description("Conformance: embedded resource").build(),
        [](const mcp::json&, const std::string&) -> mcp::json {
            return {{"content",
                     mcp::json::array({{{"type", "resource"},
                                        {"resource",
                                         {{"uri", "test://embedded-resource"},
                                          {"mimeType", "application/json"},
                                          {"text", R"({"embedded":true,"message":"Embedded resource content"})"}}}}})}};
        });

    server.register_tool(
        mcp::tool_builder("test_multiple_content_types").with_description("Conformance: mixed content types").build(),
        [png_base64](const mcp::json&, const std::string&) -> mcp::json {
            return {{"content", mcp::json::array({{{"type", "text"}, {"text", "Multiple content types test:"}},
                                                  {{"type", "image"}, {"data", png_base64}, {"mimeType", "image/png"}},
                                                  {{"type", "resource"},
                                                   {"resource",
                                                    {{"uri", "test://mixed-content-resource"},
                                                     {"mimeType", "application/json"},
                                                     {"text", R"({"test":"data","value":123})"}}}}})}};
        });

    server.register_tool(
        mcp::tool_builder("test_error_handling").with_description("Conformance: error handling").build(),
        [](const mcp::json&, const std::string&) -> mcp::json {
            return {{"isError", true},
                    {"content",
                     mcp::json::array(
                         {{{"type", "text"}, {"text", "This tool intentionally returns an error for testing"}}})}};
        });

    // Tool that emits logging notifications
    server.register_tool(
        mcp::tool_builder("test_tool_with_logging").with_description("Conformance: logging tool").build(),
        [&server](const mcp::json&, const std::string& session_id) -> mcp::json {
            auto send_log = [&server, &session_id](const std::string& level, const std::string& message) {
                mcp::request req;
                req.jsonrpc = "2.0";
                req.id = nullptr; // notification
                req.method = "logging/message";
                req.params = {{"level", level}, {"message", message}};
                server.send_request(session_id, req);
            };
            send_log("debug", "Logging tool started");
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            send_log("info", "Logging tool in progress");
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            send_log("notice", "Logging tool completed");
            return {
                {"content", mcp::json::array({{{"type", "text"}, {"text", "Logging tool completed successfully"}}})}};
        });

    // Tool with progress notifications (for tools-call-with-progress conformance test)
    server.register_tool(
        mcp::tool_builder("test_tool_with_progress")
            .with_description("Conformance: tool that sends progress notifications")
            .with_number_param("steps", "Number of steps to process", 5.0)
            .build(),
        [&server](const mcp::json& params, const std::string& session_id) -> mcp::json {
            int steps = params.value("steps", 5);

            // Extract progress token from _meta if present
            auto progress_token = mcp::progress_tracker::extract_progress_token(params);

            for (int i = 1; i <= steps; ++i) {
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                // Send progress notification if token is available
                if (progress_token.has_value()) {
                    std::string message = "Processing step " + std::to_string(i) + " of " + std::to_string(steps);
                    mcp::progress_notification notif = mcp::progress_notification::create(
                        progress_token.value(), static_cast<double>(i), static_cast<double>(steps), message);
                    server.send_progress(session_id, notif);
                }
            }

            return {{"content", mcp::json::array({{{"type", "text"},
                                                   {"text", "Completed " + std::to_string(steps) +
                                                                " steps with progress updates"}}})}};
        });

    // Tool with elicitation (for tools-call-elicitation conformance test)
    server.register_tool(
        mcp::tool_builder("test_elicitation")
            .with_description("Conformance: tool that requests user input via elicitation")
            .with_string_param("message", "Message to show user", "")
            .build(),
        [&server](const mcp::json& params, const std::string& session_id) -> mcp::json {
            std::string message = params.value("message", "Please provide your information");

            // Check if client supports elicitation
            if (!server.client_supports_elicitation(session_id)) {
                return {{"isError", true},
                        {"content",
                         mcp::json::array(
                             {{{"type", "text"}, {"text", "Client does not support elicitation, cannot proceed"}}})}};
            }

            // Request user input via elicitation with username and email
            mcp::json schema = {{"type", "object"},
                                {"properties",
                                 {{"username", {{"type", "string"}, {"description", "User's response"}}},
                                  {"email", {{"type", "string"}, {"description", "User's email address"}}}}},
                                {"required", mcp::json::array({"username", "email"})}};

            try {
                mcp::elicitation_result result = server.request_elicitation(session_id, message, schema);

                // Format response describing what happened
                std::string action_str;
                switch (result.action) {
                    case mcp::elicitation_action::accept:
                        action_str = "accept";
                        break;
                    case mcp::elicitation_action::decline:
                        action_str = "decline";
                        break;
                    case mcp::elicitation_action::cancel:
                        action_str = "cancel";
                        break;
                }

                std::string response_text = "User response: action=" + action_str;
                if (result.action == mcp::elicitation_action::accept) {
                    response_text += ", content=" + result.content.dump();
                }

                return {{"content", mcp::json::array({{{"type", "text"}, {"text", response_text}}})}};
            } catch (const mcp::mcp_exception& e) {
                return {{"isError", true},
                        {"content", mcp::json::array({{{"type", "text"},
                                                       {"text", "Elicitation failed: " + std::string(e.what())}}})}};
            }
        });

    // Tool with elicitation and defaults (SEP-1034)
    server.register_tool(
        mcp::tool_builder("test_elicitation_sep1034_defaults")
            .with_description("Conformance: elicitation with default values (SEP-1034)")
            .build(),
        [&server](const mcp::json&, const std::string& session_id) -> mcp::json {
            if (!server.client_supports_elicitation(session_id)) {
                return {{"isError", true},
                        {"content",
                         mcp::json::array({{{"type", "text"}, {"text", "Client does not support elicitation"}}})}};
            }

            // Schema with default values per SEP-1034
            mcp::json schema = {
                {"type", "object"},
                {"properties",
                 {{"name", {{"type", "string"}, {"default", "John Doe"}, {"description", "User name"}}},
                  {"age", {{"type", "integer"}, {"default", 30}, {"description", "User age"}}},
                  {"score", {{"type", "number"}, {"default", 95.5}, {"description", "User score"}}},
                  {"status",
                   {{"type", "string"},
                    {"enum", mcp::json::array({"active", "inactive", "pending"})},
                    {"default", "active"},
                    {"description", "User status"}}},
                  {"verified", {{"type", "boolean"}, {"default", true}, {"description", "Verification status"}}}}}};

            try {
                mcp::elicitation_result result = server.request_elicitation(
                    session_id, "Configure settings (defaults will be used if not provided)", schema);

                if (result.action == mcp::elicitation_action::accept) {
                    std::string response_text = "Elicitation completed: action=accept, content=" +
                                                result.content.dump();
                    return {{"content", mcp::json::array({{{"type", "text"}, {"text", response_text}}})}};
                }

                std::string action_str = (result.action == mcp::elicitation_action::decline) ? "decline" : "cancel";
                return {{"content", mcp::json::array({{{"type", "text"},
                                                       {"text", "Elicitation completed: action=" + action_str}}})}};
            } catch (const mcp::mcp_exception& e) {
                return {{"isError", true},
                        {"content", mcp::json::array({{{"type", "text"},
                                                       {"text", "Elicitation failed: " + std::string(e.what())}}})}};
            }
        });

    // Tool with elicitation and enums (SEP-1330)
    server.register_tool(
        mcp::tool_builder("test_elicitation_sep1330_enums")
            .with_description("Conformance: elicitation with enum schema improvements (SEP-1330)")
            .build(),
        [&server](const mcp::json&, const std::string& session_id) -> mcp::json {
            if (!server.client_supports_elicitation(session_id)) {
                return {{"isError", true},
                        {"content",
                         mcp::json::array({{{"type", "text"}, {"text", "Client does not support elicitation"}}})}};
            }

            // Schema with all 5 enum variants per SEP-1330
            mcp::json schema = {
                {"type", "object"},
                {"properties",
                 {// 1. Untitled single-select: simple enum array
                  {"untitledSingle",
                   {{"type", "string"},
                    {"enum", mcp::json::array({"option1", "option2", "option3"})},
                    {"description", "Untitled single-select enum"}}},
                  // 2. Titled single-select: oneOf with const/title
                  {"titledSingle",
                   {{"type", "string"},
                    {"oneOf", mcp::json::array({{{"const", "value1"}, {"title", "First Option"}},
                                                {{"const", "value2"}, {"title", "Second Option"}},
                                                {{"const", "value3"}, {"title", "Third Option"}}})},
                    {"description", "Titled single-select enum using oneOf"}}},
                  // 3. Legacy titled (deprecated): enum + enumNames
                  {"legacyEnum",
                   {{"type", "string"},
                    {"enum", mcp::json::array({"opt1", "opt2", "opt3"})},
                    {"enumNames", mcp::json::array({"Option One", "Option Two", "Option Three"})},
                    {"description", "Legacy titled enum with enumNames (deprecated)"}}},
                  // 4. Untitled multi-select: array with items.enum
                  {"untitledMulti",
                   {{"type", "array"},
                    {"items", {{"type", "string"}, {"enum", mcp::json::array({"option1", "option2", "option3"})}}},
                    {"description", "Untitled multi-select enum"}}},
                  // 5. Titled multi-select: array with items.anyOf
                  {"titledMulti",
                   {{"type", "array"},
                    {"items",
                     {{"anyOf", mcp::json::array({{{"const", "value1"}, {"title", "First Choice"}},
                                                  {{"const", "value2"}, {"title", "Second Choice"}},
                                                  {{"const", "value3"}, {"title", "Third Choice"}}})}}},
                    {"description", "Titled multi-select enum using items.anyOf"}}}}}};

            try {
                mcp::elicitation_result result = server.request_elicitation(
                    session_id, "Select your preferences from various enum types", schema);

                if (result.action == mcp::elicitation_action::accept) {
                    std::string response_text = "Elicitation completed: action=accept, content=" +
                                                result.content.dump();
                    return {{"content", mcp::json::array({{{"type", "text"}, {"text", response_text}}})}};
                }

                std::string action_str = (result.action == mcp::elicitation_action::decline) ? "decline" : "cancel";
                return {{"content", mcp::json::array({{{"type", "text"},
                                                       {"text", "Elicitation completed: action=" + action_str}}})}};
            } catch (const mcp::mcp_exception& e) {
                return {{"isError", true},
                        {"content", mcp::json::array({{{"type", "text"},
                                                       {"text", "Elicitation failed: " + std::string(e.what())}}})}};
            }
        });

    // Conformance methods ---------------------------------------------------
    server.register_method("logging/setLevel",
                           [](const mcp::json&, const std::string&) -> mcp::json { return mcp::json::object(); });

    server.register_method("completion/complete", [](const mcp::json&, const std::string&) -> mcp::json {
        return {{"completion", {{"values", mcp::json::array({"example-completion"})}}}};
    });

    server.register_method("resources/list", [](const mcp::json&, const std::string&) -> mcp::json {
        mcp::json resources = mcp::json::array({{{"uri", "test://static-text"},
                                                 {"name", "Static Text"},
                                                 {"description", "Static text resource for conformance"},
                                                 {"mimeType", "text/plain"}},
                                                {{"uri", "test://static-binary"},
                                                 {"name", "Static Binary"},
                                                 {"description", "Static binary resource for conformance"},
                                                 {"mimeType", "image/png"}},
                                                {{"uri", "test://embedded-resource"},
                                                 {"name", "Embedded Resource"},
                                                 {"description", "Embedded resource used in tool responses"},
                                                 {"mimeType", "application/json"}}});
        return {{"resources", resources}};
    });

    server.register_method("resources/read", [](const mcp::json& params, const std::string&) -> mcp::json {
        std::string uri = params.value("uri", "");
        mcp::json content;
        if (uri == "test://static-text") {
            content = {
                {"uri", uri}, {"mimeType", "text/plain"}, {"text", "This is the content of the static text resource."}};
        } else if (uri == "test://static-binary") {
            // 1x1 PNG (red)
            const std::string blob =
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/xcAAn8B9pQ5e0cAAAAASUVORK5CYII=";
            content = {{"uri", uri}, {"mimeType", "image/png"}, {"blob", blob}};
        } else if (uri.rfind("test://template/", 0) == 0 && uri.ends_with("/data")) {
            auto start = uri.find("template/") + std::string("template/").size();
            auto end = uri.rfind("/data");
            std::string id = uri.substr(start, end - start);
            content = {{"uri", uri},
                       {"mimeType", "application/json"},
                       {"text", "{\"id\":\"" + id + "\",\"templateTest\":true,\"data\":\"Data for ID: " + id + "\"}"}};
        } else if (uri == "test://embedded-resource") {
            content = {{"uri", uri},
                       {"mimeType", "application/json"},
                       {"text", R"({"embedded":true,"message":"Embedded resource content"})"}};
        }
        return {{"contents", mcp::json::array({content})}};
    });

    server.register_method("prompts/list", [](const mcp::json&, const std::string&) -> mcp::json {
        mcp::json prompts = mcp::json::array(
            {{{"name", "test_simple_prompt"},
              {"description", "Simple conformance prompt"},
              {"arguments", mcp::json::array()}},
             {{"name", "test_prompt_with_arguments"},
              {"description", "Prompt requiring arguments"},
              {"arguments",
               mcp::json::array({{{"name", "arg1"}, {"description", "First argument"}, {"required", true}},
                                 {{"name", "arg2"}, {"description", "Second argument"}, {"required", true}}})}},
             {{"name", "test_prompt_with_embedded_resource"},
              {"description", "Prompt with embedded resource"},
              {"arguments",
               mcp::json::array({{{"name", "resourceUri"}, {"description", "Resource URI"}, {"required", true}}})}},
             {{"name", "test_prompt_with_image"},
              {"description", "Prompt with image content"},
              {"arguments", mcp::json::array()}}});
        return {{"prompts", prompts}};
    });

    server.register_method("prompts/get", [](const mcp::json& params, const std::string&) -> mcp::json {
        const std::string png_base64 =
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/xcAAn8B9pQ5e0cAAAAASUVORK5CYII=";
        std::string name = params.value("name", "");
        mcp::json messages = mcp::json::array();
        if (name == "test_simple_prompt") {
            messages.push_back(
                {{"role", "user"}, {"content", {{"type", "text"}, {"text", "This is a simple prompt for testing."}}}});
        } else if (name == "test_prompt_with_arguments") {
            auto args = params.value("arguments", mcp::json::object());
            std::string arg1 = args.value("arg1", "");
            std::string arg2 = args.value("arg2", "");
            messages.push_back(
                {{"role", "user"},
                 {"content",
                  {{"type", "text"}, {"text", "Prompt with arguments: arg1='" + arg1 + "', arg2='" + arg2 + "'"}}}});
        } else if (name == "test_prompt_with_embedded_resource") {
            auto args = params.value("arguments", mcp::json::object());
            std::string uri = args.value("resourceUri", "");
            messages.push_back(
                {{"role", "user"},
                 {"content",
                  {{"type", "resource"},
                   {"resource",
                    {{"uri", uri}, {"mimeType", "text/plain"}, {"text", "Embedded resource content for testing."}}}}}});
            messages.push_back(
                {{"role", "user"}, {"content", {{"type", "text"}, {"text", "Additional embedded resource message."}}}});
        } else if (name == "test_prompt_with_image") {
            messages.push_back(
                {{"role", "user"}, {"content", {{"type", "image"}, {"data", png_base64}, {"mimeType", "image/png"}}}});
            messages.push_back(
                {{"role", "user"}, {"content", {{"type", "text"}, {"text", "Please analyze the image above."}}}});
        }
        return {{"messages", messages}};
    });

    // Start server
    std::cout << "Starting MCP server at " << srv_conf.host << ":" << srv_conf.port << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    // Use non-blocking mode so the server runs in a background thread
    // This allows the process to respond properly when run with nohup in CI
    std::cout << "Calling server.start(false)..." << std::endl;
    if (!server.start(false)) {
        std::cerr << "❌ Failed to start server" << std::endl;
        return 1;
    }
    std::cout << "✅ Server started successfully in non-blocking mode" << std::endl;
    std::cout << "Server is_running: " << (server.is_running() ? "true" : "false") << std::endl;

    // Give the server a moment to fully initialize
    std::cout << "Waiting 500ms for server to fully initialize..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "✅ Server should now be accepting connections" << std::endl;

    // Keep the main thread alive while server runs
    // The signal handlers will call server.stop() and break this loop
    std::cout << "Entering keep-alive loop..." << std::endl;
    size_t loop_count = 0;
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop_count++;
        // Log every 10 seconds to show we're alive
        if (loop_count % 100 == 0) {
            std::cout << "Server still running (loop iteration " << loop_count << ")" << std::endl;
        }
    }

    std::cout << "Server stopped after " << loop_count << " loop iterations" << std::endl;
    return 0;
}
