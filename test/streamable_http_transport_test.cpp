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
#include "httplib.h"
#include <thread>
#include <chrono>
#include <regex>

using namespace mcp;
using json = nlohmann::ordered_json;

// Test environment for Streamable HTTP transport tests
class StreamableHttpTransportEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = 9092; // Different port to avoid conflicts
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
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void TearDown() override {
        if (server_) {
            server_->stop();
        }
        server_.reset();
    }

    static std::unique_ptr<server>& GetServer() {
        return server_;
    }

private:
    static std::unique_ptr<server> server_;
};

std::unique_ptr<server> StreamableHttpTransportEnvironment::server_;

// Test fixture for Streamable HTTP transport tests
class StreamableHttpTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_client = std::make_unique<httplib::Client>("localhost", 9092);
        http_client->set_read_timeout(5, 0); // 5 seconds timeout
    }

    void TearDown() override {
        http_client.reset();
    }
    
    // Helper to extract session ID from Mcp-Session-Id header
    std::string extract_session_id(const httplib::Result& res) {
        if (!res) return "";
        
        auto it = res->headers.find("Mcp-Session-Id");
        if (it != res->headers.end()) {
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
        
        std::thread sse_thread([&]() {
            httplib::Client sse_client("localhost", 9092);
            auto res = sse_client.Get("/mcp", 
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
        });
        
        // Wait for endpoint
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(3), [&] { return got_endpoint; });
        }
        
        if (sse_thread.joinable()) {
            sse_thread.detach();
        }
        
        return endpoint;
    }

    std::unique_ptr<httplib::Client> http_client;
};

// Register test environment
::testing::Environment* const streamable_http_env =
    ::testing::AddGlobalTestEnvironment(new StreamableHttpTransportEnvironment);

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
    
    httplib::Headers headers = {
        {"Mcp-Session-Id", session_id}
    };
    
    auto res = http_client->Post("/mcp", headers, ping_request.dump(), "application/json");
    
    ASSERT_TRUE(res) << "POST /mcp should succeed";
    EXPECT_EQ(202, res->status) << "Should return 202 Accepted for async processing";
    
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
    auto res = http_client->Post("/mcp", test_request.dump(), "application/json");
    
    ASSERT_TRUE(res) << "POST /mcp should return a response";
    EXPECT_EQ(404, res->status) << "Should return 404 for non-existent session";
}

// Test: POST /mcp with invalid session ID returns 404
TEST_F(StreamableHttpTransportTest, PostMcpWithInvalidSessionReturns404) {
    json test_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"}
    };
    
    httplib::Headers headers = {
        {"Mcp-Session-Id", "invalid-session-id-12345"}
    };
    
    auto res = http_client->Post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(res) << "POST /mcp should return a response";
    EXPECT_EQ(404, res->status) << "Should return 404 for invalid session";
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
    httplib::Headers headers = {
        {"Mcp-Session-Id", session_id}
    };
    
    auto delete_res = http_client->Delete("/mcp", headers);
    
    ASSERT_TRUE(delete_res) << "DELETE /mcp should succeed";
    EXPECT_EQ(204, delete_res->status) << "Should return 204 No Content on successful deletion";
    
    // Verify session is gone by trying to POST with a real method (not ping)
    json test_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"}
    };
    
    auto post_res = http_client->Post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(post_res) << "POST /mcp should return a response";
    EXPECT_EQ(404, post_res->status) << "Should return 404 after session deleted";
}

// Test: DELETE /mcp without session ID returns 400
TEST_F(StreamableHttpTransportTest, DeleteMcpWithoutSessionReturns400) {
    auto res = http_client->Delete("/mcp");
    
    ASSERT_TRUE(res) << "DELETE /mcp should return a response";
    EXPECT_EQ(400, res->status) << "Should return 400 for missing session ID";
}

