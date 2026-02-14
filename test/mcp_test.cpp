/**
 * @file mcp_test.cpp
 * @brief Test the basic functions of the MCP framework
 * 
 * This file contains tests for the message format, lifecycle, version control, ping, and tool functionality of the MCP framework.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mcp_message.h"
#include "mcp_client.h"
#include "mcp_server.h"
#include "mcp_tool.h"
#include "mcp_sse_client.h"

using namespace mcp;
using json = nlohmann::ordered_json;

// Test message format
class MessageFormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }

    void TearDown() override {
        // Clean up test environment
    }
};

// Test request message format
TEST_F(MessageFormatTest, RequestMessageFormat) {
    // Create a request message
    request req = request::create("test_method", {{"key", "value"}});
    
    // Convert to JSON
    json req_json = req.to_json();
    
    // Verify JSON format is correct
    EXPECT_EQ(req_json["jsonrpc"], "2.0");
    EXPECT_TRUE(req_json.contains("id"));
    EXPECT_EQ(req_json["method"], "test_method");
    EXPECT_EQ(req_json["params"]["key"], "value");
}

// Test response message format
TEST_F(MessageFormatTest, ResponseMessageFormat) {
    // Create a successful response
    response res = response::create_success("test_id", {{"key", "value"}});
    
    // Convert to JSON
    json res_json = res.to_json();
    
    // Verify JSON format is correct
    EXPECT_EQ(res_json["jsonrpc"], "2.0");
    EXPECT_EQ(res_json["id"], "test_id");
    EXPECT_EQ(res_json["result"]["key"], "value");
    EXPECT_FALSE(res_json.contains("error"));
}

// Test error response message format
TEST_F(MessageFormatTest, ErrorResponseMessageFormat) {
    // Create an error response
    response res = response::create_error("test_id", error_code::invalid_params, "Invalid parameters", {{"details", "Missing required field"}});
    
    // Convert to JSON
    json res_json = res.to_json();
    
    // Verify JSON format is correct
    EXPECT_EQ(res_json["jsonrpc"], "2.0");
    EXPECT_EQ(res_json["id"], "test_id");
    EXPECT_FALSE(res_json.contains("result"));
    EXPECT_EQ(res_json["error"]["code"], static_cast<int>(error_code::invalid_params));
    EXPECT_EQ(res_json["error"]["message"], "Invalid parameters");
    EXPECT_EQ(res_json["error"]["data"]["details"], "Missing required field");
}

// Test notification message format
TEST_F(MessageFormatTest, NotificationMessageFormat) {
    // Create a notification message
    request notification = request::create_notification("test_notification", {{"key", "value"}});
    
    // Convert to JSON
    json notification_json = notification.to_json();
    
    // Verify JSON format is correct
    EXPECT_EQ(notification_json["jsonrpc"], "2.0");
    EXPECT_FALSE(notification_json.contains("id"));
    EXPECT_EQ(notification_json["method"], "notifications/test_notification");
    EXPECT_EQ(notification_json["params"]["key"], "value");
    
    // Verify if it is a notification message
    EXPECT_TRUE(notification.is_notification());
}

class LifecycleTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Set up test environment
        server::configuration config;
        config.host = "localhost";
        config.port = 8080;
        config.name = "TestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Set server capabilities
        json server_capabilities = {
            {"logging", json::object()},
            {"prompts", {{"listChanged", true}}},
            {"resources", {{"subscribe", true}, {"listChanged", true}}},
            {"tools", {{"listChanged", true}}}
        };
        server_->set_capabilities(server_capabilities);
        
        // Start server (non-blocking mode)
        server_->start(false);
        
        // Create client
        json client_capabilities = {
            {"roots", {{"listChanged", true}}},
            {"sampling", json::object()}
        };
        client_ = std::make_unique<sse_client>("http://localhost:8080");
        client_->set_capabilities(client_capabilities);
    }

    static void TearDownTestSuite() {
        // Clean up test environment
        // Stop server FIRST to close all connections
        if (server_) {
            server_->stop();
        }
        
        // Wait for any background SSE activity to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Now safe to destroy client and server
        client_.reset();
        server_.reset();
    }

    void SetUp() override {
        // Get client pointer
        client_ptr_ = client_.get();
    }

    // Use raw pointer for test access
    sse_client* client_ptr_;
    static std::unique_ptr<server> server_;
    static std::unique_ptr<sse_client> client_;
};

// Static member variable definition
std::unique_ptr<server> LifecycleTest::server_;
std::unique_ptr<sse_client> LifecycleTest::client_;

// Test initialize process
TEST_F(LifecycleTest, InitializeProcess) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Execute initialize
    bool init_result = client_ptr_->initialize("TestClient", "1.0.0");
    
    // Verify initialize result
    EXPECT_TRUE(init_result);
    
    // Verify server capabilities
    json server_capabilities = client_ptr_->get_server_capabilities();
    EXPECT_TRUE(server_capabilities.contains("logging"));
    EXPECT_TRUE(server_capabilities.contains("prompts"));
    EXPECT_TRUE(server_capabilities.contains("resources"));
    EXPECT_TRUE(server_capabilities.contains("tools"));
}

// Version control test environment
// Test version control
class VersioningTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Set up test environment
        server::configuration config;
        config.host = "localhost";
        config.port = 8081;
        config.name = "TestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Set server capabilities
        json server_capabilities = {
            {"logging", json::object()},
            {"prompts", {{"listChanged", true}}},
            {"resources", {{"subscribe", true}, {"listChanged", true}}},
            {"tools", {{"listChanged", true}}}
        };
        server_->set_capabilities(server_capabilities);
        
        // Start server (non-blocking mode)
        server_->start(false);

        client_ = std::make_unique<sse_client>("http://localhost:8081");
    }

    static void TearDownTestSuite() {
        // Clean up test environment
        // Stop server FIRST to close all connections
        if (server_) {
            server_->stop();
        }
        
        // CRITICAL: Wait for all threads (detached AND sse_client) to fully complete
        // httplib cleanup can take 2-3 seconds, especially with multiple connections
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        
        // Now safe to destroy client and server
        client_.reset();
        server_.reset();
    }

    void SetUp() override {
        // Get client pointer
        client_ptr_ = client_.get();
    }

    // Use raw pointer for test access
    sse_client* client_ptr_;
    static std::unique_ptr<server> server_;
    static std::unique_ptr<sse_client> client_;
};

std::unique_ptr<server> VersioningTest::server_;
std::unique_ptr<sse_client> VersioningTest::client_;

// Test supported version
TEST_F(VersioningTest, SupportedVersion) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Execute initialize
    bool init_result = client_ptr_->initialize("TestClient", "1.0.0");
    
    // Verify initialize result
    EXPECT_TRUE(init_result);
}

// Test unsupported version
TEST_F(VersioningTest, UnsupportedVersion) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    try {
        // Use httplib::Client to send unsupported version request
        std::unique_ptr<httplib::Client> http_client = std::make_unique<httplib::Client>("localhost", 8081);
        
        // Open SSE connection
        auto msg_endpoint_promise = std::make_shared<std::promise<std::string>>();
        auto sse_promise = std::make_shared<std::promise<std::string>>();
        std::future<std::string> msg_endpoint = msg_endpoint_promise->get_future();
        std::future<std::string> sse_response = sse_promise->get_future();

        auto msg_endpoint_received = std::make_shared<std::atomic<bool>>(false);
        auto sse_response_received = std::make_shared<std::atomic<bool>>(false);

        // Use std::thread with shared state to avoid lifetime issues when detaching
        std::thread sse_thread([msg_endpoint_received, sse_response_received,
                                msg_endpoint_promise, sse_promise]() {
            // Create SSE client inside thread so it's owned by the thread
            httplib::Client sse_client("localhost", 8081);
            sse_client.Get("/sse", [msg_endpoint_received, sse_response_received,
                                    msg_endpoint_promise, sse_promise](const char* data, size_t len) {
                try {
                    std::string response(data, len);
                    size_t pos = response.find("data: ");
                    if (pos != std::string::npos) {
                        std::string data_content = response.substr(pos + 6);
                        data_content = data_content.substr(0, data_content.find("\r\n"));
                        
                        if (!msg_endpoint_received->load() && response.find("endpoint") != std::string::npos) {
                            msg_endpoint_received->store(true);
                            try {
                                msg_endpoint_promise->set_value(data_content);
                            } catch (...) {
                                // Ignore duplicate exception setting
                            }
                        } else if (!sse_response_received->load() && response.find("message") != std::string::npos) {
                            sse_response_received->store(true);
                            try {
                                sse_promise->set_value(data_content);
                            } catch (...) {
                                // Ignore duplicate exception setting
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    GTEST_LOG_(ERROR) << "SSE processing error: " << e.what();
                }
                // Continue until we get both messages
                return !msg_endpoint_received->load() || !sse_response_received->load();
            });
        });
        
        std::string endpoint = msg_endpoint.get();
        EXPECT_FALSE(endpoint.empty());
        
        // Send unsupported version request
        json req = request::create("initialize", {{"protocolVersion", "0.0.1"}}).to_json();
        auto res = http_client->Post(endpoint.c_str(), req.dump(), "application/json");
        
        EXPECT_TRUE(res != nullptr);
        EXPECT_EQ(res->status / 100, 2);
        
        auto mcp_res = json::parse(sse_response.get());
        EXPECT_EQ(mcp_res["error"]["code"].get<int>(), static_cast<int>(error_code::invalid_params));

        // Detach the thread - all state is heap-allocated via shared_ptr, so it's safe
        // The thread will stop automatically once both messages are received
        if (sse_thread.joinable()) {
            sse_thread.detach();
        }
        
        // Clean up resources  
        http_client.reset();
    } catch (const std::exception& e) {
        GTEST_LOG_(ERROR) << "Test exception: " << e.what();
        EXPECT_TRUE(false);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

// Ping test environment
// Test Ping functionality
class PingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Set up test environment
        server::configuration config;
        config.host = "localhost";
        config.port = 8082;
        config.name = "TestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Start server (non-blocking mode)
        server_->start(false);
        
        // Create client
        json client_capabilities = {
            {"roots", {{"listChanged", true}}},
            {"sampling", json::object()}
        };
        client_ = std::make_unique<sse_client>("http://localhost:8082");
        client_->set_capabilities(client_capabilities);
    }

    static void TearDownTestSuite() {
        // Clean up test environment
        // Stop server FIRST to close all connections
        if (server_) {
            server_->stop();
        }
        
        // CRITICAL: Wait for all threads (detached AND sse_client) to fully complete
        // httplib cleanup can take 2-3 seconds, especially with multiple connections
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        
        // Now safe to destroy client and server
        client_.reset();
        server_.reset();
    }

    void SetUp() override {
        // Get client pointer
        client_ptr_ = client_.get();
    }

    // Use raw pointer for test access
    sse_client* client_ptr_;
    static std::unique_ptr<server> server_;
    static std::unique_ptr<sse_client> client_;
};

// Static member variable definition
std::unique_ptr<server> PingTest::server_;
std::unique_ptr<sse_client> PingTest::client_;

// Test Ping request
TEST_F(PingTest, PingRequest) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client_ptr_->initialize("TestClient", "1.0.0");
    bool ping_result = client_ptr_->ping();
    EXPECT_TRUE(ping_result);
}

TEST_F(PingTest, DirectPing) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    try {
        // Use httplib::Client to send Ping request
        std::unique_ptr<httplib::Client> http_client = std::make_unique<httplib::Client>("localhost", 8082);
        
        // Open SSE connection
        auto msg_endpoint_promise = std::make_shared<std::promise<std::string>>();
        auto sse_promise = std::make_shared<std::promise<std::string>>();
        std::future<std::string> msg_endpoint = msg_endpoint_promise->get_future();
        std::future<std::string> sse_response = sse_promise->get_future();

        auto msg_endpoint_received = std::make_shared<std::atomic<bool>>(false);
        auto sse_response_received = std::make_shared<std::atomic<bool>>(false);

        // Use std::thread with shared state to avoid lifetime issues when detaching
        std::thread sse_thread([msg_endpoint_received, sse_response_received,
                                msg_endpoint_promise, sse_promise]() {
            // Create SSE client inside thread so it's owned by the thread
            httplib::Client sse_client("localhost", 8082);
            sse_client.Get("/sse", [msg_endpoint_received, sse_response_received,
                                    msg_endpoint_promise, sse_promise](const char* data, size_t len) {
                try {
                    std::string response(data, len);
                    size_t pos = response.find("data: ");
                    if (pos != std::string::npos) {
                        std::string data_content = response.substr(pos + 6);
                        data_content = data_content.substr(0, data_content.find("\r\n"));
                        
                        if (!msg_endpoint_received->load() && response.find("endpoint") != std::string::npos) {
                            msg_endpoint_received->store(true);
                            try {
                                msg_endpoint_promise->set_value(data_content);
                            } catch (...) {
                                // Ignore duplicate exception setting
                            }
                        } else if (!sse_response_received->load() && response.find("message") != std::string::npos) {
                            sse_response_received->store(true);
                            try {
                                sse_promise->set_value(data_content);
                            } catch (...) {
                                // Ignore duplicate exception setting
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    GTEST_LOG_(ERROR) << "SSE processing error: " << e.what();
                }
                // Continue until we get both messages
                return !msg_endpoint_received->load() || !sse_response_received->load();
            });
        });

        std::string endpoint = msg_endpoint.get();
        EXPECT_FALSE(endpoint.empty());

        // Even if the SSE connection is not established, you can send a ping request
        json ping_req = request::create("ping").to_json();
        auto ping_res = http_client->Post(endpoint.c_str(), ping_req.dump(), "application/json");
        EXPECT_TRUE(ping_res != nullptr);
        EXPECT_EQ(ping_res->status / 100, 2);

        auto mcp_res = json::parse(sse_response.get());
        EXPECT_EQ(mcp_res["result"], json::object());

        // Detach the thread - all state is heap-allocated via shared_ptr, so it's safe
        // The thread will stop automatically once both messages are received
        if (sse_thread.joinable()) {
            sse_thread.detach();
        }
        
        // Clean up resources
        http_client.reset();
    } catch (const std::exception& e) {
        GTEST_LOG_(ERROR) << "Test exception: " << e.what();
        EXPECT_TRUE(false);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

// Tools test environment
// Test tools functionality
class ToolsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Set up test environment
        server::configuration config;
        config.host = "localhost";
        config.port = 8083;
        config.name = "TestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Create a test tool
        tool test_tool;
        test_tool.name = "get_weather";
        test_tool.description = "Get current weather information for a location";
        test_tool.parameters_schema = {
            {"type", "object"},
            {"properties", {
                {"location", {
                    {"type", "string"},
                    {"description", "City name or zip code"}
                }}
            }},
            {"required", json::array({"location"})}
        };
        
        // Register tool
        server_->register_tool(test_tool, [](const json& params, const std::string& /* session_id */) -> json {
            // Simple tool implementation
            std::string location = params["location"];
            return {
                {"content", json::array({
                    {
                        {"type", "text"},
                        {"text", "Current weather in " + location + ":\nTemperature: 72°F\nConditions: Partly cloudy"}
                    }
                })},
                {"isError", false}
            };
        });
        
        // Register tools list method
        server_->register_method("tools/list", [](const json& params, const std::string& /* session_id */) -> json {
            return {
                {"tools", json::array({
                    {
                        {"name", "get_weather"},
                        {"description", "Get current weather information for a location"},
                        {"inputSchema", {
                            {"type", "object"},
                            {"properties", {
                                {"location", {
                                    {"type", "string"},
                                    {"description", "City name or zip code"}
                                }}
                            }},
                            {"required", json::array({"location"})}
                        }}
                    }
                })},
                {"nextCursor", nullptr}
            };
        });
        
        // Register tools call method
        server_->register_method("tools/call", [](const json& params, const std::string& /* session_id */) -> json {
            // Verify parameters
            EXPECT_EQ(params["name"], "get_weather");
            EXPECT_EQ(params["arguments"]["location"], "New York");
            
            // Return tool call result
            return {
                {"content", json::array({
                    {
                        {"type", "text"},
                        {"text", "Current weather in New York:\nTemperature: 72°F\nConditions: Partly cloudy"}
                    }
                })},
                {"isError", false}
            };
        });
        
        // Start server (non-blocking mode)
        server_->start(false);
        
        // Create client
        json client_capabilities = {
            {"roots", {{"listChanged", true}}},
            {"sampling", json::object()}
        };
        client_ = std::make_unique<sse_client>("http://localhost:8083");
        client_->set_capabilities(client_capabilities);
        client_->initialize("TestClient", "1.0.0");
    }

    static void TearDownTestSuite() {
        // Clean up test environment
        // Stop server FIRST to close all connections
        if (server_) {
            server_->stop();
        }
        
        // Wait for any background SSE activity to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Now safe to destroy client and server
        client_.reset();
        server_.reset();
    }

    void SetUp() override {
        // Get client pointer
        client_ptr_ = client_.get();
    }

    // Use raw pointer for test access
    sse_client* client_ptr_;
    static std::unique_ptr<server> server_;
    static std::unique_ptr<sse_client> client_;
};

