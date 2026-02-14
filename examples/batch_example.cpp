/**
 * @file batch_example.cpp
 * @brief Example demonstrating JSON-RPC batch request support
 * 
 * This example shows how the MCP server handles batch requests according to
 * the JSON-RPC 2.0 specification. It demonstrates:
 * - Batch requests with multiple operations
 * - Mixed batches (requests + notifications)
 * - Notification-only batches
 * - Empty batch validation
 */

#include "mcp_server.h"
#include "mcp_message.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace mcp;

void print_batch_example(const std::string& title, const json& batch) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << "Batch request JSON:" << std::endl;
    std::cout << batch.dump(2) << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "JSON-RPC Batch Request Examples" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Example 1: Batch with multiple requests
    {
        json batch = json::array();
        
        request req1 = request::create("tools/list", json::object());
        request req2 = request::create("resources/list", json::object());
        
        batch.push_back(req1.to_json());
        batch.push_back(req2.to_json());
        
        print_batch_example("Multiple Requests Batch", batch);
        
        std::cout << "Expected behavior:" << std::endl;
        std::cout << "- Server processes both requests" << std::endl;
        std::cout << "- Returns array with 2 responses" << std::endl;
        std::cout << "- Responses match request IDs" << std::endl;
    }
    
    // Example 2: Mixed batch (requests + notifications)
    {
        json batch = json::array();
        
        request req = request::create("tools/list", json::object());
        request notif = request::create_notification("status_update", {{"status", "running"}});
        
        batch.push_back(req.to_json());
        batch.push_back(notif.to_json());
        
        print_batch_example("Mixed Batch (Request + Notification)", batch);
        
        std::cout << "Expected behavior:" << std::endl;
        std::cout << "- Server processes both items" << std::endl;
        std::cout << "- Returns array with 1 response (for request only)" << std::endl;
        std::cout << "- Notification is processed but gets no response" << std::endl;
    }
    
    // Example 3: Notification-only batch
    {
        json batch = json::array();
        
        request notif1 = request::create_notification("log", {{"message", "Event 1"}});
        request notif2 = request::create_notification("log", {{"message", "Event 2"}});
        
        batch.push_back(notif1.to_json());
        batch.push_back(notif2.to_json());
        
        print_batch_example("Notification-Only Batch", batch);
        
        std::cout << "Expected behavior:" << std::endl;
        std::cout << "- Server processes both notifications" << std::endl;
        std::cout << "- Returns HTTP 202 Accepted" << std::endl;
        std::cout << "- No response body" << std::endl;
    }
    
    // Example 4: Empty batch (invalid)
    {
        json batch = json::array();
        
        print_batch_example("Empty Batch (Invalid)", batch);
        
        std::cout << "Expected behavior:" << std::endl;
        std::cout << "- Server rejects with HTTP 400" << std::endl;
        std::cout << "- Returns error: 'Invalid Request: batch cannot be empty'" << std::endl;
    }
    
    // Example 5: Single item batch
    {
        json batch = json::array();
        
        request req = request::create("ping", json::object());
        batch.push_back(req.to_json());
        
        print_batch_example("Single Item Batch", batch);
        
        std::cout << "Expected behavior:" << std::endl;
        std::cout << "- Server processes the single request" << std::endl;
        std::cout << "- Returns array with 1 response" << std::endl;
    }
    
    std::cout << "\n=== Implementation Notes ===" << std::endl;
    std::cout << "1. Batch requests are sent as JSON arrays" << std::endl;
    std::cout << "2. Each array element is a valid JSON-RPC request or notification" << std::endl;
    std::cout << "3. Responses are returned in the same order as requests" << std::endl;
    std::cout << "4. Notifications in a batch do not generate responses" << std::endl;
    std::cout << "5. Empty batches are invalid per JSON-RPC 2.0 spec" << std::endl;
    std::cout << "6. Batch processing maintains backward compatibility with single requests" << std::endl;
    
    return 0;
}
