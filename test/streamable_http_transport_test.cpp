/**
 * @file streamable_http_transport_test.cpp
 * @brief Test Streamable HTTP transport compliance with MCP 2025-06-18 specification
 *
 * Tests unified /mcp endpoint, Mcp-Session-Id header semantics, GET/POST/DELETE methods,
 * and session lifecycle according to the MCP 2025-06-18 specification.
 */

#include "mcp_http_factory.h"
#include "mcp_message.h"
#include "mcp_server.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <regex>
#include <thread>

using namespace mcp;
using json = nlohmann::ordered_json;

// Test fixture for Streamable HTTP transport tests - each test gets isolated server
struct StreamableHttpTransportTest {
    StreamableHttpTransportTest() {
        // Create server on unique port for this test (avoid conflicts)
        static std::atomic<int> port_counter{17000};
        port_ = port_counter.fetch_add(1);

        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
        config.name = "StreamableHttpTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);

        // Set server capabilities
        json server_capabilities = {{"tools", {{"listChanged", true}}}};
        server_->set_capabilities(server_capabilities);

        // Register a test tool
        tool test_tool = tool_builder("echo")
                             .with_description("Echo tool for testing")
                             .with_string_param("message", "Message to echo", "")
                             .build();

        server_->register_tool(test_tool, [](const json& params, const std::string&) -> json {
            std::string msg = params.value("message", "");
            return {{"echo", msg}};
        });

        // Start server
        server_->start(false);

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::string base_url = "http://localhost:" + std::to_string(port_);
        http_client = http::create_client(base_url);
        http_client->set_read_timeout(5); // 5 seconds timeout
    }

    ~StreamableHttpTransportTest() {
        http_client.reset();

        // Stop and clean up server
        if (server_) {
            server_->stop();
        }
        server_.reset();
    }

    int port_;
    std::unique_ptr<server> server_;
    // Helper to extract session ID from Mcp-Session-Id header
    std::string extract_session_id(const http::client_result& res) {
        if (!res)
            return "";

        auto it = res.headers.find("Mcp-Session-Id");
        if (it != res.headers.end()) {
            return it->second;
        }
        return "";
    }

    // Helper to establish SSE connection and get endpoint (simplified)
    std::string establish_sse_session_simple() {
        std::string endpoint;
        bool got_endpoint = false;
        std::mutex mtx;
        std::condition_variable cv;
        std::string sse_url = "http://localhost:" + std::to_string(port_);
        auto sse_client = http::create_client(sse_url);
        std::atomic<http::client_interface*> client_ptr{nullptr};
        auto* sse_client_ptr = sse_client.get(); // Capture raw pointer for thread safety

        std::thread sse_thread([&, sse_client_ptr]() {
            client_ptr.store(sse_client_ptr, std::memory_order_release);

            auto res = sse_client_ptr->get_stream("/mcp", [&](const char* data, size_t len) {
                std::string response(data, len);

                // Extract session endpoint
                if (response.find("endpoint") != std::string::npos) {
                    size_t pos = response.find("data: ");
                    if (pos != std::string::npos) {
                        std::string endpoint_data = response.substr(pos + 6);
                        endpoint_data = endpoint_data.substr(0, endpoint_data.find("\r\n"));

                        std::lock_guard<std::mutex> lock(mtx);
                        endpoint = endpoint_data;
                        got_endpoint = true;
                        cv.notify_one();
                        return false; // Stop reading
                    }
                }
                return true;
            });

            client_ptr.store(nullptr, std::memory_order_release);
        });

        // Wait for endpoint
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(3), [&] { return got_endpoint; });
        }

        // Join the thread to ensure it completes before sse_client is destroyed
        if (sse_thread.joinable()) {
            sse_thread.join();
        }

        return endpoint;
    }

    std::unique_ptr<http::client_interface> http_client;
};

BOOST_FIXTURE_TEST_SUITE(StreamableHttpTransportTest_Suite, StreamableHttpTransportTest)

// Test: GET /mcp establishes SSE connection and returns Mcp-Session-Id header
BOOST_AUTO_TEST_CASE(GetMcpEstablishesSession) {
    std::string endpoint = establish_sse_session_simple();

    BOOST_CHECK(!endpoint.empty());
    BOOST_CHECK_NE(endpoint.find("/mcp"), std::string::npos);
}

// Test: POST /mcp with Mcp-Session-Id header uses existing session
BOOST_AUTO_TEST_CASE(PostMcpWithSessionHeader) {
    // First, establish a session via GET
    std::string endpoint = establish_sse_session_simple();

    // Extract session_id from endpoint query parameter
    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }

    BOOST_REQUIRE(!session_id.empty());

    // Now send a POST request with Mcp-Session-Id header
    json ping_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}};

    http::headers_map headers = {{"Mcp-Session-Id", session_id}};

    auto res = http_client->post("/mcp", headers, ping_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(202, res.status_code);

    // Verify session ID is echoed back in response
    std::string response_session_id = extract_session_id(res);
    BOOST_CHECK_EQUAL(session_id, response_session_id);
}