// Static member variable definition
std::unique_ptr<server> ToolsTest::server_;
std::unique_ptr<sse_client> ToolsTest::client_;

// Test listing tools
TEST_F(ToolsTest, ListTools) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Call list tools method
    json tools_list = client_ptr_->send_request("tools/list").result;
    
    // Verify tools list
    EXPECT_TRUE(tools_list.contains("tools"));
    EXPECT_EQ(tools_list["tools"].size(), 1);
    EXPECT_EQ(tools_list["tools"][0]["name"], "get_weather");
    EXPECT_EQ(tools_list["tools"][0]["description"], "Get current weather information for a location");
}

// Test calling tool
TEST_F(ToolsTest, CallTool) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Call tool
    json tool_result = client_ptr_->call_tool("get_weather", {{"location", "New York"}});
    
    // Verify tool call result
    EXPECT_TRUE(tool_result.contains("content"));
    EXPECT_FALSE(tool_result["isError"]);
    EXPECT_EQ(tool_result["content"][0]["type"], "text");
    EXPECT_EQ(tool_result["content"][0]["text"], "Current weather in New York:\nTemperature: 72°F\nConditions: Partly cloudy");
}

// Test tool metadata annotations
class ToolMetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for metadata tests
    }
};

// Test tool with readOnly annotation
TEST_F(ToolMetadataTest, ReadOnlyAnnotation) {
    tool read_only_tool = tool_builder("get_data")
        .with_description("Get data from database")
        .with_string_param("id", "Data ID")
        .with_read_only(true)
        .build();
    
    json tool_json = read_only_tool.to_json();
    
    EXPECT_TRUE(tool_json.contains("annotations"));
    EXPECT_TRUE(tool_json["annotations"].contains("readOnly"));
    EXPECT_TRUE(tool_json["annotations"]["readOnly"].get<bool>());
}

