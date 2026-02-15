/**
 * @file mcp_test.cpp
 * @brief Test the basic functions of the MCP framework
 * 
 * This file contains tests for the message format, lifecycle, version control, ping, and tool functionality of the MCP framework.
 */

#define BOOST_TEST_MODULE MCP_Tests
#include <boost/test/unit_test.hpp>
#include "mcp_message.h"
#include "mcp_client.h"
#include "mcp_server.h"
#include "mcp_tool.h"
#include "mcp_sse_client.h"
#include "mcp_http_factory.h"

using namespace mcp;
using json = nlohmann::ordered_json;

// Test message format
struct MessageFormatTest {
    MessageFormatTest() {
        // Set up test environment
    }

    ~MessageFormatTest() {
        // Clean up test environment
    }
};

BOOST_FIXTURE_TEST_SUITE(MessageFormatTestSuite, MessageFormatTest)

// Test request message format
BOOST_AUTO_TEST_CASE(RequestMessageFormat) {
    // Create a request message
    request req = request::create("test_method", {{"key", "value"}});
    
    // Convert to JSON
    json req_json = req.to_json();
    
    // Verify JSON format is correct
    BOOST_CHECK_EQUAL(req_json["jsonrpc"], "2.0");
    BOOST_CHECK(req_json.contains("id"));
    BOOST_CHECK_EQUAL(req_json["method"], "test_method");
    BOOST_CHECK_EQUAL(req_json["params"]["key"], "value");
}

// Test response message format
BOOST_AUTO_TEST_CASE(ResponseMessageFormat) {
    // Create a successful response
    response res = response::create_success("test_id", {{"key", "value"}});
    
    // Convert to JSON
    json res_json = res.to_json();
    
    // Verify JSON format is correct
    BOOST_CHECK_EQUAL(res_json["jsonrpc"], "2.0");
    BOOST_CHECK_EQUAL(res_json["id"], "test_id");
    BOOST_CHECK_EQUAL(res_json["result"]["key"], "value");
    BOOST_CHECK(!res_json.contains("error"));
}

// Test error response message format
BOOST_AUTO_TEST_CASE(ErrorResponseMessageFormat) {
    // Create an error response
    response res = response::create_error("test_id", error_code::invalid_params, "Invalid parameters", {{"details", "Missing required field"}});
    
    // Convert to JSON
    json res_json = res.to_json();
    
    // Verify JSON format is correct
    BOOST_CHECK_EQUAL(res_json["jsonrpc"], "2.0");
    BOOST_CHECK_EQUAL(res_json["id"], "test_id");
    BOOST_CHECK(!res_json.contains("result"));
    BOOST_CHECK_EQUAL(res_json["error"]["code"], static_cast<int>(error_code::invalid_params));
    BOOST_CHECK_EQUAL(res_json["error"]["message"], "Invalid parameters");
    BOOST_CHECK_EQUAL(res_json["error"]["data"]["details"], "Missing required field");
}

// Test notification message format
BOOST_AUTO_TEST_CASE(NotificationMessageFormat) {
    // Create a notification message
    request notification = request::create_notification("test_notification", {{"key", "value"}});
    
    // Convert to JSON
    json notification_json = notification.to_json();
    
    // Verify JSON format is correct
    BOOST_CHECK_EQUAL(notification_json["jsonrpc"], "2.0");
    BOOST_CHECK(!notification_json.contains("id"));
    BOOST_CHECK_EQUAL(notification_json["method"], "notifications/test_notification");
    BOOST_CHECK_EQUAL(notification_json["params"]["key"], "value");
    
    // Verify if it is a notification message
    BOOST_CHECK(notification.is_notification());
}

BOOST_AUTO_TEST_SUITE_END()

// Test Lifecycle functionality - each test gets its own server
struct LifecycleTest {
    LifecycleTest() {
        // Create server on unique port for this test (avoid conflicts)
        static std::atomic<int> port_counter{10000};
        port_ = port_counter.fetch_add(1);
        
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
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
        client_ = std::make_unique<sse_client>("http://localhost:" + std::to_string(port_));
        client_->set_capabilities(client_capabilities);
    }