// Test: POST /mcp without session is handled statelessly and returns 200 with direct body
BOOST_AUTO_TEST_CASE(PostMcpWithoutSessionStatelessSucceeds) {
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {{"protocolVersion", MCP_VERSION}, {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    // POST without session ID should be treated as stateless
    http::headers_map empty_headers;
    auto res = http_client->post("/mcp", empty_headers, init_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(200, res.status_code);

    // Response body should contain protocolVersion
    json body = json::parse(res.body);
    BOOST_CHECK_EQUAL(body["result"]["protocolVersion"], MCP_VERSION);

    // Stateless responses may include a session header for reuse
    auto session_header = res.headers.find("Mcp-Session-Id");
    BOOST_CHECK(session_header != res.headers.end());
}

// Test: Stateless tools/call request works without explicit initialize handshake
BOOST_AUTO_TEST_CASE(PostMcpWithoutSessionToolsCallSucceeds) {
    json call_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "tools/call"},
                         {"params", {{"name", "echo"}, {"arguments", {{"message", "hello"}}}}}};

    http::headers_map empty_headers;
    auto res = http_client->post("/mcp", empty_headers, call_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(200, res.status_code);

    json body = json::parse(res.body);
    BOOST_CHECK(!body.contains("error"));
    BOOST_CHECK(body.contains("result"));
    BOOST_CHECK(body["result"].is_object());
}

// Test: Stateless tools/list request works without explicit initialize handshake
BOOST_AUTO_TEST_CASE(PostMcpWithoutSessionToolsListSucceeds) {
    json tools_list_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};

    http::headers_map empty_headers;
    auto res = http_client->post("/mcp", empty_headers, tools_list_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(200, res.status_code);

    json body = json::parse(res.body);
    BOOST_CHECK(body.contains("result"));
    BOOST_CHECK(body["result"].contains("tools"));
    BOOST_CHECK(body["result"]["tools"].is_array());
}

// Test: initialize response returns negotiated protocol version (not hardcoded latest)
BOOST_AUTO_TEST_CASE(PostMcpInitializeNegotiatesRequestedProtocolVersion) {
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {{"protocolVersion", "2025-06-18"}, {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    http::headers_map empty_headers;
    auto res = http_client->post("/mcp", empty_headers, init_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(200, res.status_code);

    json body = json::parse(res.body);
    BOOST_CHECK_EQUAL(body["result"]["protocolVersion"], "2025-06-18");
}

// Test: POST /mcp with invalid session ID creates recovery session (stale session resilience)
BOOST_AUTO_TEST_CASE(PostMcpWithInvalidSessionCreatesRecoverySession) {
    json test_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};

    http::headers_map headers = {{"Mcp-Session-Id", "invalid-session-id-12345"}};

    auto res = http_client->post("/mcp", headers, test_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    // Server creates a recovery session instead of returning 404
    BOOST_CHECK_EQUAL(200, res.status_code);
    // Response should contain a new session ID (different from the invalid one)
    auto new_session = res.headers.find("Mcp-Session-Id");
    BOOST_REQUIRE(new_session != res.headers.end());
    BOOST_CHECK(new_session->second != "invalid-session-id-12345");
}

// Test: Stateless session ID can be reused synchronously without SSE
BOOST_AUTO_TEST_CASE(StatelessSessionIdCanBeReused) {
    json init_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {{"protocolVersion", MCP_VERSION}, {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

    http::headers_map empty_headers;
    auto init_res = http_client->post("/mcp", empty_headers, init_request.dump(), "application/json");

    BOOST_REQUIRE(init_res.success);
    BOOST_REQUIRE_EQUAL(200, init_res.status_code);

    auto session_header = init_res.headers.find("Mcp-Session-Id");
    BOOST_REQUIRE(session_header != init_res.headers.end());
    std::string session_id = session_header->second;
    BOOST_REQUIRE(!session_id.empty());

    // Reuse the session ID in a follow-up request; should still return 200 with direct body
    json ping_request = {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "ping"}};
    http::headers_map headers = {{"Mcp-Session-Id", session_id}};
    auto ping_res = http_client->post("/mcp", headers, ping_request.dump(), "application/json");

    BOOST_REQUIRE(ping_res.success);
    BOOST_CHECK_EQUAL(200, ping_res.status_code);

    json ping_body = json::parse(ping_res.body);
    BOOST_CHECK(ping_body.contains("result"));
}

// Test: DELETE /mcp terminates session
BOOST_AUTO_TEST_CASE(DeleteMcpTerminatesSession) {
    // First, establish a session
    std::string endpoint = establish_sse_session_simple();

    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }

    BOOST_REQUIRE(!session_id.empty());

    // Delete the session
    http::headers_map headers = {{"Mcp-Session-Id", session_id}};

    // DELETE method not yet implemented in Beast client
    // This test is disabled until DELETE is implemented
    BOOST_TEST_MESSAGE("Test skipped: DELETE method not yet implemented in Beast client");
    return;

    // auto delete_res = http_client->delete_("/mcp", headers);
    // BOOST_REQUIRE(delete_res);
    // BOOST_CHECK_EQUAL(204, delete_res.status_code);

    // Verify session is gone by trying to POST with a real method (not ping)
    json test_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};

    auto post_res = http_client->post("/mcp", headers, test_request.dump(), "application/json");

    BOOST_REQUIRE(post_res);
    BOOST_CHECK_EQUAL(404, post_res.status_code);
}

// Test: DELETE /mcp without session ID returns 400
BOOST_AUTO_TEST_CASE(DeleteMcpWithoutSessionReturns400) {
    // DELETE method not yet implemented in Beast client
    BOOST_TEST_MESSAGE("Test skipped: DELETE method not yet implemented in Beast client");
    return;

    // auto res = http_client->delete_("/mcp");
    // BOOST_REQUIRE(res.success);
    // BOOST_CHECK_EQUAL(400, res.status_code);
}

// Test: DELETE /mcp with invalid session ID returns 404
BOOST_AUTO_TEST_CASE(DeleteMcpWithInvalidSessionReturns404) {
    // DELETE method not yet implemented in Beast client
    BOOST_TEST_MESSAGE("Test skipped: DELETE method not yet implemented in Beast client");
    return;

    // http::headers_map headers = {
    //     {"Mcp-Session-Id", "invalid-session-12345"}
    // };
    // auto res = http_client->delete_("/mcp", headers);
    // BOOST_REQUIRE(res.success);
    // BOOST_CHECK_EQUAL(404, res.status_code);
}

// Test: Backward compatibility with query parameter
BOOST_AUTO_TEST_CASE(BackwardCompatibilityWithQueryParameter) {
    // Establish a session
    std::string endpoint = establish_sse_session_simple();

    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }

    BOOST_REQUIRE(!session_id.empty());

    // POST using query parameter instead of header
    json ping_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}};

    std::string url_with_session = "/mcp?session_id=" + session_id;
    http::headers_map headers;
    auto res = http_client->post(url_with_session, headers, ping_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(202, res.status_code);
}

// Test: CORS headers include Mcp-Session-Id
BOOST_AUTO_TEST_CASE(CorsHeadersIncludeMcpSessionId) {
    // OPTIONS method not yet implemented in Beast client
    BOOST_TEST_MESSAGE("Test skipped: OPTIONS method not yet implemented in Beast client");
    return;

    // auto res = http_client->options("/mcp");
    // BOOST_REQUIRE(res.success);
    // BOOST_CHECK_EQUAL(204, res.status_code);

    // // Check CORS headers
    // auto allow_methods = res.headers.find("Access-Control-Allow-Methods");
    // BOOST_REQUIRE_NE(allow_methods, res.headers.end());
    // BOOST_CHECK_NE(allow_methods->second.find("GET"), std::string::npos);
    // BOOST_CHECK_NE(allow_methods->second.find("POST"), std::string::npos);
    // BOOST_CHECK_NE(allow_methods->second.find("DELETE"), std::string::npos);
    //
    // auto allow_headers = res.headers.find("Access-Control-Allow-Headers");
    // BOOST_REQUIRE_NE(allow_headers, res.headers.end());
    // BOOST_CHECK_NE(allow_headers->second.find("Mcp-Session-Id"), std::string::npos);
}

// Test: Legacy /sse endpoint still works
BOOST_AUTO_TEST_CASE(LegacySseEndpointWorks) {
    std::string endpoint;
    bool got_endpoint = false;
    std::mutex mtx;
    std::condition_variable cv;
    std::string sse_url = "http://localhost:" + std::to_string(port_);
    auto sse_client = http::create_client(sse_url);
    std::atomic<http::client_interface*> client_ptr{nullptr};
    auto* sse_client_ptr = sse_client.get(); // Capture raw pointer for thread safety

    std::thread sse_thread([&, sse_client_ptr]() {
        client_ptr.store(sse_client_ptr, std::memory_order_release);

        sse_client_ptr->get_stream("/sse", [&](const char* data, size_t len) {
            std::string response(data, len);

            if (response.find("endpoint") != std::string::npos) {
                size_t pos = response.find("data: ");
                if (pos != std::string::npos) {
                    std::string endpoint_data = response.substr(pos + 6);
                    endpoint_data = endpoint_data.substr(0, endpoint_data.find("\r\n"));

                    std::lock_guard<std::mutex> lock(mtx);
                    endpoint = endpoint_data;
                    got_endpoint = true;
                    cv.notify_one();
                    return false; // Stop reading after getting endpoint
                }
            }
            return true;
        });

        client_ptr.store(nullptr, std::memory_order_release);
    });

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(3), [&] { return got_endpoint; });
    }

    // Join the thread to ensure it completes before sse_client is destroyed
    if (sse_thread.joinable()) {
        sse_thread.join();
    }

    BOOST_CHECK(!endpoint.empty());
    BOOST_CHECK_NE(endpoint.find("/message"), std::string::npos);
}

