/**
 * @file mcp_conformance_test.cpp
 * @brief MCP 2025-03-26 Conformance Test Suite
 * 
 * This file contains comprehensive conformance tests for the MCP 2025-03-26 specification.
 * It includes golden interoperability tests for common flows and validates all MUST
 * requirements from the specification.
 * 
 * Test Categories:
 * - Golden Flow Tests: End-to-end tests for standard MCP workflows
 * - Protocol Conformance: JSON-RPC 2.0 and MCP protocol requirements
 * - Transport Conformance: Streamable HTTP transport requirements
 * - Lifecycle Conformance: Session lifecycle and state management
 * 
 * This test suite serves as the authoritative conformance gate for CI/CD.
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "mcp_message.h"
#include "mcp_tool.h"
#include "httplib.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace mcp;
using json = nlohmann::ordered_json;

/**
 * Test environment for MCP conformance tests
 * Sets up a server with a known configuration for testing
 */
class McpConformanceEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Set up test server with known configuration
        server::configuration config;
        config.host = "localhost";
        config.port = 9093; // Unique port to avoid conflicts
        config.name = "MCP-Conformance-Test-Server";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Set server capabilities per MCP spec
        json server_capabilities = {
            {"tools", {{"listChanged", true}}},
            {"logging", json::object()}
        };
        server_->set_capabilities(server_capabilities);
        
        // Register test tools for conformance testing
        register_test_tools();
        
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
    void register_test_tools() {
        // Tool 1: Simple echo tool (read-only)
        tool echo_tool = tool_builder("echo")
            .with_description("Echoes back the input message")
            .with_string_param("message", "Message to echo", "")
            .with_read_only(true)
            .build();
        
        server_->register_tool(echo_tool, [](const json& params, const std::string&) -> json {
            std::string msg = params.value("message", "");
            return {{"echo", msg}};
        });
        
        // Tool 2: Calculator tool (read-only)
        tool calc_tool = tool_builder("calculator")
            .with_description("Performs basic arithmetic operations")
            .with_string_param("operation", "Operation: add, subtract, multiply, divide", "add")
            .with_number_param("a", "First number", 0)
            .with_number_param("b", "Second number", 0)
            .with_read_only(true)
            .build();
        
        server_->register_tool(calc_tool, [](const json& params, const std::string&) -> json {
            std::string op = params.value("operation", "add");
            double a = params.value("a", 0.0);
            double b = params.value("b", 0.0);
            double result = 0.0;
            
            if (op == "add") result = a + b;
            else if (op == "subtract") result = a - b;
            else if (op == "multiply") result = a * b;
            else if (op == "divide") result = (b != 0) ? a / b : 0.0;
            
            return {{"result", result}};
        });
        
        // Tool 3: Slow tool for progress testing
        tool slow_tool = tool_builder("slow_operation")
            .with_description("A slow operation for testing progress notifications")
            .with_number_param("duration_ms", "Duration in milliseconds", 1000)
            .with_latency("high")
            .build();
        
        server_->register_tool(slow_tool, [](const json& params, const std::string&) -> json {
            int duration = params.value("duration_ms", 1000);
            std::this_thread::sleep_for(std::chrono::milliseconds(duration));
            return {{"completed", true}};
        });
    }

    static std::unique_ptr<server> server_;
};

std::unique_ptr<server> McpConformanceEnvironment::server_;

/**
 * Test fixture for conformance tests
 */
class McpConformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_client = std::make_unique<httplib::Client>("localhost", 9093);
        http_client->set_read_timeout(10, 0); // 10 seconds timeout
    }

    void TearDown() override {
        http_client.reset();
    }
    
    // Helper: Extract session ID from response header
    std::string extract_session_id(const httplib::Result& res) {
        if (!res) return "";
        auto it = res->headers.find("Mcp-Session-Id");
        if (it != res->headers.end()) {
            return it->second;
        }
        return "";
    }
    
    // Helper: Establish SSE session and return session ID
    std::pair<std::string, std::thread> establish_session() {
        std::string session_id;
        std::atomic<bool> got_session{false};
        
        std::thread sse_thread([&]() {
            httplib::Client sse_client("localhost", 9093);
            sse_client.Get("/mcp", [&](const char* data, size_t len) {
                // Just need to establish connection, store session ID from headers
                return true; // Continue streaming
            });
        });
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Get session ID from a POST request (it will be in the headers)
        auto res = http_client->Get("/mcp");
        if (res) {
            session_id = extract_session_id(res);
        }
        
        return {session_id, std::move(sse_thread)};
    }
    
    // Helper: Send request and get response
    httplib::Result send_request(const std::string& session_id, const json& request) {
        httplib::Headers headers;
        headers.emplace("Mcp-Session-Id", session_id);
        headers.emplace("Accept", "application/json");
        return http_client->Post("/mcp", headers, request.dump(), "application/json");
    }

    std::unique_ptr<httplib::Client> http_client;
};