// Test tool with destructive annotation
TEST_F(ToolMetadataTest, DestructiveAnnotation) {
    tool destructive_tool = tool_builder("delete_data")
        .with_description("Delete data from database")
        .with_string_param("id", "Data ID to delete")
        .with_destructive(true)
        .build();
    
    json tool_json = destructive_tool.to_json();
    
    EXPECT_TRUE(tool_json.contains("annotations"));
    EXPECT_TRUE(tool_json["annotations"].contains("destructive"));
    EXPECT_TRUE(tool_json["annotations"]["destructive"].get<bool>());
}

// Test tool with cost metadata
TEST_F(ToolMetadataTest, CostMetadata) {
    tool costly_tool = tool_builder("ai_inference")
        .with_description("Run AI inference")
        .with_string_param("prompt", "Inference prompt")
        .with_cost(0.05)
        .build();
    
    json tool_json = costly_tool.to_json();
    
    EXPECT_TRUE(tool_json.contains("annotations"));
    EXPECT_TRUE(tool_json["annotations"].contains("cost"));
    EXPECT_DOUBLE_EQ(tool_json["annotations"]["cost"].get<double>(), 0.05);
}

// Test tool with latency metadata
TEST_F(ToolMetadataTest, LatencyMetadata) {
    tool slow_tool = tool_builder("slow_query")
        .with_description("Run a slow database query")
        .with_string_param("query", "SQL query")
        .with_latency(5000)
        .build();
    
    json tool_json = slow_tool.to_json();
    
    EXPECT_TRUE(tool_json.contains("annotations"));
    EXPECT_TRUE(tool_json["annotations"].contains("latency"));
    EXPECT_EQ(tool_json["annotations"]["latency"].get<int>(), 5000);
}

