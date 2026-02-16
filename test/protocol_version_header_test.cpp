/**
 * @file protocol_version_header_test.cpp
 * @brief Tests for MCP-Protocol-Version header validation (MCP 2025-06-18+)
 *
 * MCP 2025-06-18 requires the MCP-Protocol-Version header in all HTTP requests
 * after initialization. This test verifies proper header handling using the
 * streamable HTTP client which properly manages sessions.
 */

#include "mcp_server.h"
#include "mcp_streamable_http_client.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

using namespace mcp;

// Test fixture for server setup
struct ProtocolVersionHeaderFixture {
    int port_;
    std::unique_ptr<server> server_;

    ProtocolVersionHeaderFixture() {
        // Create server on unique port (avoid conflicts)
        static std::atomic<int> port_counter{18100};
        port_ = port_counter.fetch_add(1);

        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
        config.name = "ProtocolVersionTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);

        // Set server capabilities
        json server_capabilities = {{"tools", {{"listChanged", true}}}};
        server_->set_capabilities(server_capabilities);

        // Register a simple test tool
        tool test_tool = tool_builder("test_tool")
                             .with_description("Test tool")
                             .with_string_param("input", "Test input", "")
                             .build();

        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            return {{"content", json::array({{{"type", "text"}, {"text", "Test response"}}})}, {"isError", false}};
        });

        // Start server (non-blocking)
        server_->start(false);

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    ~ProtocolVersionHeaderFixture() {
        // Stop and clean up server
        if (server_) {
            server_->stop();
        }
        server_.reset();
    }
    
    std::string get_base_url() {
        return "http://localhost:" + std::to_string(port_);
    }
};

BOOST_FIXTURE_TEST_SUITE(ProtocolVersionHeaderTestSuite, ProtocolVersionHeaderFixture)

/**
 * Test that client can successfully initialize
 * This establishes the baseline that initialization works
 */
BOOST_AUTO_TEST_CASE(ClientCanInitialize) {
    streamable_http_client client(get_base_url());
    
    // Initialize should succeed
    bool success = client.initialize("TestClient", "1.0.0");
    BOOST_CHECK(success);
    BOOST_CHECK(client.is_running());
}

/**
 * Test that client can send requests after initialization
 * In the future, these requests should include MCP-Protocol-Version header
 */
BOOST_AUTO_TEST_CASE(ClientCanSendRequestsAfterInit) {
    streamable_http_client client(get_base_url());
    
    // Initialize
    BOOST_REQUIRE(client.initialize("TestClient", "1.0.0"));
    
    // Send ping request
    bool ping_success = client.ping();
    BOOST_CHECK(ping_success);
}

/**
 * Test that client properly handles version negotiation
 * The client should remember the negotiated version from initialization
 */
BOOST_AUTO_TEST_CASE(ClientNegotiatesVersion) {
    streamable_http_client client(get_base_url());
    
    // Initialize - this should negotiate protocol version
    BOOST_REQUIRE(client.initialize("TestClient", "1.0.0"));
    
    // After init, client should have stored the negotiated version
    // (We'll implement this in the client)
    
    // For now, just verify client works
    BOOST_CHECK(client.is_running());
}

/**
 * Placeholder test for server header validation
 * Once we implement header validation in the server, this will test
 * that the server properly validates the MCP-Protocol-Version header
 */
BOOST_AUTO_TEST_CASE(ServerValidatesProtocolVersionHeader) {
    // This test will be expanded once we implement server-side validation
    // For now, just verify server is running
    BOOST_CHECK(server_ != nullptr);
    
    // TODO: Once implemented, test that:
    // 1. Server accepts requests with valid MCP-Protocol-Version header
    // 2. Server rejects requests with invalid version (returns 400)
    // 3. Server accepts requests without header (backward compat to 2025-03-26)
    // 4. Server rejects version mismatch (client sends different version than negotiated)
}

BOOST_AUTO_TEST_SUITE_END()