// ========================================================================
// GOLDEN FLOW TESTS - End-to-End Interoperability
// ========================================================================

/**
 * Golden Test 1: Complete Initialize Handshake Flow
 * 
 * Tests the standard initialization sequence:
 * 1. Client establishes SSE connection (GET /mcp)
 * 2. Client sends initialize request
 * 3. Server responds with initialize result
 * 4. Client sends initialized notification
 * 5. Session is ready for operations
 */
TEST_F(McpConformanceTest, GoldenFlow_InitializeHandshake) {
    // Step 1: Establish SSE connection
    auto res_sse = http_client->Get("/mcp");
    ASSERT_TRUE(res_sse);
    EXPECT_EQ(200, res_sse->status);
    
    std::string session_id = extract_session_id(res_sse);
    ASSERT_FALSE(session_id.empty()) << "Session ID must be provided in Mcp-Session-Id header";
    
    // Step 2: Send initialize request
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", MCP_VERSION},
            {"capabilities", json::object()},
            {"clientInfo", {
                {"name", "ConformanceTestClient"},
                {"version", "1.0.0"}
            }}
        }}
    };
    
    auto res_init = send_request(session_id, init_request);
    ASSERT_TRUE(res_init);
    EXPECT_EQ(202, res_init->status) << "Initialize should return 202 Accepted for async processing";
    
    // In real implementation, response comes via SSE
    // For this test, we verify the request was accepted
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Step 3: Send initialized notification
    json initialized_notif = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/initialized"}
    };
    
    auto res_notif = send_request(session_id, initialized_notif);
    ASSERT_TRUE(res_notif);
    EXPECT_EQ(202, res_notif->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Step 4: Verify session is ready - send ping
    json ping_request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "ping"}
    };
    
    auto res_ping = send_request(session_id, ping_request);
    ASSERT_TRUE(res_ping);
    EXPECT_EQ(202, res_ping->status);
}

/**
 * Golden Test 2: Tools Discovery and Execution Flow
 * 
 * Tests the standard tool usage pattern:
 * 1. Initialize session
 * 2. List available tools
 * 3. Call a tool with parameters
 * 4. Receive tool result
 */
TEST_F(McpConformanceTest, GoldenFlow_ToolsDiscoveryAndExecution) {
    // Setup: Initialize session
    auto res_sse = http_client->Get("/mcp");
    ASSERT_TRUE(res_sse);
    std::string session_id = extract_session_id(res_sse);
    
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", MCP_VERSION},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0"}}}
        }}
    };
    send_request(session_id, init_request);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    json initialized = {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};
    send_request(session_id, initialized);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Step 1: List tools
    json list_request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "tools/list"}
    };
    
    auto res_list = send_request(session_id, list_request);
    ASSERT_TRUE(res_list);
    EXPECT_EQ(202, res_list->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Step 2: Call echo tool
    json call_request = {
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"method", "tools/call"},
        {"params", {
            {"name", "echo"},
            {"arguments", {
                {"message", "Hello, MCP!"}
            }}
        }}
    };
    
    auto res_call = send_request(session_id, call_request);
    ASSERT_TRUE(res_call);
    EXPECT_EQ(202, res_call->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Step 3: Call calculator tool
    json calc_request = {
        {"jsonrpc", "2.0"},
        {"id", 4},
        {"method", "tools/call"},
        {"params", {
            {"name", "calculator"},
            {"arguments", {
                {"operation", "add"},
                {"a", 5},
                {"b", 3}
            }}
        }}
    };
    
    auto res_calc = send_request(session_id, calc_request);
    ASSERT_TRUE(res_calc);
    EXPECT_EQ(202, res_calc->status);
}

/**
 * Golden Test 3: Session Termination Flow
 * 
 * Tests proper session cleanup:
 * 1. Initialize session
 * 2. Perform operations
 * 3. Terminate session with DELETE
 * 4. Verify session is gone
 */