// Test tool with multiple annotations
TEST_F(ToolMetadataTest, MultipleAnnotations) {
    tool multi_tool = tool_builder("complex_operation")
        .with_description("Complex operation with multiple annotations")
        .with_string_param("data", "Input data")
        .with_read_only(false)
        .with_destructive(true)
        .with_cost(0.10)
        .with_latency(3000)
        .build();
    
    json tool_json = multi_tool.to_json();
    
    EXPECT_TRUE(tool_json.contains("annotations"));
    EXPECT_FALSE(tool_json["annotations"]["readOnly"].get<bool>());
    EXPECT_TRUE(tool_json["annotations"]["destructive"].get<bool>());
    EXPECT_DOUBLE_EQ(tool_json["annotations"]["cost"].get<double>(), 0.10);
    EXPECT_EQ(tool_json["annotations"]["latency"].get<int>(), 3000);
}

// Test tool without annotations
TEST_F(ToolMetadataTest, NoAnnotations) {
    tool simple_tool = tool_builder("simple")
        .with_description("Simple tool without annotations")
        .with_string_param("input", "Input parameter")
        .build();
    
    json tool_json = simple_tool.to_json();
    
    // Annotations should not be present if not set
    EXPECT_FALSE(tool_json.contains("annotations"));
}

// Progress notification tests
#include "mcp_progress.h"

class ProgressNotificationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }

    void TearDown() override {
        // Clean up test environment
    }
};

// Test progress notification creation
TEST_F(ProgressNotificationTest, CreateProgressNotification) {
    json token = "test-token-123";
    progress_notification notif = progress_notification::create(token, 50.0, 100.0, "Processing...");
    
    EXPECT_EQ(notif.progress_token, token);
    EXPECT_DOUBLE_EQ(notif.progress, 50.0);
    ASSERT_TRUE(notif.total.has_value());
    EXPECT_DOUBLE_EQ(notif.total.value(), 100.0);
    ASSERT_TRUE(notif.message.has_value());
    EXPECT_EQ(notif.message.value(), "Processing...");
}

// Test progress notification without total
TEST_F(ProgressNotificationTest, ProgressWithoutTotal) {
    json token = 42;
    progress_notification notif = progress_notification::create(token, 25.5);
    
    EXPECT_EQ(notif.progress_token, token);
    EXPECT_DOUBLE_EQ(notif.progress, 25.5);
    EXPECT_FALSE(notif.total.has_value());
    EXPECT_FALSE(notif.message.has_value());
}

// Test progress notification to JSON
TEST_F(ProgressNotificationTest, ProgressToJson) {
    json token = "operation-001";
    progress_notification notif = progress_notification::create(token, 75.0, 100.0, "Almost done");
    
    json params = notif.to_params();
    
    EXPECT_EQ(params["progressToken"], token);
    EXPECT_DOUBLE_EQ(params["progress"].get<double>(), 75.0);
    EXPECT_DOUBLE_EQ(params["total"].get<double>(), 100.0);
    EXPECT_EQ(params["message"].get<std::string>(), "Almost done");
}

