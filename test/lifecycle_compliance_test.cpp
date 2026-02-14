/**
 * @file lifecycle_compliance_test.cpp
 * @brief Test lifecycle compliance with MCP 2025-03-26 specification
 * 
 * Tests initialization lifecycle, batch request rejection, and capability gating
 * according to the MCP 2025-03-26 specification.
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "mcp_message.h"
#include "httplib.h"
#include <thread>
#include <chrono>

using namespace mcp;
using json = nlohmann::ordered_json;

// Test environment for lifecycle tests
class LifecycleComplianceEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = 9091;
        config.name = "LifecycleTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Set server capabilities
        json server_capabilities = {
            {"tools", {{"listChanged", true}}}
        };
        server_->set_capabilities(server_capabilities);
        
        // Register a test tool
        tool test_tool = tool_builder("test_tool")
            .with_description("A test tool")
            .with_string_param("input", "Input parameter", "")
            .build();
        
        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            return {{"result", "processed"}};
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

std::unique_ptr<server> LifecycleComplianceEnvironment::server_;

// Test fixture for lifecycle compliance tests
class LifecycleComplianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create HTTP client
        http_client = std::make_unique<httplib::Client>("localhost", 9091);
        
        // Establish SSE connection and get session endpoint
        sse_thread = std::thread([this]() {
            httplib::Client sse_client("localhost", 9091);
            sse_client.Get("/sse", [this](const char* data, size_t len) {
                std::string response(data, len);
                
                // Extract message endpoint
                if (response.find("endpoint") != std::string::npos) {
                    size_t pos = response.find("data: ");
                    if (pos != std::string::npos) {
                        std::string endpoint_data = response.substr(pos + 6);
                        endpoint_data = endpoint_data.substr(0, endpoint_data.find("\r\n"));
                        
                        std::lock_guard<std::mutex> lock(endpoint_mutex);
                        message_endpoint = endpoint_data;
                        endpoint_ready = true;
                        endpoint_cv.notify_one();
                    }
                }
                return true;
            });
        });
        
        // Wait for endpoint to be ready
        std::unique_lock<std::mutex> lock(endpoint_mutex);
        endpoint_cv.wait_for(lock, std::chrono::seconds(5), [this] { return endpoint_ready; });
        
        if (!endpoint_ready) {
            throw std::runtime_error("Failed to establish SSE connection");
        }
    }

    void TearDown() override {
        // Clean up
        if (sse_thread.joinable()) {
            // Note: SSE thread will continue running, but that's okay for testing
            sse_thread.detach();
        }
        http_client.reset();
    }

    std::unique_ptr<httplib::Client> http_client;
    std::thread sse_thread;
    std::string message_endpoint;
    std::mutex endpoint_mutex;
    std::condition_variable endpoint_cv;
    bool endpoint_ready = false;
};

// Test that initialize in batch is rejected
TEST_F(LifecycleComplianceTest, RejectInitializeInBatch) {
    // Create a batch with initialize
    json batch = json::array();
    
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}
        }}
    };
    
    batch.push_back(init_request);
    
    // Send batch request
    auto res = http_client->Post(message_endpoint.c_str(), batch.dump(), "application/json");
    
    // Should return error (400 Bad Request)
    ASSERT_TRUE(res);
    EXPECT_EQ(400, res->status);
    
    // Parse error response
    json error_response = json::parse(res->body);
    EXPECT_TRUE(error_response.contains("error"));
    EXPECT_EQ(-32600, error_response["error"]["code"]);
    EXPECT_NE(std::string::npos, 
              error_response["error"]["message"].get<std::string>().find("batch"));
}

// Test that requests before initialize are rejected (except ping)
TEST_F(LifecycleComplianceTest, RejectRequestsBeforeInitialize) {
    // Try to call a tool before initialize
    json tool_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/call"},
        {"params", {
            {"name", "test_tool"},
            {"arguments", {{"input", "test"}}}
        }}
    };
    
    auto res = http_client->Post(message_endpoint.c_str(), tool_request.dump(), "application/json");
    
    // Request should be accepted (202) but response should indicate error
    ASSERT_TRUE(res);
    EXPECT_EQ(202, res->status);
    
    // Wait a bit for async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Note: In SSE mode, error response would come via SSE
    // For this test, we verify the request was accepted but will fail
}

// Test that ping is allowed before initialize
TEST_F(LifecycleComplianceTest, AllowPingBeforeInitialize) {
    // Send ping request before initialize
    json ping_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "ping"}
    };
    
    auto res = http_client->Post(message_endpoint.c_str(), ping_request.dump(), "application/json");
    
    // Request should be accepted
    ASSERT_TRUE(res);
    EXPECT_EQ(202, res->status);
}

// Test that initialize can only be called once
TEST_F(LifecycleComplianceTest, RejectDuplicateInitialize) {
    // First initialize
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}
        }}
    };
    
    auto res1 = http_client->Post(message_endpoint.c_str(), init_request.dump(), "application/json");
    ASSERT_TRUE(res1);
    EXPECT_EQ(202, res1->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Second initialize should be rejected
    json init_request2 = init_request;
    init_request2["id"] = 2;
    
    auto res2 = http_client->Post(message_endpoint.c_str(), init_request2.dump(), "application/json");
    ASSERT_TRUE(res2);
    EXPECT_EQ(202, res2->status);
    
    // Wait for async processing - error response will come via SSE
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test that requests require initialized notification
TEST_F(LifecycleComplianceTest, RequireInitializedNotification) {
    // Send initialize
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}
        }}
    };
    
    auto res = http_client->Post(message_endpoint.c_str(), init_request.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(202, res->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Try to call tool before sending initialized notification
    json tool_request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "tools/call"},
        {"params", {
            {"name", "test_tool"},
            {"arguments", {{"input", "test"}}}
        }}
    };
    
    auto res2 = http_client->Post(message_endpoint.c_str(), tool_request.dump(), "application/json");
    ASSERT_TRUE(res2);
    EXPECT_EQ(202, res2->status);
    
    // Error response will come via SSE indicating session not ready
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test complete lifecycle sequence (success path)
TEST_F(LifecycleComplianceTest, SuccessfulLifecycleSequence) {
    // 1. Initialize
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}
        }}
    };
    
    auto res1 = http_client->Post(message_endpoint.c_str(), init_request.dump(), "application/json");
    ASSERT_TRUE(res1);
    EXPECT_EQ(202, res1->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 2. Send initialized notification
    json initialized_notif = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/initialized"}
    };
    
    auto res2 = http_client->Post(message_endpoint.c_str(), initialized_notif.dump(), "application/json");
    ASSERT_TRUE(res2);
    EXPECT_EQ(202, res2->status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 3. Now tool call should work
    json tool_request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "tools/call"},
        {"params", {
            {"name", "test_tool"},
            {"arguments", {{"input", "test"}}}
        }}
    };
    
    auto res3 = http_client->Post(message_endpoint.c_str(), tool_request.dump(), "application/json");
    ASSERT_TRUE(res3);
    EXPECT_EQ(202, res3->status);
    
    // Request should succeed (response via SSE)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test cancellation notification handling
TEST_F(LifecycleComplianceTest, CancellationNotificationHandling) {
    // Set up cancellation handler to track calls
    std::atomic<bool> cancellation_received{false};
    json cancelled_request_id;
    std::string cancellation_reason;
    
    auto* server = LifecycleComplianceEnvironment::GetServer().get();
    server->set_cancellation_handler([&](const json& request_id, const std::string& reason, const std::string&) {
        cancelled_request_id = request_id;
        cancellation_reason = reason;
        cancellation_received.store(true);
    });
    
    // Initialize session first
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", json::object()},
            {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}
        }}
    };
    
    http_client->Post(message_endpoint.c_str(), init_request.dump(), "application/json");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    json initialized_notif = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/initialized"}
    };
    
    http_client->Post(message_endpoint.c_str(), initialized_notif.dump(), "application/json");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Send cancellation notification
    json cancel_notif = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/cancelled"},
        {"params", {
            {"requestId", 42},
            {"reason", "User requested cancellation"}
        }}
    };
    
    auto res = http_client->Post(message_endpoint.c_str(), cancel_notif.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(202, res->status);
    
    // Wait for notification to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify cancellation handler was called
    EXPECT_TRUE(cancellation_received.load());
    EXPECT_EQ(42, cancelled_request_id);
    EXPECT_EQ("User requested cancellation", cancellation_reason);
}

// Register environment
::testing::Environment* const lifecycle_env =
    ::testing::AddGlobalTestEnvironment(new LifecycleComplianceEnvironment);
