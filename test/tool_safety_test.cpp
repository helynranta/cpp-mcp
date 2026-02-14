/**
 * @file tool_safety_test.cpp
 * @brief Tests for tool execution safety features (MCP 2025-03-26)
 * 
 * This file tests tool confirmation requirements, safety controls,
 * and trust model for tool annotations.
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "mcp_sse_client.h"
#include <thread>
#include <chrono>
#include <atomic>

class ToolSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Port for this test suite
        test_port = 9200;
    }

    void TearDown() override {
        if (server) {
            server->stop();
            server.reset();
        }
        if (client) {
            client.reset();
        }
        // Wait a bit for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    int test_port;
    std::unique_ptr<mcp::server> server;
    std::unique_ptr<mcp::sse_client> client;
};

// Test: Tool with confirmation requirement denied without handler
TEST_F(ToolSafetyTest, ConfirmationRequiredDeniedWithoutHandler) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.enable_tool_confirmation = true;
    
    server = std::make_unique<mcp::server>(config);
    
    // Register a tool that requires confirmation
    auto destructive_tool = mcp::tool_builder("delete_file")
        .with_description("Deletes a file")
        .with_string_param("path", "File path to delete", true)
        .with_destructive(true)
        .with_confirmation_required(true)
        .build();
    
    server->register_tool(destructive_tool, [](const mcp::json& params, const std::string&) -> mcp::json {
        return mcp::json::array({{{"type", "text"}, {"text", "File deleted"}}});
    });
    
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client and initialize
    client = std::make_unique<mcp::sse_client>("http://localhost:" + std::to_string(test_port));
    
    bool init_result = client->initialize("test-client", "1.0.0");
    ASSERT_TRUE(init_result) << "Initialize should succeed";
    
    // Try to call the tool that requires confirmation
    auto result = client->call_tool("delete_file", {{"path", "/tmp/test.txt"}});
    
    EXPECT_TRUE(result["isError"]) << "Tool call should fail without confirmation handler";
    if (result["isError"] && result.contains("content") && result["content"].is_array() && !result["content"].empty()) {
        auto first_item = result["content"][0];
        if (first_item.contains("text")) {
            std::string error_text = first_item["text"];
            EXPECT_NE(error_text.find("confirmation"), std::string::npos) 
                << "Error should mention confirmation";
        }
    }
}

// Test: Tool with confirmation requirement allowed with handler
TEST_F(ToolSafetyTest, ConfirmationRequiredAllowedWithHandler) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.enable_tool_confirmation = true;
    
    server = std::make_unique<mcp::server>(config);
    
    // Set confirmation handler that always allows
    server->set_tool_confirmation_handler([](const std::string& tool_name, 
                                             const mcp::json& arguments, 
                                             const std::string& session_id) -> bool {
        return true;  // Always confirm
    });
    
    // Register a tool that requires confirmation
    auto destructive_tool = mcp::tool_builder("delete_file")
        .with_description("Deletes a file")
        .with_string_param("path", "File path to delete", true)
        .with_destructive(true)
        .with_confirmation_required(true)
        .build();
    
    std::atomic<bool> tool_executed{false};
    server->register_tool(destructive_tool, [&tool_executed](const mcp::json& params, const std::string&) -> mcp::json {
        tool_executed = true;
        return mcp::json::array({{{"type", "text"}, {"text", "File deleted"}}});
    });
    
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client and initialize
    client = std::make_unique<mcp::sse_client>("http://localhost:" + std::to_string(test_port));
    
    bool init_result = client->initialize("test-client", "1.0.0");
    ASSERT_TRUE(init_result) << "Initialize should succeed";
    
    // Call the tool with confirmation handler set
    auto result = client->call_tool("delete_file", {{"path", "/tmp/test.txt"}});
    
    EXPECT_FALSE(result["isError"]) << "Tool call should succeed with confirmation handler";
    EXPECT_TRUE(tool_executed) << "Tool should have been executed";
}

// Test: Tool confirmation handler can deny execution
TEST_F(ToolSafetyTest, ConfirmationHandlerCanDeny) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.enable_tool_confirmation = true;
    
    server = std::make_unique<mcp::server>(config);
    
    // Set confirmation handler that denies specific paths
    server->set_tool_confirmation_handler([](const std::string& tool_name, 
                                             const mcp::json& arguments, 
                                             const std::string& session_id) -> bool {
        if (arguments.contains("path")) {
            std::string path = arguments["path"];
            // Deny deletion of system files
            if (path.find("/etc/") == 0 || path.find("/sys/") == 0) {
                return false;
            }
        }
        return true;
    });
    
    // Register a tool that requires confirmation
    auto destructive_tool = mcp::tool_builder("delete_file")
        .with_description("Deletes a file")
        .with_string_param("path", "File path to delete", true)
        .with_confirmation_required(true)
        .build();
    
    std::atomic<int> execution_count{0};
    server->register_tool(destructive_tool, [&execution_count](const mcp::json& params, const std::string&) -> mcp::json {
        execution_count++;
        return mcp::json::array({{{"type", "text"}, {"text", "File deleted"}}});
    });
    
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client and initialize
    client = std::make_unique<mcp::sse_client>("http://localhost:" + std::to_string(test_port));
    
    bool init_result = client->initialize("test-client", "1.0.0");
    ASSERT_TRUE(init_result) << "Initialize should succeed";
    
    // Try to delete a system file (should be denied)
    auto result1 = client->call_tool("delete_file", {{"path", "/etc/passwd"}});
    EXPECT_TRUE(result1["isError"]) << "Tool call should fail for system file";
    
    // Try to delete a user file (should be allowed)
    auto result2 = client->call_tool("delete_file", {{"path", "/tmp/test.txt"}});
    EXPECT_FALSE(result2["isError"]) << "Tool call should succeed for user file";
    
    EXPECT_EQ(1, execution_count.load()) << "Tool should execute only once (for allowed file)";
}

// Test: Tool without confirmation requirement executes freely
TEST_F(ToolSafetyTest, ToolWithoutConfirmationExecutesFreelyy) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.enable_tool_confirmation = true;
    
    server = std::make_unique<mcp::server>(config);
    
    // Set strict confirmation handler
    server->set_tool_confirmation_handler([](const std::string&, const mcp::json&, const std::string&) -> bool {
        return false;  // Deny all confirmations
    });
    
    // Register a tool that does NOT require confirmation
    auto safe_tool = mcp::tool_builder("read_file")
        .with_description("Reads a file")
        .with_string_param("path", "File path to read", true)
        .with_read_only(true)
        .build();  // No confirmation required
    
    std::atomic<bool> tool_executed{false};
    server->register_tool(safe_tool, [&tool_executed](const mcp::json& params, const std::string&) -> mcp::json {
        tool_executed = true;
        return mcp::json::array({{{"type", "text"}, {"text", "File content"}}});
    });
    
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client and initialize
    client = std::make_unique<mcp::sse_client>("http://localhost:" + std::to_string(test_port));
    
    bool init_result = client->initialize("test-client", "1.0.0");
    ASSERT_TRUE(init_result) << "Initialize should succeed";
    
    // Call the safe tool (should work despite strict handler)
    auto result = client->call_tool("read_file", {{"path", "/tmp/test.txt"}});
    
    EXPECT_FALSE(result["isError"]) << "Tool call should succeed for tool without confirmation";
    EXPECT_TRUE(tool_executed) << "Tool should have been executed";
}

// Test: Confirmation disabled allows all tools
TEST_F(ToolSafetyTest, ConfirmationDisabledAllowsAllTools) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.enable_tool_confirmation = false;  // Disabled
    
    server = std::make_unique<mcp::server>(config);
    
    // Register a tool that requires confirmation
    auto destructive_tool = mcp::tool_builder("delete_file")
        .with_description("Deletes a file")
        .with_string_param("path", "File path to delete", true)
        .with_confirmation_required(true)
        .build();
    
    std::atomic<bool> tool_executed{false};
    server->register_tool(destructive_tool, [&tool_executed](const mcp::json& params, const std::string&) -> mcp::json {
        tool_executed = true;
        return mcp::json::array({{{"type", "text"}, {"text", "File deleted"}}});
    });
    
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client and initialize
    client = std::make_unique<mcp::sse_client>("http://localhost:" + std::to_string(test_port));
    
    bool init_result = client->initialize("test-client", "1.0.0");
    ASSERT_TRUE(init_result) << "Initialize should succeed";
    
    // Call the tool (should work even without handler when confirmation is disabled)
    auto result = client->call_tool("delete_file", {{"path", "/tmp/test.txt"}});
    
    EXPECT_FALSE(result["isError"]) << "Tool call should succeed when confirmation is disabled";
    EXPECT_TRUE(tool_executed) << "Tool should have been executed";
}

// Test: Tool builder sets confirmation flag correctly
TEST_F(ToolSafetyTest, ToolBuilderSetsConfirmationFlag) {
    auto tool1 = mcp::tool_builder("tool1")
        .with_description("Tool without confirmation")
        .build();
    
    EXPECT_FALSE(tool1.requires_confirmation) << "Tool should not require confirmation by default";
    
    auto tool2 = mcp::tool_builder("tool2")
        .with_description("Tool with confirmation")
        .with_confirmation_required(true)
        .build();
    
    EXPECT_TRUE(tool2.requires_confirmation) << "Tool should require confirmation when set";
    
    auto tool3 = mcp::tool_builder("tool3")
        .with_description("Tool with confirmation disabled")
        .with_confirmation_required(false)
        .build();
    
    EXPECT_FALSE(tool3.requires_confirmation) << "Tool should not require confirmation when explicitly disabled";
}