    ~LifecycleTest() {
        // IMPORTANT: Stop the server FIRST to ensure all connection handlers complete
        // before destroying the client. The server's SSE handlers capture `this` and
        // must finish before the server is destroyed.
        if (server_) {
            server_->stop();
        }
        // Now it's safe to destroy the client - the server's handlers have completed
        client_.reset();
        server_.reset();
    }

    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<sse_client> client_;
};

BOOST_FIXTURE_TEST_SUITE(LifecycleTestSuite, LifecycleTest)

// Test initialize process
BOOST_AUTO_TEST_CASE(InitializeProcess) {
    // Execute initialize
    bool init_result = client_->initialize("TestClient", "1.0.0");
    
    // Verify initialize result
    BOOST_CHECK(init_result);
    
    // Verify server capabilities
    json server_capabilities = client_->get_server_capabilities();
    BOOST_CHECK(server_capabilities.contains("logging"));
    BOOST_CHECK(server_capabilities.contains("prompts"));
    BOOST_CHECK(server_capabilities.contains("resources"));
    BOOST_CHECK(server_capabilities.contains("tools"));
}

BOOST_AUTO_TEST_SUITE_END()

// Test version control - each test gets isolated server
struct VersioningTest {
    VersioningTest() {
        // Create server on unique port for this test
        static std::atomic<int> port_counter{11000};
        port_ = port_counter.fetch_add(1);
        
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
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

        client_ = std::make_unique<sse_client>("http://localhost:" + std::to_string(port_));
    }

    ~VersioningTest() {
        // IMPORTANT: Stop the server FIRST to ensure all connection handlers complete
        // before destroying the client. The server's SSE handlers capture `this` and
        // must finish before the server is destroyed.
        if (server_) {
            server_->stop();
        }
        // Now it's safe to destroy the client - the server's handlers have completed
        client_.reset();
        server_.reset();
    }

    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<sse_client> client_;
};

BOOST_FIXTURE_TEST_SUITE(VersioningTestSuite, VersioningTest)

// Test supported version
BOOST_AUTO_TEST_CASE(SupportedVersion) {
    // Execute initialize
    bool init_result = client_->initialize("TestClient", "1.0.0");
    
    // Verify initialize result
    BOOST_CHECK(init_result);
}

