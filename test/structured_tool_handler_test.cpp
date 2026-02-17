/**
 * @file structured_tool_handler_test.cpp
 * @brief Tests for MCP 2025-06-18 structured tool handler result formats
 *
 * Tests that the server correctly handles tool handlers that return:
 * 1. New format: Full result object with content and structuredContent
 * 2. Legacy format: Just content array (backward compatibility)
 */

#include "mcp_server.h"
#include "mcp_sse_client.h"
#include "mcp_tool.h"

#include <boost/test/unit_test.hpp>
#include <memory>
#include <thread>

using namespace mcp;

BOOST_AUTO_TEST_SUITE(StructuredToolHandlerTestSuite)

// Test fixture for server/client setup
struct StructuredHandlerFixture {
    std::unique_ptr<server> srv;
    std::unique_ptr<sse_client> client;
    int test_port = 9910;

    StructuredHandlerFixture() {
        // Create and configure server
        server::configuration srv_conf;
        srv_conf.host = "localhost";
        srv_conf.port = test_port;
        srv = std::make_unique<server>(srv_conf);
        srv->set_server_info("Test Server", "1.0.0");
    }

    ~StructuredHandlerFixture() {
        if (client) {
            client.reset();
        }
        if (srv) {
            srv->stop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void start_server_and_client() {
        srv->start(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        client = std::make_unique<sse_client>("http://localhost:" + std::to_string(test_port));
        bool init = client->initialize("test-client", "1.0.0");
        BOOST_REQUIRE(init);
    }
};

// Test 1: Server handles new format - handler returns full result object
BOOST_FIXTURE_TEST_CASE(ServerHandlesNewFormatWithStructuredContent, StructuredHandlerFixture) {
    // Define tool with output schema
    json output_schema = {{"type", "object"},
                          {"properties", {{"value", {{"type", "number"}}}, {"unit", {{"type", "string"}}}}},
                          {"required", json::array({"value", "unit"})}};

    tool test_tool = tool_builder("new_format_tool")
                         .with_description("Tool using new format")
                         .with_title("New Format Tool")
                         .with_output_schema(output_schema)
                         .build();

    // Register tool with handler that returns full result object (new format)
    srv->register_tool(test_tool, [](const json& params, const std::string& session_id) -> json {
        json structured = {{"value", 42}, {"unit", "meters"}};
        return {{"content", json::array({{{"type", "text"}, {"text", "Result: 42 meters"}}})},
                {"structuredContent", structured},
                {"isError", false}};
    });

    start_server_and_client();

    // Call the tool
    json result = client->call_tool("new_format_tool", json::object());

    // Verify result structure
    BOOST_CHECK(result.contains("content"));
    BOOST_CHECK(result.contains("structuredContent"));
    BOOST_CHECK_EQUAL(result["isError"].get<bool>(), false);

    // Verify structured content is preserved
    BOOST_CHECK_EQUAL(result["structuredContent"]["value"].get<int>(), 42);
    BOOST_CHECK_EQUAL(result["structuredContent"]["unit"].get<std::string>(), "meters");

    // Verify content is preserved
    BOOST_CHECK(result["content"].is_array());
    BOOST_CHECK_EQUAL(result["content"][0]["type"].get<std::string>(), "text");
}

// Test 2: Server handles legacy format - handler returns just content array
BOOST_FIXTURE_TEST_CASE(ServerHandlesLegacyFormatContentOnly, StructuredHandlerFixture) {
    test_port = 9911; // Use different port

    // Create new server with new port
    server::configuration srv_conf;
    srv_conf.host = "localhost";
    srv_conf.port = test_port;
    srv = std::make_unique<server>(srv_conf);
    srv->set_server_info("Test Server", "1.0.0");

    // Define tool without output schema (legacy)
    tool legacy_tool = tool_builder("legacy_format_tool").with_description("Tool using legacy format").build();

    // Register tool with handler that returns just content array (legacy format)
    srv->register_tool(legacy_tool, [](const json& params, const std::string& session_id) -> json {
        return json::array({{{"type", "text"}, {"text", "Legacy result"}}});
    });

    start_server_and_client();

    // Call the tool
    json result = client->call_tool("legacy_format_tool", json::object());

    // Verify result structure
    BOOST_CHECK(result.contains("content"));
    BOOST_CHECK(!result.contains("structuredContent")); // No structured content in legacy format
    BOOST_CHECK_EQUAL(result["isError"].get<bool>(), false);

    // Verify content is correct
    BOOST_CHECK(result["content"].is_array());
    BOOST_CHECK_EQUAL(result["content"][0]["type"].get<std::string>(), "text");
    BOOST_CHECK_EQUAL(result["content"][0]["text"].get<std::string>(), "Legacy result");
}

// Test 3: Server handles new format without structuredContent (content only)
BOOST_FIXTURE_TEST_CASE(ServerHandlesNewFormatWithoutStructuredContent, StructuredHandlerFixture) {
    test_port = 9912; // Use different port

    server::configuration srv_conf;
    srv_conf.host = "localhost";
    srv_conf.port = test_port;
    srv = std::make_unique<server>(srv_conf);
    srv->set_server_info("Test Server", "1.0.0");

    // Define tool
    tool test_tool = tool_builder("hybrid_tool").with_description("Tool with hybrid format").build();

    // Register tool with handler that returns new format but no structuredContent
    srv->register_tool(test_tool, [](const json& params, const std::string& session_id) -> json {
        return {{"content", json::array({{{"type", "text"}, {"text", "Content without structured data"}}})},
                {"isError", false}};
    });

    start_server_and_client();

    // Call tool
    json result = client->call_tool("hybrid_tool", json::object());

    // Verify result
    BOOST_CHECK(result.contains("content"));
    BOOST_CHECK(!result.contains("structuredContent")); // No structured content provided
    BOOST_CHECK_EQUAL(result["isError"].get<bool>(), false);
}

// Test 4: Mixed handlers in same server
BOOST_FIXTURE_TEST_CASE(ServerHandlesMixedHandlerFormats, StructuredHandlerFixture) {
    test_port = 9913; // Use different port

    server::configuration srv_conf;
    srv_conf.host = "localhost";
    srv_conf.port = test_port;
    srv = std::make_unique<server>(srv_conf);
    srv->set_server_info("Test Server", "1.0.0");

    // Register tool with new format
    json output_schema = {{"type", "object"}, {"properties", {{"count", {{"type", "number"}}}}}};
    tool new_tool = tool_builder("new_tool").with_description("New format").with_output_schema(output_schema).build();

    srv->register_tool(new_tool, [](const json& params, const std::string&) -> json {
        return {{"content", json::array({{{"type", "text"}, {"text", "New: 5"}}})},
                {"structuredContent", {{"count", 5}}},
                {"isError", false}};
    });

    // Register tool with legacy format
    tool old_tool = tool_builder("old_tool").with_description("Legacy format").build();

    srv->register_tool(old_tool, [](const json& params, const std::string&) -> json {
        return json::array({{{"type", "text"}, {"text", "Old: 3"}}});
    });

    start_server_and_client();

    // Call new format tool
    json new_result = client->call_tool("new_tool", json::object());
    BOOST_CHECK(new_result.contains("structuredContent"));
    BOOST_CHECK_EQUAL(new_result["structuredContent"]["count"].get<int>(), 5);

    // Call legacy format tool
    json old_result = client->call_tool("old_tool", json::object());
    BOOST_CHECK(!old_result.contains("structuredContent"));
    BOOST_CHECK(old_result["content"][0]["text"].get<std::string>().find("Old: 3") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
