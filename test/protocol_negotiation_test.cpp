/**
 * @file protocol_negotiation_test.cpp
 * @brief Tests for protocol version negotiation and lifecycle leniency
 *
 * Verifies:
 * 1. Server returns client-requested protocol version (not hardcoded latest)
 * 2. Server accepts tool calls in initializing state (without notifications/initialized)
 * 3. Both Streamable HTTP and legacy SSE transports handle lifecycle correctly
 */

#include "mcp_http_factory.h"
#include "mcp_message.h"
#include "mcp_server.h"
#include "mcp_sse_client.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

using namespace mcp;
using json = nlohmann::ordered_json;

// --------------------------------------------------------------------------
// Fixture: Streamable HTTP transport
// --------------------------------------------------------------------------
struct ProtocolNegotiationTest {
    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<http::client_interface> http_client;

    ProtocolNegotiationTest() {
        static std::atomic<int> port_counter{18500};
        port_ = port_counter.fetch_add(1);

        server::configuration config;
        config.host = "localhost";
        config.port = port_;
        config.name = "NegotiationTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);

        json caps = {{"tools", {{"listChanged", true}}}};
        server_->set_capabilities(caps);

        tool echo = tool_builder("echo")
                        .with_description("Echo tool")
                        .with_string_param("message", "Message", "")
                        .build();

        server_->register_tool(echo, [](const json& params, const std::string&) -> json {
            return {{"content", json::array({{{"type", "text"}, {"text", params.value("message", "")}}})}};
        });

        server_->start(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        http_client = http::create_client("http://localhost:" + std::to_string(port_));
        http_client->set_read_timeout(5);
    }

    ~ProtocolNegotiationTest() {
        if (server_) {
            server_->stop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

BOOST_FIXTURE_TEST_SUITE(ProtocolNegotiationTestSuite, ProtocolNegotiationTest)

/**
 * Server must echo the client-requested protocol version in the initialize response,
 * not always return MCP_VERSION (the latest version the server supports).
 */
BOOST_AUTO_TEST_CASE(InitializeReturnsRequestedProtocolVersion) {
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params", {{"protocolVersion", "2025-06-18"},
                                     {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    http::headers_map headers;
    auto res = http_client->post("/mcp", headers, init_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(200, res.status_code);

    json body = json::parse(res.body);
    // Must return the requested version, not the server's latest
    BOOST_CHECK_EQUAL(body["result"]["protocolVersion"], "2025-06-18");
}

/**
 * Server must also negotiate 2025-03-26 when requested.
 */
BOOST_AUTO_TEST_CASE(InitializeNegotiatesOldestSupportedVersion) {
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params", {{"protocolVersion", "2025-03-26"},
                                     {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    http::headers_map headers;
    auto res = http_client->post("/mcp", headers, init_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(200, res.status_code);

    json body = json::parse(res.body);
    BOOST_CHECK_EQUAL(body["result"]["protocolVersion"], "2025-03-26");
}

/**
 * Tool calls must succeed immediately after initialize, even if the client
 * has not yet sent notifications/initialized. This matches real-world MCP
 * client behavior (e.g., VS Code Copilot).
 */
BOOST_AUTO_TEST_CASE(ToolCallSucceedsWithoutInitializedNotification) {
    // Step 1: Initialize (stateless - no session ID)
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params", {{"protocolVersion", "2025-06-18"},
                                     {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    http::headers_map empty_headers;
    auto init_res = http_client->post("/mcp", empty_headers, init_request.dump(), "application/json");
    BOOST_REQUIRE(init_res.success);
    BOOST_REQUIRE_EQUAL(200, init_res.status_code);

    auto session_header = init_res.headers.find("Mcp-Session-Id");
    BOOST_REQUIRE(session_header != init_res.headers.end());
    std::string session_id = session_header->second;

    // Step 2: Skip notifications/initialized entirely

    // Step 3: Call tool immediately - must succeed
    json tool_call = {{"jsonrpc", "2.0"},
                      {"id", 2},
                      {"method", "tools/call"},
                      {"params", {{"name", "echo"}, {"arguments", {{"message", "hello"}}}}}};

    http::headers_map session_headers = {{"Mcp-Session-Id", session_id}};
    auto tool_res = http_client->post("/mcp", session_headers, tool_call.dump(), "application/json");

    BOOST_REQUIRE(tool_res.success);
    BOOST_CHECK_EQUAL(200, tool_res.status_code);

    json tool_body = json::parse(tool_res.body);
    // Must NOT be an error
    BOOST_CHECK(!tool_body.contains("error"));
    BOOST_CHECK(tool_body.contains("result"));
}

/**
 * Tool calls must also succeed after the full handshake
 * (initialize + notifications/initialized).
 */
BOOST_AUTO_TEST_CASE(ToolCallSucceedsWithFullHandshake) {
    // Step 1: Initialize
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params", {{"protocolVersion", "2025-06-18"},
                                     {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    http::headers_map empty_headers;
    auto init_res = http_client->post("/mcp", empty_headers, init_request.dump(), "application/json");
    BOOST_REQUIRE(init_res.success);

    auto session_header = init_res.headers.find("Mcp-Session-Id");
    BOOST_REQUIRE(session_header != init_res.headers.end());
    std::string session_id = session_header->second;

    // Step 2: Send notifications/initialized
    json notif = {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};
    http::headers_map session_headers = {{"Mcp-Session-Id", session_id}};
    auto notif_res = http_client->post("/mcp", session_headers, notif.dump(), "application/json");
    BOOST_CHECK_EQUAL(202, notif_res.status_code);

    // Step 3: Call tool - must succeed
    json tool_call = {{"jsonrpc", "2.0"},
                      {"id", 2},
                      {"method", "tools/call"},
                      {"params", {{"name", "echo"}, {"arguments", {{"message", "hello"}}}}}};

    auto tool_res = http_client->post("/mcp", session_headers, tool_call.dump(), "application/json");

    BOOST_REQUIRE(tool_res.success);
    BOOST_CHECK_EQUAL(200, tool_res.status_code);

    json tool_body = json::parse(tool_res.body);
    BOOST_CHECK(!tool_body.contains("error"));
    BOOST_CHECK(tool_body.contains("result"));
}

/**
 * Tool calls must succeed even when the client sends a stale (unknown) session ID.
 * This handles the case where a server restarts and the client has a cached session ID.
 * The server should create a new stateless session instead of returning 404.
 */
BOOST_AUTO_TEST_CASE(ToolCallSucceedsWithStaleSessionId) {
    // Step 1: Initialize to set up tools
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params", {{"protocolVersion", "2025-06-18"},
                                     {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    http::headers_map empty_headers;
    auto init_res = http_client->post("/mcp", empty_headers, init_request.dump(), "application/json");
    BOOST_REQUIRE(init_res.success);

    // Step 2: Use a completely fake/stale session ID (simulating server restart)
    std::string stale_session_id = "00000000-dead-beef-0000-000000000000";
    http::headers_map stale_headers = {{"Mcp-Session-Id", stale_session_id}};

    json tool_call = {{"jsonrpc", "2.0"},
                      {"id", 2},
                      {"method", "tools/call"},
                      {"params", {{"name", "echo"}, {"arguments", {{"message", "stale session test"}}}}}};

    auto tool_res = http_client->post("/mcp", stale_headers, tool_call.dump(), "application/json");

    BOOST_REQUIRE(tool_res.success);
    // Should succeed with 200, not 404
    BOOST_CHECK_EQUAL(200, tool_res.status_code);

    json tool_body = json::parse(tool_res.body);
    BOOST_CHECK(!tool_body.contains("error"));
    BOOST_CHECK(tool_body.contains("result"));

    // Verify the server returned a NEW session ID (not the stale one)
    auto new_session_header = tool_res.headers.find("Mcp-Session-Id");
    BOOST_REQUIRE(new_session_header != tool_res.headers.end());
    BOOST_CHECK(new_session_header->second != stale_session_id);
}

BOOST_AUTO_TEST_SUITE_END()

// --------------------------------------------------------------------------
// Fixture: Legacy SSE transport (used by sse_client)
// --------------------------------------------------------------------------
struct LegacySseLifecycleTest {
    int port_;
    std::unique_ptr<server> server_;

    LegacySseLifecycleTest() {
        static std::atomic<int> port_counter{18600};
        port_ = port_counter.fetch_add(1);

        server::configuration config;
        config.host = "localhost";
        config.port = port_;
        server_ = std::make_unique<server>(config);

        json caps = {{"tools", {{"listChanged", true}}}};
        server_->set_capabilities(caps);

        tool echo = tool_builder("echo")
                        .with_description("Echo tool")
                        .with_string_param("message", "Message", "")
                        .build();

        server_->register_tool(echo, [](const json& params, const std::string&) -> json {
            return {{"content", json::array({{{"type", "text"}, {"text", params.value("message", "")}}})}};
        });

        server_->start(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~LegacySseLifecycleTest() {
        if (server_) {
            server_->stop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

BOOST_FIXTURE_TEST_SUITE(LegacySseLifecycleTestSuite, LegacySseLifecycleTest)

/**
 * Legacy SSE client must be able to initialize and call tools without race conditions.
 * The notifications/initialized notification must be processed before tool calls.
 */
BOOST_AUTO_TEST_CASE(SseClientInitAndToolCallSucceeds) {
    sse_client client("http://localhost:" + std::to_string(port_));

    bool init_result = client.initialize("test-client", "1.0.0");
    BOOST_REQUIRE(init_result);

    auto result = client.call_tool("echo", {{"message", "hello from sse"}});
    BOOST_CHECK(!result.value("isError", false));

    if (result.contains("content") && result["content"].is_array() && !result["content"].empty()) {
        std::string text = result["content"][0].value("text", "");
        BOOST_CHECK_EQUAL(text, "hello from sse");
    }
}

BOOST_AUTO_TEST_SUITE_END()
