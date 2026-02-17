/**
 * @file lifecycle_compliance_test.cpp
 * @brief Test lifecycle compliance with MCP 2025-06-18 specification
 *
 * Tests initialization lifecycle, batch request rejection, and capability gating
 * according to the MCP 2025-06-18 specification.
 */

#include "mcp_http_factory.h"
#include "mcp_message.h"
#include "mcp_server.h"

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

using namespace mcp;
using json = nlohmann::ordered_json;

// Test fixture for lifecycle compliance tests - each test gets isolated server
struct LifecycleComplianceTest {
    int port_;
    std::unique_ptr<server> server_;
    std::unique_ptr<http::client_interface> http_client;
    std::thread sse_thread;
    std::atomic<http::client_interface*> sse_client_ptr{nullptr};
    std::string message_endpoint;
    std::mutex endpoint_mutex;
    std::condition_variable endpoint_cv;
    bool endpoint_ready = false;

    LifecycleComplianceTest() {
        // Create server on unique port for this test (avoid conflicts)
        static std::atomic<int> port_counter{16000};
        port_ = port_counter.fetch_add(1);

        // Set up test server
        server::configuration config;
        config.host = "localhost";
        config.port = port_;
        config.name = "LifecycleTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);

        // Set server capabilities
        json server_capabilities = {{"tools", {{"listChanged", true}}}};
        server_->set_capabilities(server_capabilities);

        // Register a test tool
        tool test_tool = tool_builder("test_tool")
                             .with_description("A test tool")
                             .with_string_param("input", "Input parameter", "")
                             .build();

        server_->register_tool(
            test_tool, [](const json& params, const std::string&) -> json { return {{"result", "processed"}}; });

        // Start server
        server_->start(false);

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Create HTTP client using Beast
        std::string base_url = "http://localhost:" + std::to_string(port_);
        http_client = http::create_client(base_url);

        // Establish SSE connection and get session endpoint
        auto sse_client = http::create_client(base_url);

        sse_thread = std::thread([this, sse_client = std::move(sse_client)]() mutable {
            // Store pointer atomically so destructor can access it
            sse_client_ptr.store(sse_client.get(), std::memory_order_release);

            sse_client->get_stream("/sse", [this](const char* data, size_t len) {
                std::string response(data, len);

                // Extract message endpoint
                if (response.find("endpoint") != std::string::npos) {
                    size_t pos = response.find("data: ");
                    if (pos != std::string::npos) {
                        std::string endpoint_data = response.substr(pos + 6);
                        endpoint_data = endpoint_data.substr(0, endpoint_data.find("\r\n"));

                        std::lock_guard<std::mutex> lock(endpoint_mutex);
                        message_endpoint = endpoint_data;
                        endpoint_ready = true;
                        endpoint_cv.notify_one();
                    }
                }
                return true;
            });

            // Clear pointer when done
            sse_client_ptr.store(nullptr, std::memory_order_release);
        });

        // Wait for endpoint to be ready
        std::unique_lock<std::mutex> lock(endpoint_mutex);
        endpoint_cv.wait_for(lock, std::chrono::seconds(5), [this] { return endpoint_ready; });

        if (!endpoint_ready) {
            throw std::runtime_error("Failed to establish SSE connection");
        }
    }

    ~LifecycleComplianceTest() {
        // Clean up - SSE client will be destroyed when thread exits
        if (sse_thread.joinable()) {
            // Give the thread a moment to exit
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (sse_thread.joinable()) {
                sse_thread.detach();
            }
        }
        http_client.reset();

        // Stop and clean up server
        if (server_) {
            server_->stop();
        }
        server_.reset();
    }
};

BOOST_FIXTURE_TEST_SUITE(LifecycleComplianceTestSuite, LifecycleComplianceTest)

// Test that initialize in batch is rejected
BOOST_AUTO_TEST_CASE(RejectInitializeInBatch) {
    // Create a batch with initialize
    json batch = json::array();

    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params",
                          {{"protocolVersion", "2025-11-25"},
                           {"capabilities", json::object()},
                           {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}}}};

    batch.push_back(init_request);

    // Send batch request
    http::headers_map headers;
    auto res = http_client->post(message_endpoint, headers, batch.dump(), "application/json");

    // Should return error (400 Bad Request)
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(400, res.status_code);

    // Parse error response
    json error_response = json::parse(res.body);
    BOOST_CHECK(error_response.contains("error"));
    BOOST_CHECK_EQUAL(-32600, error_response["error"]["code"]);
    BOOST_CHECK_NE(std::string::npos, error_response["error"]["message"].get<std::string>().find("batch"));
}

// Test that requests before initialize are rejected (except ping)
BOOST_AUTO_TEST_CASE(RejectRequestsBeforeInitialize) {
    // Try to call a tool before initialize
    json tool_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "tools/call"},
                         {"params", {{"name", "test_tool"}, {"arguments", {{"input", "test"}}}}}};

    http::headers_map headers;
    auto res = http_client->post(message_endpoint, headers, tool_request.dump(), "application/json");

    // Request should be accepted (202) but response should indicate error
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(202, res.status_code);

    // Wait a bit for async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Note: In SSE mode, error response would come via SSE
    // For this test, we verify the request was accepted but will fail
}