// Test progress notification from JSON
TEST_F(ProgressNotificationTest, ProgressFromJson) {
    json params = {
        {"progressToken", "test-456"},
        {"progress", 33.3},
        {"total", 99.9},
        {"message", "Working..."}
    };
    
    progress_notification notif = progress_notification::from_params(params);
    
    EXPECT_EQ(notif.progress_token.get<std::string>(), "test-456");
    EXPECT_DOUBLE_EQ(notif.progress, 33.3);
    ASSERT_TRUE(notif.total.has_value());
    EXPECT_DOUBLE_EQ(notif.total.value(), 99.9);
    ASSERT_TRUE(notif.message.has_value());
    EXPECT_EQ(notif.message.value(), "Working...");
}

// Test progress tracker
TEST_F(ProgressNotificationTest, ProgressTrackerBasic) {
    progress_tracker tracker;
    
    json token1 = "token-1";
    json request_id1 = 123;
    
    // Register token
    tracker.register_token(token1, request_id1);
    EXPECT_TRUE(tracker.is_token_active(token1));
    
    auto req_id = tracker.get_request_id(token1);
    ASSERT_TRUE(req_id.has_value());
    EXPECT_EQ(req_id.value(), request_id1);
    
    // Unregister token
    tracker.unregister_token(token1);
    EXPECT_FALSE(tracker.is_token_active(token1));
    EXPECT_FALSE(tracker.get_request_id(token1).has_value());
}

// Test progress token extraction
TEST_F(ProgressNotificationTest, ExtractProgressToken) {
    // Test with progress token
    json params_with_token = {
        {"arg1", "value1"},
        {"_meta", {
            {"progressToken", "my-token"}
        }}
    };
    
    auto token = progress_tracker::extract_progress_token(params_with_token);
    ASSERT_TRUE(token.has_value());
    EXPECT_EQ(token.value().get<std::string>(), "my-token");
    
    // Test without progress token
    json params_without_token = {
        {"arg1", "value1"}
    };
    
    auto no_token = progress_tracker::extract_progress_token(params_without_token);
    EXPECT_FALSE(no_token.has_value());
    
    // Test with empty _meta
    json params_empty_meta = {
        {"arg1", "value1"},
        {"_meta", json::object()}
    };
    
    auto no_token2 = progress_tracker::extract_progress_token(params_empty_meta);
    EXPECT_FALSE(no_token2.has_value());
}