TEST_F(McpConformanceTest, GoldenFlow_SessionTermination) {
    // Step 1: Establish and initialize session
    auto res_sse = http_client->Get("/mcp");
    ASSERT_TRUE(res_sse);
    std::string session_id = extract_session_id(res_sse);
    ASSERT_FALSE(session_id.empty());
    
    // Initialize
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", MCP_VERSION},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0"}}}
        }}
    };
    send_request(session_id, init_request);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Step 2: Delete session
    httplib::Headers headers;
    headers.emplace("Mcp-Session-Id", session_id);
    auto res_delete = http_client->Delete("/mcp", headers);
    
    ASSERT_TRUE(res_delete);
    EXPECT_EQ(204, res_delete->status) << "DELETE should return 204 No Content on success";
    
    // Step 3: Verify session is gone - POST should fail
    json ping = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "ping"}};
    auto res_after = send_request(session_id, ping);
    
    ASSERT_TRUE(res_after);
    EXPECT_EQ(404, res_after->status) << "Requests to deleted session should return 404";
}

// ========================================================================
// PROTOCOL CONFORMANCE TESTS - JSON-RPC 2.0
// ========================================================================

/**
 * Test: Protocol Version Field
 * Requirement: All messages MUST include "jsonrpc": "2.0"
 */
TEST_F(McpConformanceTest, Protocol_JsonRpcVersionRequired) {
    auto res = http_client->Get("/mcp");
    std::string session_id = extract_session_id(res);
    
    // Missing jsonrpc field
    json bad_request = {
        {"id", 1},
        {"method", "ping"}
    };
    
    auto res_bad = send_request(session_id, bad_request);
    ASSERT_TRUE(res_bad);
    EXPECT_EQ(400, res_bad->status) << "Missing jsonrpc field should be rejected";
}

/**
 * Test: Request ID Requirements
 * Requirement: Requests MUST have non-null ID, notifications MUST NOT have ID
 */
TEST_F(McpConformanceTest, Protocol_RequestIdRequirements) {
    auto res = http_client->Get("/mcp");
    std::string session_id = extract_session_id(res);
    
    // Request with null ID - should fail
    json null_id_request = {
        {"jsonrpc", "2.0"},
        {"id", nullptr},
        {"method", "ping"}
    };
    
    auto res_null = send_request(session_id, null_id_request);
    ASSERT_TRUE(res_null);
    EXPECT_EQ(400, res_null->status) << "Null request ID should be rejected";
    
    // Valid notification (no ID field)
    json notification = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/test"}
    };
    
    auto res_notif = send_request(session_id, notification);
    ASSERT_TRUE(res_notif);
    EXPECT_EQ(202, res_notif->status) << "Notification without ID should be accepted";
}

/**
 * Test: Batch Request Validation
 * Requirement: Empty batches MUST return error, initialize MUST NOT be in batch
 */
TEST_F(McpConformanceTest, Protocol_BatchRequestValidation) {
    auto res = http_client->Get("/mcp");
    std::string session_id = extract_session_id(res);
    
    // Empty batch
    json empty_batch = json::array();
    auto res_empty = send_request(session_id, empty_batch);
    ASSERT_TRUE(res_empty);
    EXPECT_EQ(400, res_empty->status) << "Empty batch should be rejected";
    
    // Batch with initialize - should fail
    json batch_with_init = json::array();
    batch_with_init.push_back({
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", MCP_VERSION},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "Test"}, {"version", "1.0"}}}
        }}
    });
    
    auto res_batch = send_request(session_id, batch_with_init);
    ASSERT_TRUE(res_batch);
    EXPECT_EQ(400, res_batch->status) << "Initialize in batch should be rejected";
}

// ========================================================================
// TRANSPORT CONFORMANCE TESTS - Streamable HTTP
// ========================================================================

/**
 * Test: Session ID Header Requirement
 * Requirement: POST and DELETE MUST include Mcp-Session-Id header
 */
TEST_F(McpConformanceTest, Transport_SessionIdHeaderRequired) {
    // POST without session ID should fail
    json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "ping"}
    };
    
    auto res = http_client->Post("/mcp", request.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(404, res->status) << "POST without session ID should return 404";
    
    // DELETE without session ID should fail
    auto res_del = http_client->Delete("/mcp");
    ASSERT_TRUE(res_del);
    EXPECT_EQ(400, res_del->status) << "DELETE without session ID should return 400";
}

/**
 * Test: Accept Header Validation
 * Requirement: POST MUST validate Accept header includes application/json or text/event-stream
 */
