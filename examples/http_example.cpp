/**
 * @file http_example.cpp
 * @brief Minimal HTTP client/server example using Boost.Beast
 * 
 * This example demonstrates the low-level HTTP abstraction layer backed by Boost.Beast.
 * It shows:
 * - Creating an HTTP server with route handlers
 * - Handling GET and POST requests
 * - Creating an HTTP client
 * - Making HTTP requests and handling responses
 * 
 * This is a minimal example focused on the HTTP transport layer, separate from
 * the higher-level MCP protocol abstractions.
 * 
 * Source code: https://github.com/helynranta/cpp-mcp/blob/main/examples/http_example.cpp
 * Related APIs:
 * - HTTP Factory: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_http_factory.h
 * - HTTP Abstraction: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_http_abstraction.h
 * - Boost.Beast Adapter: https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_http_beast_adapter.h
 */

#include "mcp_http_factory.h"
#include "json.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

int main() {
    // =====================================================================
    // PART 1: Create and configure an HTTP server
    // =====================================================================
    
    std::cout << "Creating HTTP server using Boost.Beast...\n";
    auto server = mcp::http::create_server(false, "", "");  // No SSL for this example
    
    // Register a simple GET handler
    server->register_get("/hello", [](const mcp::http::request_data& req, mcp::http::response_builder& res) {
        std::cout << "GET /hello received from " << req.remote_addr << ":" << req.remote_port << "\n";
        
        // Extract query parameters if any
        std::string name = "World";
        auto it = req.params.find("name");
        if (it != req.params.end()) {
            name = it->second;
        }
        
        // Build JSON response
        json response = {
            {"message", "Hello, " + name + "!"},
            {"timestamp", std::time(nullptr)}
        };
        
        res.set_content(response.dump(), "application/json");
        res.set_status(200);
    });
    
    // Register a POST handler that echoes the request body
    server->register_post("/echo", [](const mcp::http::request_data& req, mcp::http::response_builder& res) {
        std::cout << "POST /echo received with body length: " << req.body.size() << " bytes\n";
        
        try {
            // Try to parse as JSON
            json request_body = json::parse(req.body);
            
            json response = {
                {"echo", request_body},
                {"content_length", req.body.size()},
                {"content_type", req.get_header("content-type").value_or("unknown")}
            };
            
            res.set_content(response.dump(), "application/json");
            res.set_status(200);
        } catch (const std::exception& e) {
            // If not valid JSON, return error
            json error = {
                {"error", "Invalid JSON"},
                {"details", e.what()}
            };
            res.set_content(error.dump(), "application/json");
            res.set_status(400);
        }
    });
    
    // Register a calculator endpoint
    server->register_post("/calculate", [](const mcp::http::request_data& req, mcp::http::response_builder& res) {
        std::cout << "POST /calculate received\n";
        
        try {
            json request_body = json::parse(req.body);
            
            if (!request_body.contains("operation") || 
                !request_body.contains("a") || 
                !request_body.contains("b")) {
                throw std::runtime_error("Missing required fields: operation, a, b");
            }
            
            std::string operation = request_body["operation"];
            double a = request_body["a"];
            double b = request_body["b"];
            double result = 0.0;
            
            if (operation == "add") {
                result = a + b;
            } else if (operation == "subtract") {
                result = a - b;
            } else if (operation == "multiply") {
                result = a * b;
            } else if (operation == "divide") {
                if (b == 0.0) {
                    throw std::runtime_error("Division by zero");
                }
                result = a / b;
            } else {
                throw std::runtime_error("Unknown operation: " + operation);
            }
            
            json response = {
                {"result", result},
                {"operation", operation},
                {"operands", {a, b}}
            };
            
            res.set_content(response.dump(), "application/json");
            res.set_status(200);
        } catch (const std::exception& e) {
            json error = {
                {"error", e.what()}
            };
            res.set_content(error.dump(), "application/json");
            res.set_status(400);
        }
    });
    
    // Start server in a separate thread (non-blocking)
    std::thread server_thread([&server]() {
        std::cout << "Starting HTTP server on localhost:8890...\n";
        server->listen("localhost", 8890);
    });
    
    // Give the server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // =====================================================================
    // PART 2: Create an HTTP client and make requests
    // =====================================================================
    
    std::cout << "\nCreating HTTP client using Boost.Beast...\n";
    auto client = mcp::http::create_client("http://localhost:8890");
    
    // Test 1: Simple GET request
    std::cout << "\n--- Test 1: GET /hello ---\n";
    auto result1 = client->get("/hello");
    if (result1.success) {
        std::cout << "Status: " << result1.status_code << "\n";
        std::cout << "Body: " << result1.body << "\n";
    } else {
        std::cout << "Request failed: " << result1.error_message << "\n";
    }
    
    // Test 2: GET request with query parameters
    std::cout << "\n--- Test 2: GET /hello?name=Beast ---\n";
    auto result2 = client->get("/hello?name=Beast");
    if (result2.success) {
        std::cout << "Status: " << result2.status_code << "\n";
        std::cout << "Body: " << result2.body << "\n";
    } else {
        std::cout << "Request failed: " << result2.error_message << "\n";
    }
    
    // Test 3: POST request with JSON body
    std::cout << "\n--- Test 3: POST /echo ---\n";
    json echo_data = {
        {"message", "Hello from Beast client!"},
        {"timestamp", std::time(nullptr)}
    };
    mcp::http::headers_map headers;
    auto result3 = client->post("/echo", headers, echo_data.dump(), "application/json");
    if (result3.success) {
        std::cout << "Status: " << result3.status_code << "\n";
        std::cout << "Body: " << result3.body << "\n";
    } else {
        std::cout << "Request failed: " << result3.error_message << "\n";
    }
    
    // Test 4: Calculator endpoint
    std::cout << "\n--- Test 4: POST /calculate (10 + 5) ---\n";
    json calc_data = {
        {"operation", "add"},
        {"a", 10},
        {"b", 5}
    };
    auto result4 = client->post("/calculate", headers, calc_data.dump(), "application/json");
    if (result4.success) {
        std::cout << "Status: " << result4.status_code << "\n";
        std::cout << "Body: " << result4.body << "\n";
    } else {
        std::cout << "Request failed: " << result4.error_message << "\n";
    }
    
    // Test 5: Error handling (invalid JSON)
    std::cout << "\n--- Test 5: POST /echo with invalid JSON ---\n";
    auto result5 = client->post("/echo", headers, "not valid json{", "application/json");
    if (result5.success) {
        std::cout << "Status: " << result5.status_code << "\n";
        std::cout << "Body: " << result5.body << "\n";
    } else {
        std::cout << "Request failed: " << result5.error_message << "\n";
    }
    
    std::cout << "\n=====================================================\n";
    std::cout << "Example completed. Press Ctrl+C to stop the server.\n";
    std::cout << "=====================================================\n";
    
    // Keep server running
    server_thread.join();
    
    return 0;
}
