/**
 * @file progress_example.cpp
 * @brief Progress notification example
 * 
 * This example demonstrates MCP's real-time progress notification system.
 * Shows how to:
 * - Send progress notifications from a server during long-running operations
 * - Handle progress notifications on the client side
 * - Use progress tokens to track operations
 * 
 * Demonstrates both with and without progress tokens to show the difference.
 * 
 * Source code: https://github.com/helynranta/cpp-mcp/blob/main/examples/progress_example.cpp
 * Related APIs:
 * - Progress API: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_progress.h
 * - MCP Server: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_server.h
 */

#include "mcp_server.h"
#include "mcp_sse_client.h"
#include "mcp_tool.h"
#include "mcp_progress.h"
#include "mcp_logger.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace mcp;

int main() {
    LOG_INFO("=== MCP Progress Notification Example ===\n");

    // Create and configure the server
    server::configuration srv_conf;
    srv_conf.host = "localhost";
    srv_conf.port = 8891;
    srv_conf.name = "Progress Demo Server";
    srv_conf.version = "1.0.0";

    server srv(srv_conf);
    srv.set_server_info("Progress Demo Server", "1.0.0");

    // Register a long-running tool that sends progress notifications
    tool long_operation = tool_builder("long_operation")
        .with_description("Simulates a long-running operation with progress updates")
        .with_number_param("steps", "Number of steps to process", 10.0)
        .with_number_param("step_duration_ms", "Duration of each step in milliseconds", 100.0)
        .build();

    srv.register_tool(long_operation, [&srv](const json& params, const std::string& session_id) -> json {
        // Extract parameters
        int steps = params.value("steps", 10);
        int step_duration_ms = params.value("step_duration_ms", 100);
        
        // Check for progress token
        auto progress_token = progress_tracker::extract_progress_token(params);
        
        LOG_INFO("Starting long operation with ", steps, " steps");
        if (progress_token.has_value()) {
            LOG_INFO("Progress token found: ", progress_token.value().dump());
        }

        // Process steps and send progress notifications
        for (int i = 1; i <= steps; ++i) {
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(step_duration_ms));
            
            // Send progress notification if token is available
            if (progress_token.has_value()) {
                std::string message = "Processing step " + std::to_string(i) + " of " + std::to_string(steps);
                
                progress_notification notif = progress_notification::create(
                    progress_token.value(),
                    static_cast<double>(i),
                    static_cast<double>(steps),
                    message
                );
                
                srv.send_progress(session_id, notif);
                LOG_INFO("Sent progress: ", i, "/", steps, " - ", message);
            }
        }
        
        LOG_INFO("Long operation completed");
        
        // Return result
        return json::array({
            {
                {"type", "text"},
                {"text", "Operation completed successfully with " + std::to_string(steps) + " steps"}
            }
        });
    });

    // Start server in non-blocking mode
    LOG_INFO("Starting server on http://", srv_conf.host, ":", srv_conf.port);
    std::thread server_thread([&srv]() {
        srv.start(true);
    });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Create client
    std::string server_url = "http://" + srv_conf.host + ":" + std::to_string(srv_conf.port);
    sse_client client(server_url);

    // Set up progress handler
    LOG_INFO("\n=== Setting up progress handler ===");
    client.set_progress_handler([](const progress_notification& notif) {
        std::cout << "[PROGRESS] ";
        
        if (notif.total.has_value()) {
            double percentage = (notif.progress / notif.total.value()) * 100.0;
            std::cout << static_cast<int>(percentage) << "% ";
            std::cout << "(" << static_cast<int>(notif.progress) << "/" 
                      << static_cast<int>(notif.total.value()) << ") ";
        } else {
            std::cout << "Progress: " << notif.progress << " ";
        }
        
        if (notif.message.has_value()) {
            std::cout << "- " << notif.message.value();
        }
        
        std::cout << std::endl;
    });

    // Initialize client
    LOG_INFO("Initializing client...");
    if (!client.initialize("Progress Demo Client", "1.0.0")) {
        LOG_ERROR("Failed to initialize client");
        srv.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
        return 1;
    }
    LOG_INFO("Client initialized successfully\n");

    // Call the tool WITH progress token
    LOG_INFO("=== Calling tool WITH progress token ===");
    try {
        json params = {
            {"steps", 5},
            {"step_duration_ms", 200},
            {"_meta", {
                {"progressToken", "operation-001"}
            }}
        };
        
        json result = client.call_tool("long_operation", params);
        LOG_INFO("Tool call completed");
        LOG_INFO("Result: ", result.dump(2));
    } catch (const mcp_exception& e) {
        LOG_ERROR("Error calling tool: ", e.what());
    }

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Call the tool WITHOUT progress token (no progress notifications)
    LOG_INFO("\n=== Calling tool WITHOUT progress token ===");
    try {
        json params = {
            {"steps", 3},
            {"step_duration_ms", 150}
        };
        
        json result = client.call_tool("long_operation", params);
        LOG_INFO("Tool call completed");
        LOG_INFO("Result: ", result.dump(2));
    } catch (const mcp_exception& e) {
        LOG_ERROR("Error calling tool: ", e.what());
    }

    // Cleanup
    LOG_INFO("\n=== Cleaning up ===");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    srv.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    LOG_INFO("Example completed successfully");
    return 0;
}
