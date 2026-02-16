/**
 * @file streamable_http_client_test.cpp
 * @brief TDD tests for streamable_http_client
 *
 * This file contains tests for the Streamable HTTP client implementation
 * following MCP 2025-06-18 specification.
 */

#include "mcp_server.h"
#include "mcp_streamable_http_client.h"

#include <atomic>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

using namespace mcp;

/**
 * Test fixture for streamable_http_client
 *
 * Sets up a test server and tests that streamable_http_client can connect
 * and communicate using the Streamable HTTP transport.
 *
 * Each test gets a unique port to avoid conflicts and ensure isolation.
 */
struct StreamableHttpClientTest {
    StreamableHttpClientTest() {
        // Use unique port for each test to avoid conflicts
        static std::atomic<int> port_counter{20000};
        test_port_ = port_counter.fetch_add(1);

        // Create server
        server::configuration config;
        config.host = "localhost";
        config.port = test_port_;
        config.name = "StreamableHttpTestServer";
        config.version = "1.0.0";

        server_ = std::make_unique<server>(config);
        server_->set_capabilities({{"tools", json::object()}});

        // Register a simple echo tool
        tool echo_tool = tool_builder("echo")
                             .with_description("Echo test tool")
                             .with_string_param("message", "Message to echo", "")
                             .build();

        server_->register_tool(echo_tool, [](const json& params, const std::string&) -> json {
            return {{"echo", params.value("message", "")}};
        });

        // Start server
        server_->start(false);
    }

    ~StreamableHttpClientTest() {
        // Ensure proper cleanup to avoid shared state between tests
        if (server_) {
            server_->stop();
            server_.reset();
        }

        // Allow sufficient time for all threads to complete and resources to be released
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::string get_base_url() { return "http://localhost:" + std::to_string(test_port_); }

    int test_port_;
    std::unique_ptr<server> server_;
};

BOOST_FIXTURE_TEST_SUITE(StreamableHttpClientTestSuite, StreamableHttpClientTest)

/**
 * Test: streamable_http_client can initialize
 *
 * This test verifies that the Streamable HTTP client can connect
 * to the /mcp endpoint and establish a session.
 */
BOOST_AUTO_TEST_CASE(CanInitialize) {
    // Create client (uses /mcp endpoint by default)
    streamable_http_client client(get_base_url());

    // Initialize connection
    bool success = client.initialize("StreamableHttpTestClient", "1.0.0");

    BOOST_CHECK(success);
    BOOST_CHECK(client.is_running());
}

/**
 * Test: streamable_http_client can ping server
 */
BOOST_AUTO_TEST_CASE(CanPingServer) {
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    bool ping_result = client.ping();

    BOOST_CHECK(ping_result);
}

/**
 * Test: streamable_http_client can call tools
 */
BOOST_AUTO_TEST_CASE(CanCallTools) {
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // Call echo tool
    json result = client.call_tool("echo", {{"message", "Hello Streamable HTTP!"}});

    BOOST_CHECK(result.contains("content"));
    BOOST_CHECK(!result.value("isError", true));
    BOOST_CHECK(result["content"].contains("echo"));
    BOOST_CHECK_EQUAL(result["content"]["echo"], "Hello Streamable HTTP!");
}

/**
 * Test: streamable_http_client uses /mcp endpoint
 *
 * Verifies that the client uses the unified /mcp endpoint instead of
 * separate /sse and /message endpoints.
 */
BOOST_AUTO_TEST_CASE(UsesUnifiedMcpEndpoint) {
    // Create client with default /mcp endpoint
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // After initialization, client should be running
    BOOST_CHECK(client.is_running());

    // Multiple tool calls should work
    for (int i = 0; i < 3; i++) {
        json result = client.call_tool("echo", {{"message", "test"}});
        BOOST_CHECK(result.contains("content"));
        BOOST_CHECK(!result.value("isError", true));
    }
}

/**
 * Test: streamable_http_client can use custom endpoint
 */
BOOST_AUTO_TEST_CASE(CanUseCustomEndpoint) {
    // Create client with custom endpoint (still /mcp, just testing the parameter)
    streamable_http_client client(get_base_url(), "/mcp");

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    bool ping_result = client.ping();
    BOOST_CHECK(ping_result);
}

/**
 * Test: streamable_http_client timeout configuration
 */
BOOST_AUTO_TEST_CASE(TimeoutConfiguration) {
    streamable_http_client client(get_base_url());

    // Set timeout before initialization
    client.set_timeout(10);

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // Client should work normally with configured timeout
    bool ping_result = client.ping();
    BOOST_CHECK(ping_result);
}

/**
 * Test: streamable_http_client header management
 */
BOOST_AUTO_TEST_CASE(HeaderManagement) {
    streamable_http_client client(get_base_url());

    // Set custom header
    client.set_header("X-Custom-Header", "test-value");

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // Headers should be sent with requests
    bool ping_result = client.ping();
    BOOST_CHECK(ping_result);
}

/**
 * Test: streamable_http_client can get server capabilities
 */
BOOST_AUTO_TEST_CASE(CanGetServerCapabilities) {
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    json capabilities = client.get_server_capabilities();

    BOOST_CHECK(capabilities.is_object());
}

/**
 * Test: streamable_http_client can get tools list
 */
BOOST_AUTO_TEST_CASE(CanGetToolsList) {
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    std::vector<tool> tools = client.get_tools();

    BOOST_CHECK_GT(tools.size(), 0);

    bool found_echo = false;
    for (const auto& t : tools) {
        if (t.name == "echo") {
            found_echo = true;
            break;
        }
    }

    BOOST_CHECK(found_echo);
}

/**
 * Test: streamable_http_client cleanup works properly
 */
BOOST_AUTO_TEST_CASE(CleanupWorksCorrectly) {
    {
        streamable_http_client client(get_base_url());
        BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));
        BOOST_REQUIRE(client.ping());
        // Client destroyed here - should cleanup gracefully
    }

    // If we get here without hanging, cleanup worked
    BOOST_CHECK(true);
}

