/**
 * @file catch2_tests.cpp
 * @brief Client/Server communication tests using Catch2
 * 
 * This file contains integration tests for MCP client/server communication
 * using the Catch2 testing framework.
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include "mcp_message.h"
#include "mcp_client.h"
#include "mcp_server.h"
#include "mcp_tool.h"
#include "mcp_stdio_client.h"

#include <thread>
#include <chrono>

using namespace mcp;

// Helper function to create a test server
class TestServer {
public:
    TestServer(int port = 8890) : port_(port) {
        config_.host = "localhost";
        config_.port = port_;
        config_.name = "Test Server";
        config_.version = "1.0.0";
        
        server_ = std::make_unique<server>(config_);
        
        // Register a simple echo tool
        tool echo_tool = tool_builder("echo")
            .with_description("Echoes the input message")
            .with_string_param("message", "Message to echo", "")
            .build();
            
        server_->register_tool(echo_tool, [](const json& params, const std::string&) -> json {
            std::string message = params.value("message", "");
            return json::array({{
                {"type", "text"},
                {"text", message}
            }});
        });
        
        // Register a calculator tool
        tool calc_tool = tool_builder("add")
            .with_description("Adds two numbers")
            .with_number_param("a", "First number", 0)
            .with_number_param("b", "Second number", 0)
            .build();
            
        server_->register_tool(calc_tool, [](const json& params, const std::string&) -> json {
            double a = params.value("a", 0.0);
            double b = params.value("b", 0.0);
            double result = a + b;
            return json::array({{
                {"type", "text"},
                {"text", std::to_string(result)}
            }});
        });
    }
    
    ~TestServer() {
        stop();
    }
    
    void start() {
        if (!server_) return;
        server_thread_ = std::make_unique<std::thread>([this]() {
            server_->start(true); // blocking mode in thread
        });
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void stop() {
        if (server_) {
            server_->stop();
        }
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
        }
    }
    
    server* get_server() { return server_.get(); }
    int get_port() const { return port_; }

private:
    int port_;
    server::configuration config_;
    std::unique_ptr<server> server_;
    std::unique_ptr<std::thread> server_thread_;
};

TEST_CASE("Server can start and stop", "[server][basic]") {
    SECTION("Server starts successfully") {
        TestServer test_server(8891);
        
        REQUIRE_NOTHROW(test_server.start());
        REQUIRE(test_server.get_server()->is_running());
        
        test_server.stop();
        REQUIRE_FALSE(test_server.get_server()->is_running());
    }
}

TEST_CASE("Message format validation", "[message][format]") {
    SECTION("Request message format is correct") {
        request req = request::create("test_method", {{"key", "value"}});
        json req_json = req.to_json();
        
        REQUIRE(req_json["jsonrpc"] == "2.0");
        REQUIRE(req_json.contains("id"));
        REQUIRE(req_json["method"] == "test_method");
        REQUIRE(req_json["params"]["key"] == "value");
    }
    
    SECTION("Response message format is correct") {
        response res = response::create_success("test_id", {{"result", "success"}});
        json res_json = res.to_json();
        
        REQUIRE(res_json["jsonrpc"] == "2.0");
        REQUIRE(res_json["id"] == "test_id");
        REQUIRE(res_json.contains("result"));
        REQUIRE(res_json["result"]["result"] == "success");
        REQUIRE_FALSE(res.is_error());
    }
    
    SECTION("Error response format is correct") {
        response err = response::create_error(
            "test_id", 
            error_code::invalid_params, 
            "Invalid parameters"
        );
        json err_json = err.to_json();
        
        REQUIRE(err_json["jsonrpc"] == "2.0");
        REQUIRE(err_json["id"] == "test_id");
        REQUIRE(err_json.contains("error"));
        REQUIRE(err_json["error"]["code"] == -32602);
        REQUIRE(err_json["error"]["message"] == "Invalid parameters");
        REQUIRE(err.is_error());
    }
}

TEST_CASE("Tool builder creates valid tools", "[tool][builder]") {
    SECTION("Tool with string parameter") {
        tool test_tool = tool_builder("test_tool")
            .with_description("A test tool")
            .with_string_param("param1", "First parameter", "default")
            .build();
        
        REQUIRE(test_tool.name == "test_tool");
        REQUIRE(test_tool.description == "A test tool");
        REQUIRE(test_tool.input_schema.contains("properties"));
        REQUIRE(test_tool.input_schema["properties"].contains("param1"));
    }
    
    SECTION("Tool with multiple parameters") {
        tool calc_tool = tool_builder("calculator")
            .with_description("Simple calculator")
            .with_number_param("x", "First number", 0)
            .with_number_param("y", "Second number", 0)
            .with_string_param("operation", "Operation (+, -, *, /)", "+")
            .build();
        
        REQUIRE(calc_tool.name == "calculator");
        REQUIRE(calc_tool.input_schema["properties"].contains("x"));
        REQUIRE(calc_tool.input_schema["properties"].contains("y"));
        REQUIRE(calc_tool.input_schema["properties"].contains("operation"));
    }
}

TEST_CASE("Server tool registration", "[server][tool]") {
    server::configuration config;
    config.host = "localhost";
    config.port = 8892;
    
    server test_server(config);
    
    SECTION("Can register a tool") {
        tool echo_tool = tool_builder("echo")
            .with_description("Echo tool")
            .with_string_param("text", "Text to echo", "")
            .build();
        
        bool handler_called = false;
        test_server.register_tool(echo_tool, [&handler_called](const json& params, const std::string&) -> json {
            handler_called = true;
            return json::array({{{"type", "text"}, {"text", "echoed"}}});
        });
        
        // Note: We can't easily test the handler execution without starting the server
        // and making an actual RPC call, but we can verify registration doesn't throw
        REQUIRE_NOTHROW(test_server.register_tool(echo_tool, [](const json&, const std::string&) -> json {
            return json::array();
        }));
    }
}

TEST_CASE("Error handling", "[error][exception]") {
    SECTION("MCP exception contains error code and message") {
        try {
            throw mcp_exception(error_code::method_not_found, "Method not found");
        } catch (const mcp_exception& e) {
            REQUIRE(e.code() == error_code::method_not_found);
            REQUIRE(std::string(e.what()) == "Method not found");
        }
    }
    
    SECTION("Different error codes") {
        auto test_error = [](error_code code, const std::string& msg) {
            try {
                throw mcp_exception(code, msg);
            } catch (const mcp_exception& e) {
                REQUIRE(e.code() == code);
                REQUIRE(std::string(e.what()) == msg);
            }
        };
        
        test_error(error_code::parse_error, "Parse error");
        test_error(error_code::invalid_request, "Invalid request");
        test_error(error_code::invalid_params, "Invalid params");
        test_error(error_code::internal_error, "Internal error");
    }
}

TEST_CASE("JSON-RPC protocol version", "[protocol][version]") {
    SECTION("Request has correct JSON-RPC version") {
        request req = request::create("test");
        REQUIRE(req.jsonrpc == "2.0");
    }
    
    SECTION("Response has correct JSON-RPC version") {
        response res = response::create_success("1", json::object());
        REQUIRE(res.jsonrpc == "2.0");
    }
}

TEST_CASE("Notification messages", "[message][notification]") {
    SECTION("Notification has no ID") {
        request notif = request::create_notification("test_event", {{"data", "value"}});
        
        REQUIRE(notif.is_notification());
        REQUIRE(notif.id.is_null());
        REQUIRE(notif.method.find("notifications/") == 0);
    }
    
    SECTION("Regular request has ID") {
        request req = request::create("test_method");
        
        REQUIRE_FALSE(req.is_notification());
        REQUIRE_FALSE(req.id.is_null());
    }
}