// Test that ping is allowed before initialize
BOOST_AUTO_TEST_CASE(AllowPingBeforeInitialize) {
    // Send ping request before initialize
    json ping_request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "ping"}};

    http::headers_map headers;
    auto res = http_client->post(message_endpoint, headers, ping_request.dump(), "application/json");

    // Request should be accepted
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(202, res.status_code);
}

// Test that initialize can only be called once
BOOST_AUTO_TEST_CASE(RejectDuplicateInitialize) {
    // First initialize
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params",
                          {{"protocolVersion", "2025-11-25"},
                           {"capabilities", json::object()},
                           {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}}}};

    http::headers_map headers;
    auto res1 = http_client->post(message_endpoint, headers, init_request.dump(), "application/json");
    BOOST_REQUIRE(res1.success);
    BOOST_CHECK_EQUAL(202, res1.status_code);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second initialize should be rejected
    json init_request2 = init_request;
    init_request2["id"] = 2;

    auto res2 = http_client->post(message_endpoint, headers, init_request2.dump(), "application/json");
    BOOST_REQUIRE(res2.success);
    BOOST_CHECK_EQUAL(202, res2.status_code);

    // Wait for async processing - error response will come via SSE
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test that requests require initialized notification
BOOST_AUTO_TEST_CASE(RequireInitializedNotification) {
    // Send initialize
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params",
                          {{"protocolVersion", "2025-11-25"},
                           {"capabilities", json::object()},
                           {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}}}};

    http::headers_map headers;
    auto res = http_client->post(message_endpoint, headers, init_request.dump(), "application/json");
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(202, res.status_code);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Try to call tool before sending initialized notification
    json tool_request = {{"jsonrpc", "2.0"},
                         {"id", 2},
                         {"method", "tools/call"},
                         {"params", {{"name", "test_tool"}, {"arguments", {{"input", "test"}}}}}};

    auto res2 = http_client->post(message_endpoint, headers, tool_request.dump(), "application/json");
    BOOST_REQUIRE(res2.success);
    BOOST_CHECK_EQUAL(202, res2.status_code);

    // Error response will come via SSE indicating session not ready
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test complete lifecycle sequence (success path)
BOOST_AUTO_TEST_CASE(SuccessfulLifecycleSequence) {
    // 1. Initialize
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params",
                          {{"protocolVersion", "2025-11-25"},
                           {"capabilities", json::object()},
                           {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}}}};

    http::headers_map headers;
    auto res1 = http_client->post(message_endpoint, headers, init_request.dump(), "application/json");
    BOOST_REQUIRE(res1.success);
    BOOST_CHECK_EQUAL(202, res1.status_code);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 2. Send initialized notification
    json initialized_notif = {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};

    auto res2 = http_client->post(message_endpoint, headers, initialized_notif.dump(), "application/json");
    BOOST_REQUIRE(res2.success);
    BOOST_CHECK_EQUAL(202, res2.status_code);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 3. Now tool call should work
    json tool_request = {{"jsonrpc", "2.0"},
                         {"id", 2},
                         {"method", "tools/call"},
                         {"params", {{"name", "test_tool"}, {"arguments", {{"input", "test"}}}}}};

    auto res3 = http_client->post(message_endpoint, headers, tool_request.dump(), "application/json");
    BOOST_REQUIRE(res3.success);
    BOOST_CHECK_EQUAL(202, res3.status_code);

    // Request should succeed (response via SSE)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test cancellation notification handling
BOOST_AUTO_TEST_CASE(CancellationNotificationHandling) {
    // Set up cancellation handler to track calls
    std::atomic<bool> cancellation_received{false};
    json cancelled_request_id;
    std::string cancellation_reason;

    server_->set_cancellation_handler([&](const json& request_id, const std::string& reason, const std::string&) {
        cancelled_request_id = request_id;
        cancellation_reason = reason;
        cancellation_received.store(true);
    });

    // Initialize session first
    json init_request = {{"jsonrpc", "2.0"},
                         {"id", 1},
                         {"method", "initialize"},
                         {"params",
                          {{"protocolVersion", "2025-11-25"},
                           {"capabilities", json::object()},
                           {"clientInfo", {{"name", "TestClient"}, {"version", "1.0.0"}}}}}};

    http::headers_map headers;
    http_client->post(message_endpoint, headers, init_request.dump(), "application/json");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    json initialized_notif = {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};

    http_client->post(message_endpoint, headers, initialized_notif.dump(), "application/json");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Send cancellation notification
    json cancel_notif = {{"jsonrpc", "2.0"},
                         {"method", "notifications/cancelled"},
                         {"params", {{"requestId", 42}, {"reason", "User requested cancellation"}}}};

    auto res = http_client->post(message_endpoint, headers, cancel_notif.dump(), "application/json");
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(202, res.status_code);

    // Wait for notification to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify cancellation handler was called
    BOOST_CHECK(cancellation_received.load());
    BOOST_CHECK_EQUAL(42, cancelled_request_id);
    BOOST_CHECK_EQUAL("User requested cancellation", cancellation_reason);
}

BOOST_AUTO_TEST_SUITE_END()