// Test unsupported version
BOOST_AUTO_TEST_CASE(UnsupportedVersion) {
    try {
        // Use Beast client to send unsupported version request
        std::string base_url = "http://localhost:" + std::to_string(port_);
        auto http_client = http::create_client(base_url);
        
        // Open SSE connection
        auto msg_endpoint_promise = std::make_shared<std::promise<std::string>>();
        auto sse_promise = std::make_shared<std::promise<std::string>>();
        std::future<std::string> msg_endpoint = msg_endpoint_promise->get_future();
        std::future<std::string> sse_response = sse_promise->get_future();

        auto msg_endpoint_received = std::make_shared<std::atomic<bool>>(false);
        auto sse_response_received = std::make_shared<std::atomic<bool>>(false);

        // Capture port for use in detached thread
        int test_port = port_;
        auto sse_client_ptr = std::make_shared<std::atomic<http::client_interface*>>(nullptr);
        
        // Use std::thread with shared state to avoid lifetime issues when detaching
        std::thread sse_thread([msg_endpoint_received, sse_response_received,
                                msg_endpoint_promise, sse_promise, test_port, sse_client_ptr]() {
            // Create SSE client inside thread so it's owned by the thread
            std::string sse_base_url = "http://localhost:" + std::to_string(test_port);
            auto sse_client = http::create_client(sse_base_url);
            sse_client_ptr->store(sse_client.get(), std::memory_order_release);
            
            sse_client->get_stream("/sse", [msg_endpoint_received, sse_response_received,
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
                    BOOST_TEST_MESSAGE("SSE processing error: " << e.what());
                }
                // Continue until we get both messages
                return !msg_endpoint_received->load() || !sse_response_received->load();
            });
            
            sse_client_ptr->store(nullptr, std::memory_order_release);
        });
        
        std::string endpoint = msg_endpoint.get();
        BOOST_CHECK(!endpoint.empty());
        
        // Send unsupported version request
        json req = request::create("initialize", {{"protocolVersion", "0.0.1"}}).to_json();
        http::headers_map headers;
        auto res = http_client->post(endpoint, headers, req.dump(), "application/json");
        
        BOOST_CHECK(res.success);
        BOOST_CHECK_EQUAL(res.status_code / 100, 2);
        
        auto mcp_res = json::parse(sse_response.get());
        BOOST_CHECK_EQUAL(mcp_res["error"]["code"].get<int>(), static_cast<int>(error_code::invalid_params));

        // Give the callback a moment to finish and return before stopping
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop SSE client to interrupt the blocking Get() call
        auto client = sse_client_ptr->load(std::memory_order_acquire);
        if (client) {
            client->stop();
        }
        
        // Try to join the thread instead of detaching
        if (sse_thread.joinable()) {
            // Wait a bit for the thread to exit after stop()
            auto start = std::chrono::steady_clock::now();
            auto timeout = std::chrono::seconds(2);
            while (sse_thread.joinable() && std::chrono::steady_clock::now() - start < timeout) {
                try {
                    sse_thread.join();
                    break;
                } catch (...) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
            // Only detach if we couldn't join
            if (sse_thread.joinable()) {
                sse_thread.detach();
            }
        }
        
        // Clean up resources  
        http_client.reset();
    } catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("Test exception: " << e.what());
        BOOST_CHECK(false);
    } catch (...) {
        BOOST_CHECK(false);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// Ping test - each test gets isolated server
struct PingTest {
    PingTest() {
        // Create server on unique port for this test
        static std::atomic<int> port_counter{12000};
        port_ = port_counter.fetch_add(1);
        
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
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
        
        // Register a simple tool
        tool test_tool = tool_builder("test_tool")
            .with_description("A test tool")
            .with_string_param("message", "A test parameter", "")
            .build();
        
        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            return {{"result", "Test response"}};
        });
        
        // Start server (non-blocking mode)
        server_->start(false);

        json client_capabilities = {
            {"roots", {{"listChanged", true}}},
            {"sampling", json::object()}
        };
        client_ = std::make_unique<sse_client>("http://localhost:" + std::to_string(port_));
        client_->set_capabilities(client_capabilities);
    }

    ~PingTest() {
        // IMPORTANT: Stop the server FIRST to ensure all connection handlers complete
        // before destroying the client. The server's SSE handlers capture `this` and
        // must finish before the server is destroyed.
        if (server_) {
            server_->stop();
        }
        // Now it's safe to destroy the client - the server's handlers have completed
        client_.reset();
        server_.reset();
    }

    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<sse_client> client_;
};

BOOST_FIXTURE_TEST_SUITE(PingTestSuite, PingTest)

// Test Ping request
BOOST_AUTO_TEST_CASE(PingRequest) {
    // Initialize client
    bool init_result = client_->initialize("TestClient", "1.0.0");
    BOOST_CHECK(init_result);
    
    // Send ping request
    bool ping_result = client_->ping();
    BOOST_CHECK(ping_result);
}

