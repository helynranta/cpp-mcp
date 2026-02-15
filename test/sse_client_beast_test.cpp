/**
 * @file sse_client_beast_test.cpp
 * @brief TDD tests for sse_client using Beast adapter
 * 
 * This file contains tests written BEFORE migrating sse_client to use
 * the HTTP abstraction layer with Beast. Following strict TDD:
 * 1. Write failing test
 * 2. Implement minimal code to pass
 * 3. Refactor
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "mcp_sse_client.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace mcp;

/**
 * Test fixture for sse_client with Beast backend
 * 
 * Sets up a test server using Beast adapter (already migrated)
 * and tests that sse_client can connect and communicate.
 * 
 * Each test gets a unique port to avoid conflicts and ensure isolation.
 */
class SseClientBeastTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use unique port for each test to avoid conflicts
        // Note: This static counter is shared but necessary for port allocation
        static std::atomic<int> port_counter{19000};
        test_port_ = port_counter.fetch_add(1);
        
        // Create server (already using Beast via factory)
        server::configuration config;
        config.host = "localhost";
        config.port = test_port_;
        config.name = "BeastClientTestServer";
        config.version = "1.0.0";
        
        server_ = std::make_unique<server>(config);
        
        // Register a simple echo tool
        tool echo_tool = tool_builder("echo")
            .with_description("Echo test tool")
            .with_string_param("message", "Message to echo", "")
            .build();
        
        server_->register_tool(echo_tool, [](const json& params, const std::string&) -> json {
            return {{"echo", params.value("message", "")}};
        });
        
        // Start server - Beast server now properly signals when ready
        server_->start(false);
    }
    
    void TearDown() override {
        // Ensure proper cleanup to avoid shared state between tests
        if (server_) {
            server_->stop();
            server_.reset();
        }
        
        // Allow sufficient time for all threads to complete and resources to be released
        // This is critical to prevent race conditions and port conflicts when the next test starts
        // The delay accounts for:
        // - Server thread termination
        // - Connection handler threads completion
        // - Socket TIME_WAIT state
        // - OS-level resource cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    std::string get_base_url() {
        return "http://localhost:" + std::to_string(test_port_);
    }
    
    int test_port_;
    std::unique_ptr<server> server_;
};

/**
 * Test: sse_client can initialize with Beast backend
 * 
 * This test verifies that sse_client uses the
 * HTTP abstraction layer (which uses Beast by default).
 */
TEST_F(SseClientBeastTest, CanInitializeWithBeastBackend) {
    // Create client (should use Beast via factory after migration)
    sse_client client(get_base_url());
    
    // Initialize connection
    bool success = client.initialize("BeastTestClient", "1.0.0");
    
    EXPECT_TRUE(success) << "Client should initialize successfully with Beast backend";
    EXPECT_TRUE(client.is_running()) << "Client should be running after initialization";
}

/**
 * Test: sse_client can ping server using Beast
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, CanPingServer) {
    sse_client client(get_base_url());
    
    ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
    
    bool ping_result = client.ping();
    
    EXPECT_TRUE(ping_result) << "Ping should succeed with Beast backend";
}

/**
 * Test: sse_client can call tools using Beast
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, CanCallTools) {
    sse_client client(get_base_url());
    
    ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
    
    // Call echo tool
    json result = client.call_tool("echo", {{"message", "Hello Beast!"}});
    
    EXPECT_TRUE(result.contains("echo")) << "Result should contain echo field";
    EXPECT_EQ(result["echo"], "Hello Beast!") << "Echo should return correct message";
}

/**
 * Test: sse_client dual client pattern preserved
 * 
 * Verifies that the dual client pattern (one for POST, one for SSE GET)
 * is preserved after migration to abstraction layer.
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, DualClientPatternPreserved) {
    sse_client client(get_base_url());
    
    ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
    
    // After initialization, SSE should be running
    EXPECT_TRUE(client.is_running()) << "SSE connection should be active";
    
    // Multiple tool calls should work (testing POST client)
    for (int i = 0; i < 5; i++) {
        json result = client.call_tool("echo", {{"message", "test"}});
        EXPECT_TRUE(result.contains("echo"));
    }
    
    // SSE should still be running
    EXPECT_TRUE(client.is_running()) << "SSE connection should remain active during POST requests";
}

/**
 * Test: sse_client timeout configuration works with Beast
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, TimeoutConfiguration) {
    sse_client client(get_base_url());
    
    // Set timeout before initialization
    client.set_timeout(10);
    
    ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
    
    // Client should work normally with configured timeout
    bool ping_result = client.ping();
    EXPECT_TRUE(ping_result);
}

/**
 * Test: sse_client header management works with Beast
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, HeaderManagement) {
    sse_client client(get_base_url());
    
    // Set custom header
    client.set_header("X-Custom-Header", "test-value");
    
    ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
    
    // Headers should be sent with requests
    // (Server would need to echo headers for full verification,
    //  but this tests the API doesn't break)
    bool ping_result = client.ping();
    EXPECT_TRUE(ping_result);
}

/**
 * Test: sse_client can get server capabilities with Beast
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, CanGetServerCapabilities) {
    sse_client client(get_base_url());
    
    ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
    
    json capabilities = client.get_server_capabilities();
    
    EXPECT_TRUE(capabilities.is_object()) << "Server capabilities should be an object";
}

/**
 * Test: sse_client can get tools list with Beast
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, CanGetToolsList) {
    sse_client client(get_base_url());
    
    ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
    
    std::vector<tool> tools = client.get_tools();
    
    EXPECT_GT(tools.size(), 0) << "Should have at least the echo tool";
    
    bool found_echo = false;
    for (const auto& t : tools) {
        if (t.name == "echo") {
            found_echo = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_echo) << "Should find the echo tool in the list";
}

/**
 * Test: sse_client cleanup works properly with Beast
 * 
 * Tests that the client can be destroyed cleanly without hangs or crashes.
 * 
 * EXPECTED TO FAIL initially
 */
TEST_F(SseClientBeastTest, CleanupWorksCorrectly) {
    {
        sse_client client(get_base_url());
        ASSERT_TRUE(client.initialize("BeastTestClient", "1.0.0"));
        ASSERT_TRUE(client.ping());
        // Client destroyed here - should cleanup gracefully
    }
    
    // If we get here without hanging, cleanup worked
    SUCCEED();
}
