/**
 * @file batch_rejection_test.cpp
 * @brief Tests for JSON-RPC batch request rejection (MCP 2025-06-18+)
 * 
 * MCP 2025-06-18 removed JSON-RPC batching support. This test verifies that
 * the server properly rejects batch requests with appropriate error responses.
 */

#include <boost/test/unit_test.hpp>

#include "mcp_server.h"
#include "mcp_message.h"
#include "mcp_http_factory.h"
#include <thread>
#include <chrono>

using namespace mcp;

// Test fixture for server setup
struct BatchRejectionServerFixture {
    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<http::client_interface> http_client;
    
    BatchRejectionServerFixture() {
        // Create server on unique port (avoid conflicts)
        static std::atomic<int> port_counter{18000};
        port_ = port_counter.fetch_add(1);
        
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
        config.name = "BatchRejectionTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Set server capabilities
        json server_capabilities = {
            {"tools", {{"listChanged", true}}}
        };
        server_->set_capabilities(server_capabilities);
        
        // Register a simple test tool
        tool test_tool = tool_builder("test_tool")
            .with_description("Test tool")
            .with_string_param("input", "Test input", "")
            .build();
        
        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            return {
                {"content", json::array({
                    {{"type", "text"}, {"text", "Test response"}}
                })},
                {"isError", false}
            };
        });
        
        // Start server (non-blocking)
        server_->start(false);
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Create HTTP client
        std::string base_url = "http://localhost:" + std::to_string(port_);
        http_client = http::create_client(base_url);
        http_client->set_read_timeout(5); // 5 seconds timeout
    }
    
    ~BatchRejectionServerFixture() {
        http_client.reset();
        
        // Stop and clean up server
        if (server_) {
            server_->stop();
        }
        server_.reset();
    }
};

BOOST_FIXTURE_TEST_SUITE(BatchRejectionTestSuite, BatchRejectionServerFixture)

/**
 * Test that the server rejects batch request arrays
 * Per MCP 2025-06-18: JSON-RPC batching is NOT supported
 */
BOOST_AUTO_TEST_CASE(RejectsBatchRequestArray) {
    // Create a batch request (array of JSON-RPC messages)
    json batch = json::array();
    
    request req1 = request::create("tools/list", json::object());
    request req2 = request::create("resources/list", json::object());
    
    batch.push_back(req1.to_json());
    batch.push_back(req2.to_json());
    
    // Send batch request to server via HTTP client
    auto res = http_client->post(
        "/mcp",
        {{"Content-Type", "application/json"}},
        batch.dump(),
        "application/json"
    );
    
    // Verify server rejected the batch request
    BOOST_CHECK_EQUAL(res.status_code, 400);  // Bad Request
    
    // Parse response
    json response_json;
    BOOST_REQUIRE_NO_THROW(response_json = json::parse(res.body));
    
    // Verify error response structure
    BOOST_CHECK(response_json.contains("error"));
    BOOST_CHECK_EQUAL(response_json["error"]["code"], -32600);  // Invalid Request
    
    // Verify error message indicates batching not supported
    std::string error_message = response_json["error"]["message"];
    BOOST_CHECK(error_message.find("batch") != std::string::npos ||
                error_message.find("array") != std::string::npos ||
                error_message.find("not supported") != std::string::npos);
}

/**
 * Test that single requests still work correctly
 * Ensure we didn't break non-batch functionality
 */
BOOST_AUTO_TEST_CASE(AcceptsSingleRequest) {
    // Create a single request (not an array) - use ping which doesn't require session
    request req = request::create("ping", json::object());
    json single_request = req.to_json();
    
    // Send single request to server
    auto res = http_client->post(
        "/mcp",
        {{"Content-Type", "application/json"}},
        single_request.dump(),
        "application/json"
    );
    
    // Verify server accepted the request (ping returns 202 Accepted)
    BOOST_CHECK_EQUAL(res.status_code, 202);  // Accepted
    
    // For ping, we should get "Accepted" response
    BOOST_CHECK_EQUAL(res.body, "Accepted");
}

/**
 * Test that empty batch array is also rejected
 */
BOOST_AUTO_TEST_CASE(RejectsEmptyBatchArray) {
    // Create an empty batch array
    json batch = json::array();
    
    // Send empty batch to server
    auto res = http_client->post(
        "/mcp",
        {{"Content-Type", "application/json"}},
        batch.dump(),
        "application/json"
    );
    
    // Verify server rejected the empty batch
    BOOST_CHECK_EQUAL(res.status_code, 400);  // Bad Request
    
    // Parse response
    json response_json;
    BOOST_REQUIRE_NO_THROW(response_json = json::parse(res.body));
    
    // Verify error response
    BOOST_CHECK(response_json.contains("error"));
    BOOST_CHECK_EQUAL(response_json["error"]["code"], -32600);  // Invalid Request
}

/**
 * Test error message content
 * Verify the error message is helpful and indicates batching is not supported
 */
BOOST_AUTO_TEST_CASE(ErrorMessageIndicatesBatchingNotSupported) {
    // Create a batch request
    json batch = json::array();
    request req = request::create("tools/list", json::object());
    batch.push_back(req.to_json());
    
    // Send batch request
    auto res = http_client->post(
        "/mcp",
        {{"Content-Type", "application/json"}},
        batch.dump(),
        "application/json"
    );
    
    // Parse response
    json response_json = json::parse(res.body);
    
    // Verify error message is informative
    BOOST_REQUIRE(response_json.contains("error"));
    BOOST_REQUIRE(response_json["error"].contains("message"));
    
    std::string error_message = response_json["error"]["message"];
    
    // Error message should mention:
    // - "batch" or "batching"
    // - "not supported" or "removed"
    // - Ideally mention the spec version
    bool mentions_batch = error_message.find("batch") != std::string::npos;
    bool mentions_not_supported = error_message.find("not supported") != std::string::npos ||
                                   error_message.find("removed") != std::string::npos;
    
    BOOST_CHECK(mentions_batch || error_message.find("array") != std::string::npos);
    BOOST_CHECK(mentions_not_supported);
}

BOOST_AUTO_TEST_SUITE_END()