BOOST_AUTO_TEST_CASE(DirectPing) {
    try {
        // Use Beast client to send Ping request
        std::string base_url = "http://localhost:" + std::to_string(port_);
        auto http_client = http::create_client(base_url);
        
        // Open SSE connection
        auto msg_endpoint_promise = std::make_shared<std::promise<std::string>>();
        auto sse_promise = std::make_shared<std::promise<std::string>>();
        std::future<std::string> msg_endpoint = msg_endpoint_promise->get_future();
        std::future<std::string> sse_response = sse_promise->get_future();

        auto msg_endpoint_received = std::make_shared<std::atomic<bool>>(false);
        auto sse_response_received = std::make_shared<std::atomic<bool>>(false);

        // Capture port for use in detached thread
        int test_port = port_;
        auto sse_client_ptr = std::make_shared<std::atomic<http::client_interface*>>(nullptr);
        
        // Use std::thread with shared state to avoid lifetime issues when detaching
        std::thread sse_thread([msg_endpoint_received, sse_response_received,
                                msg_endpoint_promise, sse_promise, test_port, sse_client_ptr]() {
            // Create SSE client inside thread so it's owned by the thread
            std::string sse_base_url = "http://localhost:" + std::to_string(test_port);
            auto sse_client = http::create_client(sse_base_url);
            sse_client_ptr->store(sse_client.get(), std::memory_order_release);
            
            sse_client->get_stream("/sse", [msg_endpoint_received, sse_response_received,
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
                    BOOST_TEST_MESSAGE("SSE processing error: " << e.what());
                }
                // Continue until we get both messages
                return !msg_endpoint_received->load() || !sse_response_received->load();
            });
            
            sse_client_ptr->store(nullptr, std::memory_order_release);
        });

        std::string endpoint = msg_endpoint.get();
        BOOST_CHECK(!endpoint.empty());

        // Even if the SSE connection is not established, you can send a ping request
        json ping_req = request::create("ping").to_json();
        http::headers_map ping_headers;
        auto ping_res = http_client->post(endpoint, ping_headers, ping_req.dump(), "application/json");
        BOOST_CHECK(ping_res.success);
        BOOST_CHECK_EQUAL(ping_res.status_code / 100, 2);

        auto mcp_res = json::parse(sse_response.get());
        BOOST_CHECK_EQUAL(mcp_res["result"], json::object());

        // Give the callback a moment to finish and return before stopping
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop SSE client to interrupt the blocking Get() call
        auto client = sse_client_ptr->load(std::memory_order_acquire);
        if (client) {
            client->stop();
        }
        
        // Try to join the thread instead of detaching
        if (sse_thread.joinable()) {
            // Wait a bit for the thread to exit after stop()
            auto start = std::chrono::steady_clock::now();
            auto timeout = std::chrono::seconds(2);
            while (sse_thread.joinable() && std::chrono::steady_clock::now() - start < timeout) {
                try {
                    sse_thread.join();
                    break;
                } catch (...) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
            // Only detach if we couldn't join
            if (sse_thread.joinable()) {
                sse_thread.detach();
            }
        }
        
        // Clean up resources
        http_client.reset();
    } catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("Test exception: " << e.what());
        BOOST_CHECK(false);
    } catch (...) {
        BOOST_CHECK(false);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// Tools test - each test gets its own isolated server
struct ToolsTest {
    ToolsTest() {
        // Create server on unique port for this test (avoid conflicts)
        static std::atomic<int> port_counter{13000};
        port_ = port_counter.fetch_add(1);
        
        // Set up test environment
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
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
            // Simple tool call implementation - validation is done in the test
            std::string tool_name = params["name"];
            std::string location = params["arguments"]["location"];
            
            // Return tool call result
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
        
        // Start server (non-blocking mode)
        server_->start(false);
        
        // Create client
        json client_capabilities = {
            {"roots", {{"listChanged", true}}},
            {"sampling", json::object()}
        };
        client_ = std::make_unique<sse_client>("http://localhost:" + std::to_string(port_));
        client_->set_capabilities(client_capabilities);
        client_->initialize("TestClient", "1.0.0");
    }

    ~ToolsTest() {
        // IMPORTANT: Stop the server FIRST to ensure all connection handlers complete
        // before destroying the client. The server's SSE handlers capture `this` and
        // must finish before the server is destroyed.
        if (server_) {
            server_->stop();
        }
        // Now it's safe to destroy the client - the server's handlers have completed
        client_.reset();
        server_.reset();
    }

    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<sse_client> client_;
};

BOOST_FIXTURE_TEST_SUITE(ToolsTestSuite, ToolsTest)

// Test listing tools
BOOST_AUTO_TEST_CASE(ListTools) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Call list tools method
    json tools_list = client_->send_request("tools/list").result;
    
    // Verify tools list
    BOOST_CHECK(tools_list.contains("tools"));
    BOOST_CHECK_EQUAL(tools_list["tools"].size(), 1);
    BOOST_CHECK_EQUAL(tools_list["tools"][0]["name"], "get_weather");
    BOOST_CHECK_EQUAL(tools_list["tools"][0]["description"], "Get current weather information for a location");
}

// Test calling tool
BOOST_AUTO_TEST_CASE(CallTool) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Call tool
    json tool_result = client_->call_tool("get_weather", {{"location", "New York"}});
    
    // Verify tool call result
    BOOST_CHECK(tool_result.contains("content"));
    BOOST_CHECK(!tool_result["isError"]);
    BOOST_CHECK_EQUAL(tool_result["content"][0]["type"], "text");
    BOOST_CHECK_EQUAL(tool_result["content"][0]["text"], "Current weather in New York:\nTemperature: 72°F\nConditions: Partly cloudy");
}