// Test: POST with unsupported Accept header returns 406
BOOST_AUTO_TEST_CASE(PostMcpWithUnsupportedAcceptReturns406) {
    // Establish a session
    std::string endpoint = establish_sse_session_simple();

    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }

    BOOST_REQUIRE(!session_id.empty());

    // POST with unsupported Accept header (e.g., only text/html)
    json test_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};

    http::headers_map headers = {
        {"Mcp-Session-Id", session_id}, {"Accept", "text/html"} // Unsupported media type
    };

    auto res = http_client->post("/mcp", headers, test_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(406, res.status_code);
}

// Test: POST with valid Accept headers (application/json and text/event-stream) succeeds
BOOST_AUTO_TEST_CASE(PostMcpWithValidAcceptHeaderSucceeds) {
    // Establish a session
    std::string endpoint = establish_sse_session_simple();

    std::regex session_regex("session_id=([^&]+)");
    std::smatch match;
    std::string session_id;
    if (std::regex_search(endpoint, match, session_regex)) {
        session_id = match[1].str();
    }

    BOOST_REQUIRE(!session_id.empty());

    // POST with proper Accept header
    json test_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}};

    http::headers_map headers = {
        {"Mcp-Session-Id", session_id}, {"Accept", "application/json, text/event-stream"} // Both supported types
    };

    auto res = http_client->post("/mcp", headers, test_request.dump(), "application/json");

    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(202, res.status_code);
}