// Test create progress notification request
TEST_F(ProgressNotificationTest, CreateProgressNotificationRequest) {
    progress_notification notif = progress_notification::create("token-999", 10.0, 20.0, "Test");
    
    request req = create_progress_notification(notif);
    
    EXPECT_EQ(req.jsonrpc, "2.0");
    EXPECT_TRUE(req.is_notification());
    EXPECT_EQ(req.method, "notifications/progress");
    EXPECT_EQ(req.params["progressToken"].get<std::string>(), "token-999");
    EXPECT_DOUBLE_EQ(req.params["progress"].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(req.params["total"].get<double>(), 20.0);
    EXPECT_EQ(req.params["message"].get<std::string>(), "Test");
}

// Test batch request handling
class BatchRequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }

    void TearDown() override {
        // Clean up test environment
    }
};

// Test batch array with multiple requests
TEST_F(BatchRequestTest, BatchArrayFormat) {
    // Create a batch of requests
    json batch = json::array();
    
    request req1 = request::create("method1", {{"param1", "value1"}});
    request req2 = request::create("method2", {{"param2", "value2"}});
    
    batch.push_back(req1.to_json());
    batch.push_back(req2.to_json());
    
    // Verify batch is an array
    EXPECT_TRUE(batch.is_array());
    EXPECT_EQ(batch.size(), 2);
    
    // Verify each item in the batch
    EXPECT_EQ(batch[0]["method"], "method1");
    EXPECT_EQ(batch[1]["method"], "method2");
}

// Test batch with mixed requests and notifications
TEST_F(BatchRequestTest, MixedBatchFormat) {
    json batch = json::array();
    
    // Add a regular request (with ID)
    request req = request::create("test_method", {{"key", "value"}});
    batch.push_back(req.to_json());
    
    // Add a notification (no ID)
    request notif = request::create_notification("test_notification", {{"key2", "value2"}});
    batch.push_back(notif.to_json());
    
    EXPECT_TRUE(batch.is_array());
    EXPECT_EQ(batch.size(), 2);
    
    // Verify request has ID
    EXPECT_TRUE(batch[0].contains("id"));
    EXPECT_FALSE(batch[0]["id"].is_null());
    
    // Verify notification has no ID (same pattern as line 988-990)
    bool is_notification = !batch[1].contains("id") || batch[1]["id"].is_null();
    EXPECT_TRUE(is_notification);
}

// Test notification-only batch
TEST_F(BatchRequestTest, NotificationOnlyBatch) {
    json batch = json::array();
    
    // Add multiple notifications
    request notif1 = request::create_notification("notif1", {{"key1", "value1"}});
    request notif2 = request::create_notification("notif2", {{"key2", "value2"}});
    
    batch.push_back(notif1.to_json());
    batch.push_back(notif2.to_json());
    
    EXPECT_TRUE(batch.is_array());
    EXPECT_EQ(batch.size(), 2);
    
    // Verify both are notifications (no ID field or ID is null)
    for (const auto& item : batch) {
        bool is_notification = !item.contains("id") || item["id"].is_null();
        EXPECT_TRUE(is_notification);
    }
}

// Test empty batch validation
TEST_F(BatchRequestTest, EmptyBatchValidation) {
    json empty_batch = json::array();
    
    // Empty batch should be an array
    EXPECT_TRUE(empty_batch.is_array());
    EXPECT_EQ(empty_batch.size(), 0);
}

// Test single request batch
TEST_F(BatchRequestTest, SingleRequestBatch) {
    json batch = json::array();
    
    request req = request::create("single_method", {{"param", "value"}});
    batch.push_back(req.to_json());
    
    EXPECT_TRUE(batch.is_array());
    EXPECT_EQ(batch.size(), 1);
    EXPECT_EQ(batch[0]["method"], "single_method");
}