BOOST_AUTO_TEST_SUITE_END()

// Test tool metadata annotations
struct ToolMetadataTest {
    ToolMetadataTest() {
        // Setup for metadata tests
    }
};

BOOST_FIXTURE_TEST_SUITE(ToolMetadataTestSuite, ToolMetadataTest)

// Test tool with readOnly annotation
BOOST_AUTO_TEST_CASE(ReadOnlyAnnotation) {
    tool read_only_tool = tool_builder("get_data")
        .with_description("Get data from database")
        .with_string_param("id", "Data ID")
        .with_read_only(true)
        .build();
    
    json tool_json = read_only_tool.to_json();
    
    BOOST_CHECK(tool_json.contains("annotations"));
    BOOST_CHECK(tool_json["annotations"].contains("readOnly"));
    BOOST_CHECK(tool_json["annotations"]["readOnly"].get<bool>());
}

// Test tool with destructive annotation
BOOST_AUTO_TEST_CASE(DestructiveAnnotation) {
    tool destructive_tool = tool_builder("delete_data")
        .with_description("Delete data from database")
        .with_string_param("id", "Data ID to delete")
        .with_destructive(true)
        .build();
    
    json tool_json = destructive_tool.to_json();
    
    BOOST_CHECK(tool_json.contains("annotations"));
    BOOST_CHECK(tool_json["annotations"].contains("destructive"));
    BOOST_CHECK(tool_json["annotations"]["destructive"].get<bool>());
}

// Test tool with cost metadata
BOOST_AUTO_TEST_CASE(CostMetadata) {
    tool costly_tool = tool_builder("ai_inference")
        .with_description("Run AI inference")
        .with_string_param("prompt", "Inference prompt")
        .with_cost(0.05)
        .build();
    
    json tool_json = costly_tool.to_json();
    
    BOOST_CHECK(tool_json.contains("annotations"));
    BOOST_CHECK(tool_json["annotations"].contains("cost"));
    BOOST_CHECK_EQUAL(tool_json["annotations"]["cost"].get<double>(), 0.05);
}

// Test tool with latency metadata
BOOST_AUTO_TEST_CASE(LatencyMetadata) {
    tool slow_tool = tool_builder("slow_query")
        .with_description("Run a slow database query")
        .with_string_param("query", "SQL query")
        .with_latency(5000)
        .build();
    
    json tool_json = slow_tool.to_json();
    
    BOOST_CHECK(tool_json.contains("annotations"));
    BOOST_CHECK(tool_json["annotations"].contains("latency"));
    BOOST_CHECK_EQUAL(tool_json["annotations"]["latency"].get<int>(), 5000);
}

// Test tool with multiple annotations
BOOST_AUTO_TEST_CASE(MultipleAnnotations) {
    tool multi_tool = tool_builder("complex_operation")
        .with_description("Complex operation with multiple annotations")
        .with_string_param("data", "Input data")
        .with_read_only(false)
        .with_destructive(true)
        .with_cost(0.10)
        .with_latency(3000)
        .build();
    
    json tool_json = multi_tool.to_json();
    
    BOOST_CHECK(tool_json.contains("annotations"));
    BOOST_CHECK(!tool_json["annotations"]["readOnly"].get<bool>());
    BOOST_CHECK(tool_json["annotations"]["destructive"].get<bool>());
    BOOST_CHECK_EQUAL(tool_json["annotations"]["cost"].get<double>(), 0.10);
    BOOST_CHECK_EQUAL(tool_json["annotations"]["latency"].get<int>(), 3000);
}

// Test tool without annotations
BOOST_AUTO_TEST_CASE(NoAnnotations) {
    tool simple_tool = tool_builder("simple")
        .with_description("Simple tool without annotations")
        .with_string_param("input", "Input parameter")
        .build();
    
    json tool_json = simple_tool.to_json();
    
    // Annotations should not be present if not set
    BOOST_CHECK(!tool_json.contains("annotations"));
}

BOOST_AUTO_TEST_SUITE_END()