// Test: CORS headers include Accept in allowed headers
// DISABLED: OPTIONS method not yet implemented in Beast client
BOOST_AUTO_TEST_CASE(DISABLED_CorsHeadersIncludeAccept) {
    // TODO: Re-enable when OPTIONS method is implemented
}

// Test: Multiple concurrent stateless requests work correctly
// This test verifies that when clients (like codex) make multiple requests
// without session IDs, each request is handled independently
BOOST_AUTO_TEST_CASE(ConcurrentStatelessRequestsWork) {
    // Make multiple stateless requests concurrently
    // Use initialize requests since they don't require prior session state
    const int num_requests = 5;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_requests, false);

    for (int i = 0; i < num_requests; ++i) {
        threads.emplace_back([this, i, &results]() {
            json test_request = {
                {"jsonrpc", "2.0"},
                {"id", i + 1},
                {"method", "initialize"},
                {"params",
                 {{"protocolVersion", MCP_VERSION}, {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

            http::headers_map empty_headers; // No session ID
            auto res = http_client->post("/mcp", empty_headers, test_request.dump(), "application/json");

            results[i] = res.success && res.status_code == 200;
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all requests succeeded
    for (int i = 0; i < num_requests; ++i) {
        BOOST_CHECK(results[i]);
    }
}

// Test: Sequential stateless requests work correctly
// This test verifies that when clients make multiple sequential requests
// without session IDs, each request is handled independently
BOOST_AUTO_TEST_CASE(SequentialStatelessRequestsWork) {
    const int num_requests = 5;

    for (int i = 0; i < num_requests; ++i) {
        json test_request = {
            {"jsonrpc", "2.0"},
            {"id", i + 1},
            {"method", "initialize"},
            {"params", {{"protocolVersion", MCP_VERSION}, {"clientInfo", {{"name", "test"}, {"version", "1.0.0"}}}}}};

        http::headers_map empty_headers; // No session ID
        auto res = http_client->post("/mcp", empty_headers, test_request.dump(), "application/json");

        BOOST_REQUIRE(res.success);
        BOOST_CHECK_EQUAL(200, res.status_code);

        // Parse response to ensure it's valid
        json body = json::parse(res.body);
        BOOST_CHECK(body.contains("result"));
        BOOST_CHECK(body["result"].contains("protocolVersion"));
    }
}

BOOST_AUTO_TEST_SUITE_END()
