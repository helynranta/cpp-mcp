/**
 * @file streamable_http_transport_test.cpp
 * @brief Test Streamable HTTP transport compliance with MCP 2025-03-26 specification
 * 
 * Tests unified /mcp endpoint, Mcp-Session-Id header semantics, GET/POST/DELETE methods,
 * and session lifecycle according to the MCP 2025-03-26 specification.
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "mcp_message.h"
#include "mcp_http_factory.h"
#include <thread>
#include <chrono>
#include <regex>

using namespace mcp;
using json = nlohmann::ordered_json;

// Test fixture for Streamable HTTP transport tests - each test gets isolated server
class StreamableHttpTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create server on unique port for this test (avoid conflicts)
        static std::atomic<int> port_counter{17000};
        port_ = port_counter.fetch_add(1);
        
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
        config.name = "StreamableHttpTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Set server capabilities
        json server_capabilities = {
            {"tools", {{"listChanged", true}}}
        };
        server_->set_capabilities(server_capabilities);
        
        // Register a test tool
        tool test_tool = tool_builder("echo")
            .with_description("Echo tool for testing")
            .with_string_param("message", "Message to echo", "")
            .build();
        
        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            std::string msg = params.value("message", "");
            return {{"echo", msg}};
        });
        
        // Start server
        server_->start(false);
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::string base_url = "http://localhost:" + std::to_string(port_);
        http_client = http::create_client(base_url);
        http_client->set_read_timeout(5); // 5 seconds timeout
    }

    void TearDown() override {
        http_client.reset();
        
        // Stop and clean up server
        if (server_) {
            server_->stop();
        }
        server_.reset();
    }
    
    int port_;
    std::unique_ptr<server> server_;
    // Helper to extract session ID from Mcp-Session-Id header
    std::string extract_session_id(const http::client_result& res) {
        if (!res) return "";
        
        auto it = res.headers.find("Mcp-Session-Id");
        if (it != res.headers.end()) {
            return it->second;
        }
        return "";
    }
    
    // Helper to establish SSE connection and get endpoint (simplified)
    std::string establish_sse_session_simple() {
        std::string endpoint;
        bool got_endpoint = false;
        std::mutex mtx;
        std::condition_variable cv;
        std::string sse_url = "http://localhost:" + std::to_string(port_);
        auto sse_client = http::create_client(sse_url);
        std::atomic<http::client_interface*> client_ptr{nullptr};
        auto* sse_client_ptr = sse_client.get();  // Capture raw pointer for thread safety
        
        std::thread sse_thread([&, sse_client_ptr]() {
            client_ptr.store(sse_client_ptr, std::memory_order_release);
            
            auto res = sse_client_ptr->get_stream("/mcp", 
                [&](const char* data, size_t len) {
                    std::string response(data, len);
                    
                    // Extract session endpoint
                    if (response.find("endpoint") != std::string::npos) {
                        size_t pos = response.find("data: ");
                        if (pos != std::string::npos) {
                            std::string endpoint_data = response.substr(pos + 6);
                            endpoint_data = endpoint_data.substr(0, endpoint_data.find("\r\n"));
                            
                            std::lock_guard<std::mutex> lock(mtx);
                            endpoint = endpoint_data;
                            got_endpoint = true;
                            cv.notify_one();
                            return false; // Stop reading
                        }
                    }
                    return true;
                });
            
            client_ptr.store(nullptr, std::memory_order_release);
        });
        
        // Wait for endpoint
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(3), [&] { return got_endpoint; });
        }
        
        // Stop the client before detaching to avoid crashes during teardown
        auto client = client_ptr.load(std::memory_order_acquire);
        if (client) {
            client->stop();
        }
        
        if (sse_thread.joinable()) {
            // Give it a moment to exit after stop()
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (sse_thread.joinable()) {
                sse_thread.detach();
            }
        }
        
        return endpoint;
    }

    std::unique_ptr<http::client_interface> http_client;
};

// Test: GET /mcp establishes SSE connection and returns Mcp-Session-Id header
TEST_F(StreamableHttpTransportTest, GetMcpEstablishesSession) {
    std::string endpoint = establish_sse_session_simple();
    
    EXPECT_FALSE(endpoint.empty()) << "Should receive endpoint in SSE stream";
    EXPECT_NE(endpoint.find("/mcp"), std::string::npos) 
        << "Endpoint should be /mcp for streamable HTTP transport";
}

// Test: POST /mcp with Mcp-Session-Id header uses existing session
TEST_F(StreamableHttpTransportTest, PostMcpWithSessionHeader) {
    // First, establish a session via GET
    std::string endpoint = establish_sse_session_simple();
    
    // Extract session_id from endpoint query parameter
    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }
    
    ASSERT_FALSE(session_id.empty()) << "Should have a session ID from GET /mcp";
    
    // Now send a POST request with Mcp-Session-Id header
    json ping_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "ping"}
    };
    
    http::headers_map headers = {
        {"Mcp-Session-Id", session_id}
    };
    
    auto res = http_client->post("/mcp", headers, ping_request.dump(), "application/json");
    
    ASSERT_TRUE(res.success) << "POST /mcp should succeed";
    EXPECT_EQ(202, res.status_code) << "Should return 202 Accepted for async processing";
    
    // Verify session ID is echoed back in response
    std::string response_session_id = extract_session_id(res);
    EXPECT_EQ(session_id, response_session_id) << "Response should include same session ID";
}

// Test: POST /mcp without session returns 404
TEST_F(StreamableHttpTransportTest, PostMcpWithoutSessionReturns404) {
    json test_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"}
    };
    
    // POST without session ID
    http::headers_map empty_headers;
    auto res = http_client->post("/mcp", empty_headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(res.success) << "POST /mcp should return a response";
    EXPECT_EQ(404, res.status_code) << "Should return 404 for non-existent session";
}

// Test: POST /mcp with invalid session ID returns 404
TEST_F(StreamableHttpTransportTest, PostMcpWithInvalidSessionReturns404) {
    json test_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"}
    };
    
    http::headers_map headers = {
        {"Mcp-Session-Id", "invalid-session-id-12345"}
    };
    
    auto res = http_client->post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(res.success) << "POST /mcp should return a response";
    EXPECT_EQ(404, res.status_code) << "Should return 404 for invalid session";
}

// Test: DELETE /mcp terminates session
TEST_F(StreamableHttpTransportTest, DeleteMcpTerminatesSession) {
    // First, establish a session
    std::string endpoint = establish_sse_session_simple();
    
    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }
    
    ASSERT_FALSE(session_id.empty()) << "Should have a session ID";
    
    // Delete the session
    http::headers_map headers = {
        {"Mcp-Session-Id", session_id}
    };
    
    // DELETE method not yet implemented in Beast client
    // This test is disabled until DELETE is implemented
    GTEST_SKIP() << "DELETE method not yet implemented in Beast client";
    
    // auto delete_res = http_client->delete_("/mcp", headers);
    // ASSERT_TRUE(delete_res) << "DELETE /mcp should succeed";
    // EXPECT_EQ(204, delete_res.status_code) << "Should return 204 No Content on successful deletion";
    
    // Verify session is gone by trying to POST with a real method (not ping)
    json test_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"}
    };
    
    auto post_res = http_client->post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(post_res) << "POST /mcp should return a response";
    EXPECT_EQ(404, post_res.status_code) << "Should return 404 after session deleted";
}

// Test: DELETE /mcp without session ID returns 400
TEST_F(StreamableHttpTransportTest, DeleteMcpWithoutSessionReturns400) {
    // DELETE method not yet implemented in Beast client
    GTEST_SKIP() << "DELETE method not yet implemented in Beast client";
    
    // auto res = http_client->delete_("/mcp");
    // ASSERT_TRUE(res.success) << "DELETE /mcp should return a response";
    // EXPECT_EQ(400, res.status_code) << "Should return 400 for missing session ID";
}

// Test: DELETE /mcp with invalid session ID returns 404
TEST_F(StreamableHttpTransportTest, DeleteMcpWithInvalidSessionReturns404) {
    // DELETE method not yet implemented in Beast client
    GTEST_SKIP() << "DELETE method not yet implemented in Beast client";
    
    // http::headers_map headers = {
    //     {"Mcp-Session-Id", "invalid-session-12345"}
    // };
    // auto res = http_client->delete_("/mcp", headers);
    // ASSERT_TRUE(res.success) << "DELETE /mcp should return a response";
    // EXPECT_EQ(404, res.status_code) << "Should return 404 for non-existent session";
}

// Test: Backward compatibility with query parameter
TEST_F(StreamableHttpTransportTest, BackwardCompatibilityWithQueryParameter) {
    // Establish a session
    std::string endpoint = establish_sse_session_simple();
    
    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }
    
    ASSERT_FALSE(session_id.empty()) << "Should have a session ID";
    
    // POST using query parameter instead of header
    json ping_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "ping"}
    };
    
    std::string url_with_session = "/mcp?session_id=" + session_id;
    http::headers_map headers;
    auto res = http_client->post(url_with_session, headers, ping_request.dump(), "application/json");
    
    ASSERT_TRUE(res.success) << "POST /mcp with query param should succeed";
    EXPECT_EQ(202, res.status_code) << "Should return 202 Accepted";
}

// Test: CORS headers include Mcp-Session-Id
TEST_F(StreamableHttpTransportTest, CorsHeadersIncludeMcpSessionId) {
    // OPTIONS method not yet implemented in Beast client
    GTEST_SKIP() << "OPTIONS method not yet implemented in Beast client";
    
    // auto res = http_client->options("/mcp");
    // ASSERT_TRUE(res.success) << "OPTIONS /mcp should succeed";
    // EXPECT_EQ(204, res.status_code) << "Should return 204 No Content";
    
    // // Check CORS headers
    // auto allow_methods = res.headers.find("Access-Control-Allow-Methods");
    // ASSERT_NE(allow_methods, res.headers.end()) << "Should have Allow-Methods header";
    // EXPECT_NE(allow_methods->second.find("GET"), std::string::npos) << "Should allow GET";
    // EXPECT_NE(allow_methods->second.find("POST"), std::string::npos) << "Should allow POST";
    // EXPECT_NE(allow_methods->second.find("DELETE"), std::string::npos) << "Should allow DELETE";
    // 
    // auto allow_headers = res.headers.find("Access-Control-Allow-Headers");
    // ASSERT_NE(allow_headers, res.headers.end()) << "Should have Allow-Headers header";
    // EXPECT_NE(allow_headers->second.find("Mcp-Session-Id"), std::string::npos) 
    //     << "Should allow Mcp-Session-Id header";
}

// Test: Legacy /sse endpoint still works
TEST_F(StreamableHttpTransportTest, LegacySseEndpointWorks) {
    std::string endpoint;
    bool got_endpoint = false;
    std::mutex mtx;
    std::condition_variable cv;
    std::string sse_url = "http://localhost:" + std::to_string(port_);
    auto sse_client = http::create_client(sse_url);
    std::atomic<http::client_interface*> client_ptr{nullptr};
    auto* sse_client_ptr = sse_client.get();  // Capture raw pointer for thread safety
    
    std::thread sse_thread([&, sse_client_ptr]() {
        client_ptr.store(sse_client_ptr, std::memory_order_release);
        
        sse_client_ptr->get_stream("/sse", [&](const char* data, size_t len) {
            std::string response(data, len);
            
            if (response.find("endpoint") != std::string::npos) {
                size_t pos = response.find("data: ");
                if (pos != std::string::npos) {
                    std::string endpoint_data = response.substr(pos + 6);
                    endpoint_data = endpoint_data.substr(0, endpoint_data.find("\r\n"));
                    
                    std::lock_guard<std::mutex> lock(mtx);
                    endpoint = endpoint_data;
                    got_endpoint = true;
                    cv.notify_one();
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return true;
        });
        
        client_ptr.store(nullptr, std::memory_order_release);
    });
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(3), [&] { return got_endpoint; });
    }
    
    // Stop the client before detaching to avoid crashes during teardown
    auto client = client_ptr.load(std::memory_order_acquire);
    if (client) {
        client->stop();
    }
    
    if (sse_thread.joinable()) {
        // Give it a moment to exit after stop()
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (sse_thread.joinable()) {
            sse_thread.detach();
        }
    }
    
    EXPECT_FALSE(endpoint.empty()) << "Legacy /sse endpoint should still work";
    EXPECT_NE(endpoint.find("/message"), std::string::npos) 
        << "Should return /message endpoint for backward compatibility";
}

// Test: POST with unsupported Accept header returns 406
TEST_F(StreamableHttpTransportTest, PostMcpWithUnsupportedAcceptReturns406) {
    // Establish a session
    std::string endpoint = establish_sse_session_simple();
    
    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }
    
    ASSERT_FALSE(session_id.empty()) << "Should have a session ID";
    
    // POST with unsupported Accept header (e.g., only text/html)
    json test_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"}
    };
    
    http::headers_map headers = {
        {"Mcp-Session-Id", session_id},
        {"Accept", "text/html"}  // Unsupported media type
    };
    
    auto res = http_client->post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(res.success) << "POST /mcp should return a response";
    EXPECT_EQ(406, res.status_code) << "Should return 406 Not Acceptable for unsupported Accept header";
}

// Test: POST with valid Accept headers (application/json and text/event-stream) succeeds
TEST_F(StreamableHttpTransportTest, PostMcpWithValidAcceptHeaderSucceeds) {
    // Establish a session
    std::string endpoint = establish_sse_session_simple();
    
    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }
    
    ASSERT_FALSE(session_id.empty()) << "Should have a session ID";
    
    // POST with proper Accept header
    json test_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "ping"}
    };
    
    http::headers_map headers = {
        {"Mcp-Session-Id", session_id},
        {"Accept", "application/json, text/event-stream"}  // Both supported types
    };
    
    auto res = http_client->post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(res.success) << "POST /mcp should succeed";
    EXPECT_EQ(202, res.status_code) << "Should return 202 Accepted with valid Accept header";
}

// Test: CORS headers include Accept in allowed headers
// DISABLED: OPTIONS method not yet implemented in Beast client
TEST_F(StreamableHttpTransportTest, DISABLED_CorsHeadersIncludeAccept) {
    // TODO: Re-enable when OPTIONS method is implemented
}