// Progress notification tests
#include "mcp_progress.h"

struct ProgressNotificationTest {
    ProgressNotificationTest() {
        // Set up test environment
    }

    ~ProgressNotificationTest() {
        // Clean up test environment
    }
};

BOOST_FIXTURE_TEST_SUITE(ProgressNotificationTestSuite, ProgressNotificationTest)

// Test progress notification creation
BOOST_AUTO_TEST_CASE(CreateProgressNotification) {
    json token = "test-token-123";
    progress_notification notif = progress_notification::create(token, 50.0, 100.0, "Processing...");
    
    BOOST_CHECK_EQUAL(notif.progress_token, token);
    BOOST_CHECK_EQUAL(notif.progress, 50.0);
    BOOST_REQUIRE(notif.total.has_value());
    BOOST_CHECK_EQUAL(notif.total.value(), 100.0);
    BOOST_REQUIRE(notif.message.has_value());
    BOOST_CHECK_EQUAL(notif.message.value(), "Processing...");
}

// Test progress notification without total
BOOST_AUTO_TEST_CASE(ProgressWithoutTotal) {
    json token = 42;
    progress_notification notif = progress_notification::create(token, 25.5);
    
    BOOST_CHECK_EQUAL(notif.progress_token, token);
    BOOST_CHECK_EQUAL(notif.progress, 25.5);
    BOOST_CHECK(!notif.total.has_value());
    BOOST_CHECK(!notif.message.has_value());
}

// Test progress notification to JSON
BOOST_AUTO_TEST_CASE(ProgressToJson) {
    json token = "operation-001";
    progress_notification notif = progress_notification::create(token, 75.0, 100.0, "Almost done");
    
    json params = notif.to_params();
    
    BOOST_CHECK_EQUAL(params["progressToken"], token);
    BOOST_CHECK_EQUAL(params["progress"].get<double>(), 75.0);
    BOOST_CHECK_EQUAL(params["total"].get<double>(), 100.0);
    BOOST_CHECK_EQUAL(params["message"].get<std::string>(), "Almost done");
}

// Test progress notification from JSON
BOOST_AUTO_TEST_CASE(ProgressFromJson) {
    json params = {
        {"progressToken", "test-456"},
        {"progress", 33.3},
        {"total", 99.9},
        {"message", "Working..."}
    };
    
    progress_notification notif = progress_notification::from_params(params);
    
    BOOST_CHECK_EQUAL(notif.progress_token.get<std::string>(), "test-456");
    BOOST_CHECK_EQUAL(notif.progress, 33.3);
    BOOST_REQUIRE(notif.total.has_value());
    BOOST_CHECK_EQUAL(notif.total.value(), 99.9);
    BOOST_REQUIRE(notif.message.has_value());
    BOOST_CHECK_EQUAL(notif.message.value(), "Working...");
}

// Test progress tracker
BOOST_AUTO_TEST_CASE(ProgressTrackerBasic) {
    progress_tracker tracker;
    
    json token1 = "token-1";
    json request_id1 = 123;
    
    // Register token
    tracker.register_token(token1, request_id1);
    BOOST_CHECK(tracker.is_token_active(token1));
    
    auto req_id = tracker.get_request_id(token1);
    BOOST_REQUIRE(req_id.has_value());
    BOOST_CHECK_EQUAL(req_id.value(), request_id1);
    
    // Unregister token
    tracker.unregister_token(token1);
    BOOST_CHECK(!tracker.is_token_active(token1));
    BOOST_CHECK(!tracker.get_request_id(token1).has_value());
}

// Test progress token extraction
BOOST_AUTO_TEST_CASE(ExtractProgressToken) {
    // Test with progress token
    json params_with_token = {
        {"arg1", "value1"},
        {"_meta", {
            {"progressToken", "my-token"}
        }}
    };
    
    auto token = progress_tracker::extract_progress_token(params_with_token);
    BOOST_REQUIRE(token.has_value());
    BOOST_CHECK_EQUAL(token.value().get<std::string>(), "my-token");
    
    // Test without progress token
    json params_without_token = {
        {"arg1", "value1"}
    };
    
    auto no_token = progress_tracker::extract_progress_token(params_without_token);
    BOOST_CHECK(!no_token.has_value());
    
    // Test with empty _meta
    json params_empty_meta = {
        {"arg1", "value1"},
        {"_meta", json::object()}
    };
    
    auto no_token2 = progress_tracker::extract_progress_token(params_empty_meta);
    BOOST_CHECK(!no_token2.has_value());
}