// Integration tests for batch request handling with server
class BatchIntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = 8090;
        config.name = "BatchTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Register a test tool
        tool test_tool = tool_builder("test_tool")
            .with_description("A test tool for batch processing")
            .with_string_param("input", "Input parameter", "")
            .build();
        
        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            return {
                {"result", "processed: " + params["input"].get<std::string>()}
            };
        });
        
        // Set server capabilities
        json server_capabilities = {
            {"tools", {{"listChanged", true}}}
        };
        server_->set_capabilities(server_capabilities);
        
        // Start server
        server_->start(false);
        
        // Create client
        json client_capabilities = {
            {"roots", {{"listChanged", true}}},
            {"sampling", json::object()}
        };
        client_ = std::make_unique<sse_client>("http://localhost:8090");
        client_->set_capabilities(client_capabilities);
    }

    static void TearDownTestSuite() {
        // Stop server FIRST to close all connections
        if (server_) {
            server_->stop();
        }
        
        // Wait for any background SSE activity to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Now safe to destroy client and server
        client_.reset();
        server_.reset();
    }

    void SetUp() override {
        client_ptr_ = client_.get();
        
        // Initialize the client
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool init_result = client_ptr_->initialize("BatchTestClient", "1.0.0");
        ASSERT_TRUE(init_result) << "Client initialization failed";
    }

    sse_client* client_ptr_;
    static std::unique_ptr<server> server_;
    static std::unique_ptr<sse_client> client_;
};

std::unique_ptr<server> BatchIntegrationTest::server_;
std::unique_ptr<sse_client> BatchIntegrationTest::client_;

// Note: The following tests validate batch message format and parsing logic.
// Full integration tests would require extending the SSE client to support
// sending batch requests, which is beyond the scope of batch receive support.

// Test that server accepts batch requests (format validation)
TEST_F(BatchIntegrationTest, BatchRequestValidation) {
    // This test validates that the batch detection logic works
    // by checking that single requests still work (backward compatibility)
    
    // Send a single request (non-batch)
    json response = client_ptr_->send_request("tools/list").result;
    
    // Verify response is valid
    EXPECT_TRUE(response.contains("tools"));
    EXPECT_TRUE(response["tools"].is_array());
}

// Test JSON-RPC validation integration with server
class JsonRpcServerValidationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = 8095;
        config.name = "ValidationTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
        
        // Register a simple test tool
        tool test_tool = tool_builder("echo")
            .with_description("Echo tool for validation testing")
            .with_string_param("text", "Text to echo", "")
            .build();
        
        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            return {{"echo", params["text"].get<std::string>()}};
        });
        
        json server_capabilities = {
            {"tools", {{"listChanged", true}}}
        };
        server_->set_capabilities(server_capabilities);
        
        server_->start(false);
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Create client
        json client_capabilities = {
            {"roots", {{"listChanged", true}}},
            {"sampling", json::object()}
        };
        client_ = std::make_unique<sse_client>("http://localhost:8095");
        client_->set_capabilities(client_capabilities);
        
        // Initialize
        bool init_result = client_->initialize("ValidationTestClient", "1.0.0");
        if (!init_result) {
            throw std::runtime_error("Client initialization failed");
        }
    }

    static void TearDownTestSuite() {
        // Stop server FIRST to close all connections
        if (server_) {
            server_->stop();
        }
        
        // Wait for any background SSE activity to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Now safe to destroy client and server
        client_.reset();
        server_.reset();
    }

    static std::unique_ptr<server> server_;
    static std::unique_ptr<sse_client> client_;
};

// Static member definitions
std::unique_ptr<server> JsonRpcServerValidationTest::server_;
std::unique_ptr<sse_client> JsonRpcServerValidationTest::client_;

// Test that server accepts valid requests
TEST_F(JsonRpcServerValidationTest, AcceptsValidRequest) {
    // Send a valid request
    json result = client_->send_request("tools/list").result;
    
    // Should succeed
    EXPECT_TRUE(result.contains("tools"));
}

// Test that notification structure is validated
TEST_F(JsonRpcServerValidationTest, NotificationStructureValid) {
    // Create a valid notification (no ID field)
    request notif = request::create_notification("test", {{"key", "value"}});
    json notif_json = notif.to_json();
    
    // Verify it doesn't have ID field
    EXPECT_FALSE(notif_json.contains("id"));
    EXPECT_TRUE(notif.is_notification());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    return RUN_ALL_TESTS();
} 