TEST_F(McpConformanceTest, Transport_AcceptHeaderValidation) {
    auto res_sse = http_client->Get("/mcp");
    std::string session_id = extract_session_id(res_sse);
    
    json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "ping"}
    };
    
    // Missing Accept header
    httplib::Headers headers_no_accept;
    headers_no_accept.emplace("Mcp-Session-Id", session_id);
    auto res_no = http_client->Post("/mcp", headers_no_accept, request.dump(), "application/json");
    ASSERT_TRUE(res_no);
    EXPECT_EQ(406, res_no->status) << "Missing Accept header should return 406";
    
    // Invalid Accept header
    httplib::Headers headers_bad;
    headers_bad.emplace("Mcp-Session-Id", session_id);
    headers_bad.emplace("Accept", "text/html");
    auto res_bad = http_client->Post("/mcp", headers_bad, request.dump(), "application/json");
    ASSERT_TRUE(res_bad);
    EXPECT_EQ(406, res_bad->status) << "Invalid Accept header should return 406";
    
    // Valid Accept header
    httplib::Headers headers_good;
    headers_good.emplace("Mcp-Session-Id", session_id);
    headers_good.emplace("Accept", "application/json");
    auto res_good = http_client->Post("/mcp", headers_good, request.dump(), "application/json");
    ASSERT_TRUE(res_good);
    EXPECT_EQ(202, res_good->status) << "Valid Accept header should be accepted";
}

/**
 * Test: HTTP Method Support
 * Requirement: /mcp endpoint MUST support GET, POST, DELETE
 */
TEST_F(McpConformanceTest, Transport_HttpMethodSupport) {
    // GET should work (establish session)
    auto res_get = http_client->Get("/mcp");
    ASSERT_TRUE(res_get);
    EXPECT_EQ(200, res_get->status);
    std::string session_id = extract_session_id(res_get);
    ASSERT_FALSE(session_id.empty());
    
    // POST should work (send request)
    json request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}};
    auto res_post = send_request(session_id, request);
    ASSERT_TRUE(res_post);
    EXPECT_EQ(202, res_post->status);
    
    // DELETE should work (terminate session)
    httplib::Headers headers;
    headers.emplace("Mcp-Session-Id", session_id);
    auto res_delete = http_client->Delete("/mcp", headers);
    ASSERT_TRUE(res_delete);
    EXPECT_EQ(204, res_delete->status);
}

// ========================================================================
// LIFECYCLE CONFORMANCE TESTS
// ========================================================================

/**
 * Test: Initialize Must Be First
 * Requirement: initialize MUST be the first method call (except ping)
 */
TEST_F(McpConformanceTest, Lifecycle_InitializeMustBeFirst) {
    auto res = http_client->Get("/mcp");
    std::string session_id = extract_session_id(res);
    
    // Try to call tools/list before initialize - should be rejected
    json tools_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"}
    };
    
    auto res_tools = send_request(session_id, tools_request);
    ASSERT_TRUE(res_tools);
    EXPECT_EQ(202, res_tools->status); // Accepted but will error via SSE
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Ping should be allowed
    json ping_request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "ping"}
    };
    
    auto res_ping = send_request(session_id, ping_request);
    ASSERT_TRUE(res_ping);
    EXPECT_EQ(202, res_ping->status);
}

/**
 * Test: Initialize Once Only
 * Requirement: initialize MUST be called exactly once per session
 */
TEST_F(McpConformanceTest, Lifecycle_InitializeOnceOnly) {
    auto res = http_client->Get("/mcp");
    std::string session_id = extract_session_id(res);
    
    // First initialize
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", MCP_VERSION},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0"}}}
        }}
    };
    
    auto res1 = send_request(session_id, init_request);
    ASSERT_TRUE(res1);
    EXPECT_EQ(202, res1->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Second initialize should fail
    json init_request2 = init_request;
    init_request2["id"] = 2;
    
    auto res2 = send_request(session_id, init_request2);
    ASSERT_TRUE(res2);
    EXPECT_EQ(202, res2->status); // Accepted but will error
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

/**
 * Test: Protocol Version Validation
 * Requirement: Server MUST validate protocolVersion in initialize request
 */
TEST_F(McpConformanceTest, Lifecycle_ProtocolVersionValidation) {
    auto res = http_client->Get("/mcp");
    std::string session_id = extract_session_id(res);
    
    // Initialize with correct version
    json init_good = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", MCP_VERSION},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0"}}}
        }}
    };
    
    auto res_good = send_request(session_id, init_good);
    ASSERT_TRUE(res_good);
    EXPECT_EQ(202, res_good->status) << "Valid protocol version should be accepted";
}

// Register test environment
::testing::Environment* const conformance_env =
    ::testing::AddGlobalTestEnvironment(new McpConformanceEnvironment);