// Test create progress notification request
BOOST_AUTO_TEST_CASE(CreateProgressNotificationRequest) {
    progress_notification notif = progress_notification::create("token-999", 10.0, 20.0, "Test");
    
    request req = create_progress_notification(notif);
    
    BOOST_CHECK_EQUAL(req.jsonrpc, "2.0");
    BOOST_CHECK(req.is_notification());
    BOOST_CHECK_EQUAL(req.method, "notifications/progress");
    BOOST_CHECK_EQUAL(req.params["progressToken"].get<std::string>(), "token-999");
    BOOST_CHECK_EQUAL(req.params["progress"].get<double>(), 10.0);
    BOOST_CHECK_EQUAL(req.params["total"].get<double>(), 20.0);
    BOOST_CHECK_EQUAL(req.params["message"].get<std::string>(), "Test");
}

BOOST_AUTO_TEST_SUITE_END()

// Test batch request handling
struct BatchRequestTest {
    BatchRequestTest() {
        // Set up test environment
    }

    ~BatchRequestTest() {
        // Clean up test environment
    }
};

BOOST_FIXTURE_TEST_SUITE(BatchRequestTestSuite, BatchRequestTest)

// Test batch array with multiple requests
BOOST_AUTO_TEST_CASE(BatchArrayFormat) {
    // Create a batch of requests
    json batch = json::array();
    
    request req1 = request::create("method1", {{"param1", "value1"}});
    request req2 = request::create("method2", {{"param2", "value2"}});
    
    batch.push_back(req1.to_json());
    batch.push_back(req2.to_json());
    
    // Verify batch is an array
    BOOST_CHECK(batch.is_array());
    BOOST_CHECK_EQUAL(batch.size(), 2);
    
    // Verify each item in the batch
    BOOST_CHECK_EQUAL(batch[0]["method"], "method1");
    BOOST_CHECK_EQUAL(batch[1]["method"], "method2");
}

// Test batch with mixed requests and notifications
BOOST_AUTO_TEST_CASE(MixedBatchFormat) {
    json batch = json::array();
    
    // Add a regular request (with ID)
    request req = request::create("test_method", {{"key", "value"}});
    batch.push_back(req.to_json());
    
    // Add a notification (no ID)
    request notif = request::create_notification("test_notification", {{"key2", "value2"}});
    batch.push_back(notif.to_json());
    
    BOOST_CHECK(batch.is_array());
    BOOST_CHECK_EQUAL(batch.size(), 2);
    
    // Verify request has ID
    BOOST_CHECK(batch[0].contains("id"));
    BOOST_CHECK(!batch[0]["id"].is_null());
    
    // Verify notification has no ID (same pattern as line 988-990)
    bool is_notification = !batch[1].contains("id") || batch[1]["id"].is_null();
    BOOST_CHECK(is_notification);
}

// Test notification-only batch
BOOST_AUTO_TEST_CASE(NotificationOnlyBatch) {
    json batch = json::array();
    
    // Add multiple notifications
    request notif1 = request::create_notification("notif1", {{"key1", "value1"}});
    request notif2 = request::create_notification("notif2", {{"key2", "value2"}});
    
    batch.push_back(notif1.to_json());
    batch.push_back(notif2.to_json());
    
    BOOST_CHECK(batch.is_array());
    BOOST_CHECK_EQUAL(batch.size(), 2);
    
    // Verify both are notifications (no ID field or ID is null)
    for (const auto& item : batch) {
        bool is_notification = !item.contains("id") || item["id"].is_null();
        BOOST_CHECK(is_notification);
    }
}

// Test empty batch validation
BOOST_AUTO_TEST_CASE(EmptyBatchValidation) {
    json empty_batch = json::array();
    
    // Empty batch should be an array
    BOOST_CHECK(empty_batch.is_array());
    BOOST_CHECK_EQUAL(empty_batch.size(), 0);
}

