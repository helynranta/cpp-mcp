/**
 * @file session_state_example.cpp
 * @brief Example demonstrating session state management
 * 
 * This example shows how to use the session state storage API to maintain
 * state for each connected client session.
 * 
 * Related:
 * - GitHub: https://github.com/helynranta/cpp-mcp/tree/main/examples/session_state_example.cpp
 * - Server API: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_server.h
 */

#include "mcp_server.h"
#include "mcp_tool.h"
#include "mcp_logger.h"
#include <iostream>
#include <csignal>

using namespace mcp;
using json = nlohmann::ordered_json;

// Global server pointer for signal handling
static server* g_server = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived SIGINT, stopping server..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
    }
}

int main() {
    // Server configuration
    server::configuration config;
    config.host = "localhost";
    config.port = 8080;
    config.name = "Session State Example Server";
    config.version = "1.0.0";
    
    // Create server
    server srv(config);
    g_server = &srv;
    
    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    
    // Set server capabilities
    json capabilities = {
        {"tools", {{"listChanged", true}}}
    };
    srv.set_capabilities(capabilities);
    
    // Register a tool that stores session state
    tool increment_counter = tool_builder("increment_counter")
        .with_description("Increments a counter for the current session")
        .build();
    
    srv.register_tool(increment_counter, [&srv](const json& params, const std::string& session_id) -> json {
        LOG_INFO("increment_counter called for session: ", session_id);
        
        // Get current session state
        json state = srv.get_session_state(session_id);
        
        // Initialize counter if it doesn't exist
        int counter = 0;
        if (!state.is_null() && state.contains("counter")) {
            counter = state["counter"];
        }
        
        // Increment counter
        counter++;
        
        // Update session state with timestamp (stored as nanoseconds since epoch)
        state["counter"] = counter;
        state["last_updated_ns"] = std::chrono::system_clock::now().time_since_epoch().count();
        srv.set_session_state(session_id, state);
        
        // Return current counter value
        return {
            {"counter", counter},
            {"message", "Counter incremented successfully"}
        };
    });
    
    // Register a tool that retrieves session state
    tool get_state = tool_builder("get_state")
        .with_description("Gets the current session state")
        .build();
    
    srv.register_tool(get_state, [&srv](const json& params, const std::string& session_id) -> json {
        LOG_INFO("get_state called for session: ", session_id);
        
        // Get current session state
        json state = srv.get_session_state(session_id);
        
        if (state.is_null() || state.empty()) {
            return {
                {"message", "No state data for this session"},
                {"state", json::object()}
            };
        }
        
        return {
            {"message", "Session state retrieved successfully"},
            {"state", state}
        };
    });
    
    // Register a tool that stores custom data
    tool set_user_data = tool_builder("set_user_data")
        .with_description("Stores user data in session state")
        .with_string_param("key", "The key to store data under", "")
        .with_string_param("value", "The value to store", "")
        .build();
    
    srv.register_tool(set_user_data, [&srv](const json& params, const std::string& session_id) -> json {
        LOG_INFO("set_user_data called for session: ", session_id);
        
        // Extract parameters
        std::string key = params["key"];
        std::string value = params["value"];
        
        // Get current session state
        json state = srv.get_session_state(session_id);
        
        // Store the data
        if (state.is_null()) {
            state = json::object();
        }
        state[key] = value;
        
        // Update session state
        srv.set_session_state(session_id, state);
        
        return {
            {"message", "Data stored successfully"},
            {"key", key},
            {"value", value}
        };
    });
    
    // Register session cleanup handler
    srv.register_session_cleanup("session_state_example", [&srv](const std::string& session_id) {
        LOG_INFO("Session cleanup for: ", session_id);
        
        // Get final state before cleanup
        json state = srv.get_session_state(session_id);
        if (!state.is_null() && !state.empty()) {
            LOG_INFO("Final session state: ", state.dump());
        }
        
        // State will be automatically cleared by close_session
    });
    
    std::cout << "=== Session State Example Server ===" << std::endl;
    std::cout << "Server: " << config.name << " v" << config.version << std::endl;
    std::cout << "Listening on: " << config.host << ":" << config.port << std::endl;
    std::cout << std::endl;
    std::cout << "Available tools:" << std::endl;
    std::cout << "  - increment_counter: Increments a per-session counter" << std::endl;
    std::cout << "  - get_state: Retrieves current session state" << std::endl;
    std::cout << "  - set_user_data: Stores custom key-value data in session" << std::endl;
    std::cout << std::endl;
    std::cout << "Session state features:" << std::endl;
    std::cout << "  - Each session has independent state storage" << std::endl;
    std::cout << "  - State persists across tool calls within a session" << std::endl;
    std::cout << "  - State is automatically cleaned up on disconnect" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Start server (blocking)
    bool started = srv.start(true);
    
    if (!started) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "Server stopped gracefully" << std::endl;
    return 0;
}