// Test: DELETE /mcp with invalid session ID returns 404
TEST_F(StreamableHttpTransportTest, DeleteMcpWithInvalidSessionReturns404) {
    httplib::Headers headers = {
        {"Mcp-Session-Id", "invalid-session-12345"}
    };
    
    auto res = http_client->Delete("/mcp", headers);
    
    ASSERT_TRUE(res) << "DELETE /mcp should return a response";
    EXPECT_EQ(404, res->status) << "Should return 404 for non-existent session";
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
    auto res = http_client->Post(url_with_session.c_str(), ping_request.dump(), "application/json");
    
    ASSERT_TRUE(res) << "POST /mcp with query param should succeed";
    EXPECT_EQ(202, res->status) << "Should return 202 Accepted";
}

// Test: CORS headers include Mcp-Session-Id
TEST_F(StreamableHttpTransportTest, CorsHeadersIncludeMcpSessionId) {
    auto res = http_client->Options("/mcp");
    
    ASSERT_TRUE(res) << "OPTIONS /mcp should succeed";
    EXPECT_EQ(204, res->status) << "Should return 204 No Content";
    
    // Check CORS headers
    auto allow_methods = res->headers.find("Access-Control-Allow-Methods");
    ASSERT_NE(allow_methods, res->headers.end()) << "Should have Allow-Methods header";
    EXPECT_NE(allow_methods->second.find("GET"), std::string::npos) << "Should allow GET";
    EXPECT_NE(allow_methods->second.find("POST"), std::string::npos) << "Should allow POST";
    EXPECT_NE(allow_methods->second.find("DELETE"), std::string::npos) << "Should allow DELETE";
    
    auto allow_headers = res->headers.find("Access-Control-Allow-Headers");
    ASSERT_NE(allow_headers, res->headers.end()) << "Should have Allow-Headers header";
    EXPECT_NE(allow_headers->second.find("Mcp-Session-Id"), std::string::npos) 
        << "Should allow Mcp-Session-Id header";
}

// Test: Legacy /sse endpoint still works
TEST_F(StreamableHttpTransportTest, LegacySseEndpointWorks) {
    std::string endpoint;
    bool got_endpoint = false;
    std::mutex mtx;
    std::condition_variable cv;
    
    std::thread sse_thread([&]() {
        httplib::Client sse_client("localhost", 9092);
        sse_client.Get("/sse", [&](const char* data, size_t len) {
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
    });
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(3), [&] { return got_endpoint; });
    }
    
    if (sse_thread.joinable()) {
        sse_thread.detach();
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
    
    httplib::Headers headers = {
        {"Mcp-Session-Id", session_id},
        {"Accept", "text/html"}  // Unsupported media type
    };
    
    auto res = http_client->Post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(res) << "POST /mcp should return a response";
    EXPECT_EQ(406, res->status) << "Should return 406 Not Acceptable for unsupported Accept header";
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
    
    httplib::Headers headers = {
        {"Mcp-Session-Id", session_id},
        {"Accept", "application/json, text/event-stream"}  // Both supported types
    };
    
    auto res = http_client->Post("/mcp", headers, test_request.dump(), "application/json");
    
    ASSERT_TRUE(res) << "POST /mcp should succeed";
    EXPECT_EQ(202, res->status) << "Should return 202 Accepted with valid Accept header";
}

// Test: CORS headers include Accept in allowed headers
TEST_F(StreamableHttpTransportTest, CorsHeadersIncludeAccept) {
    auto res = http_client->Options("/mcp");
    
    ASSERT_TRUE(res) << "OPTIONS /mcp should succeed";
    EXPECT_EQ(204, res->status) << "Should return 204 No Content";
    
    auto allow_headers = res->headers.find("Access-Control-Allow-Headers");
    ASSERT_NE(allow_headers, res->headers.end()) << "Should have Allow-Headers header";
    EXPECT_NE(allow_headers->second.find("Accept"), std::string::npos) 
        << "Should allow Accept header";
}