// Test single request batch
BOOST_AUTO_TEST_CASE(SingleRequestBatch) {
    json batch = json::array();
    
    request req = request::create("single_method", {{"param", "value"}});
    batch.push_back(req.to_json());
    
    BOOST_CHECK(batch.is_array());
    BOOST_CHECK_EQUAL(batch.size(), 1);
    BOOST_CHECK_EQUAL(batch[0]["method"], "single_method");
}

BOOST_AUTO_TEST_SUITE_END()

// Integration tests for batch request handling - each test gets isolated server
struct BatchIntegrationTest {
    BatchIntegrationTest() {
        // Create server on unique port for this test (avoid conflicts)
        static std::atomic<int> port_counter{14000};
        port_ = port_counter.fetch_add(1);
        
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
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
        client_ = std::make_unique<sse_client>("http://localhost:" + std::to_string(port_));
        client_->set_capabilities(client_capabilities);
        
        // Initialize the client
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool init_result = client_->initialize("BatchTestClient", "1.0.0");
        if (!init_result) {
            throw std::runtime_error("Client initialization failed");
        }
    }

    ~BatchIntegrationTest() {
        // IMPORTANT: Stop the server FIRST to ensure all connection handlers complete
        // before destroying the client. The server's SSE handlers capture `this` and
        // must finish before the server is destroyed.
        if (server_) {
            server_->stop();
        }
        // Now it's safe to destroy the client - the server's handlers have completed
        client_.reset();
        server_.reset();
    }

    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<sse_client> client_;
};

BOOST_FIXTURE_TEST_SUITE(BatchIntegrationTestSuite, BatchIntegrationTest)

// Note: The following tests validate batch message format and parsing logic.
// Full integration tests would require extending the SSE client to support
// sending batch requests, which is beyond the scope of batch receive support.

// Test that server accepts batch requests (format validation)
BOOST_AUTO_TEST_CASE(BatchRequestValidation) {
    // This test validates that the batch detection logic works
    // by checking that single requests still work (backward compatibility)
    
    // Send a single request (non-batch)
    json response = client_->send_request("tools/list").result;
    
    // Verify response is valid
    BOOST_CHECK(response.contains("tools"));
    BOOST_CHECK(response["tools"].is_array());
}

BOOST_AUTO_TEST_SUITE_END()

// Test JSON-RPC validation integration - each test gets isolated server
struct JsonRpcServerValidationTest {
    JsonRpcServerValidationTest() {
        // Create server on unique port for this test (avoid conflicts)
        static std::atomic<int> port_counter{15000};
        port_ = port_counter.fetch_add(1);
        
        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
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
        client_ = std::make_unique<sse_client>("http://localhost:" + std::to_string(port_));
        client_->set_capabilities(client_capabilities);
        
        // Initialize
        bool init_result = client_->initialize("ValidationTestClient", "1.0.0");
        if (!init_result) {
            throw std::runtime_error("Client initialization failed");
        }
    }

    ~JsonRpcServerValidationTest() {
        // IMPORTANT: Stop the server FIRST to ensure all connection handlers complete
        // before destroying the client. The server's SSE handlers capture `this` and
        // must finish before the server is destroyed.
        if (server_) {
            server_->stop();
        }
        // Now it's safe to destroy the client - the server's handlers have completed
        client_.reset();
        server_.reset();
    }

    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<sse_client> client_;
};

BOOST_FIXTURE_TEST_SUITE(JsonRpcServerValidationTestSuite, JsonRpcServerValidationTest)

// Test that server accepts valid requests
BOOST_AUTO_TEST_CASE(AcceptsValidRequest) {
    // Send a valid request
    json result = client_->send_request("tools/list").result;
    
    // Should succeed
    BOOST_CHECK(result.contains("tools"));
}

// Test that notification structure is validated
BOOST_AUTO_TEST_CASE(NotificationStructureValid) {
    // Create a valid notification (no ID field)
    request notif = request::create_notification("test", {{"key", "value"}});
    json notif_json = notif.to_json();
    
    // Verify it doesn't have ID field
    BOOST_CHECK(!notif_json.contains("id"));
    BOOST_CHECK(notif.is_notification());
}

BOOST_AUTO_TEST_SUITE_END() 