/**
 * Test: streamable_http_client can close session explicitly
 *
 * This is a unique feature of Streamable HTTP transport.
 */
BOOST_AUTO_TEST_CASE(CanCloseSessionExplicitly) {
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));
    BOOST_CHECK(client.is_running());

    // Explicitly close the session
    client.close_session();

    // After closing, client should not be running
    BOOST_CHECK(!client.is_running());
}

/**
 * Test: streamable_http_client multiple requests work
 */
BOOST_AUTO_TEST_CASE(MultipleRequestsWork) {
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // Send multiple requests
    for (int i = 0; i < 10; i++) {
        json result = client.call_tool("echo", {{"message", "request_" + std::to_string(i)}});
        BOOST_CHECK(result.contains("content"));
        BOOST_CHECK(!result.value("isError", true));
        BOOST_CHECK(result["content"].contains("echo"));
        BOOST_CHECK_EQUAL(result["content"]["echo"], "request_" + std::to_string(i));
    }
}

/**
 * Test: streamable_http_client notifications work
 */
BOOST_AUTO_TEST_CASE(NotificationsWork) {
    streamable_http_client client(get_base_url());

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // Send notification (should not throw)
    BOOST_CHECK_NO_THROW(client.send_notification("test/notification", {{"data", "test"}}));
}

/**
 * Test: streamable_http_client authentication token
 */
BOOST_AUTO_TEST_CASE(AuthenticationToken) {
    streamable_http_client client(get_base_url());

    // Set auth token before initialization
    client.set_auth_token("test-token-123");

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // Client should work with auth token set
    bool ping_result = client.ping();
    BOOST_CHECK(ping_result);
}

/**
 * Test: streamable_http_client capabilities management
 */
BOOST_AUTO_TEST_CASE(CapabilitiesManagement) {
    streamable_http_client client(get_base_url());

    // Set client capabilities
    json capabilities = {{"experimental", {{"feature1", true}}}};
    client.set_capabilities(capabilities);

    BOOST_REQUIRE(client.initialize("StreamableHttpTestClient", "1.0.0"));

    // Get capabilities back
    json returned_caps = client.get_capabilities();
    BOOST_CHECK(returned_caps.contains("experimental"));
}

BOOST_AUTO_TEST_SUITE_END()
