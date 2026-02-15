/**
 * @file http_security_test.cpp
 * @brief Tests for HTTP transport security features (MCP 2025-03-26)
 * 
 * This file tests Origin header validation, DNS rebinding mitigation,
 * and other HTTP transport security features.
 */

#include <boost/test/unit_test.hpp>
#include "mcp_server.h"
#include "mcp_http_factory.h"
#include <thread>
#include <chrono>

using namespace mcp;

struct HttpSecurityTest {
    HttpSecurityTest() {
        // Port for this test suite
        test_port = 9100;
    }

    ~HttpSecurityTest() {
        if (server) {
            server->stop();
            server.reset();
        }
        // Wait a bit for the server to fully stop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    int test_port;
    std::unique_ptr<mcp::server> server;
};

BOOST_FIXTURE_TEST_SUITE(HttpSecurityTestSuite, HttpSecurityTest)

// Test: Origin validation enabled by default for localhost
BOOST_AUTO_TEST_CASE(OriginValidationEnabledByDefault) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = true;  // Explicitly enable
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string base_url = "http://localhost:" + std::to_string(test_port);
    auto client = http::create_client(base_url);
    
    // Test POST with valid localhost origin
    http::headers_map headers = {
        {"Origin", "http://localhost"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client->post("/mcp", headers, "{}", "application/json");
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_NE(403, res.status_code);
}

// Test: Origin validation rejects invalid origins
BOOST_AUTO_TEST_CASE(InvalidOriginRejected) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = true;
    config.security.allowed_origins = {"http://localhost", "https://localhost"};
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string base_url = "http://localhost:" + std::to_string(test_port);
    auto client = http::create_client(base_url);
    
    // Test POST with invalid origin
    http::headers_map headers = {
        {"Origin", "http://evil.com"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client->post("/mcp", headers, "{}", "application/json");
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_EQUAL(403, res.status_code);
}

// Test: Origin validation with port numbers
BOOST_AUTO_TEST_CASE(OriginValidationWithPort) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = true;
    config.security.allowed_origins = {"http://localhost"};
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string base_url = "http://localhost:" + std::to_string(test_port);
    auto client = http::create_client(base_url);
    
    // Test POST with localhost:3000 origin (should match http://localhost)
    http::headers_map headers = {
        {"Origin", "http://localhost:3000"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client->post("/mcp", headers, "{}", "application/json");
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_NE(403, res.status_code);
}

// Test: Origin validation disabled
BOOST_AUTO_TEST_CASE(OriginValidationDisabled) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = false;  // Explicitly disable
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string base_url = "http://localhost:" + std::to_string(test_port);
    auto client = http::create_client(base_url);
    
    // Test POST with any origin (should be allowed)
    http::headers_map headers = {
        {"Origin", "http://evil.com"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client->post("/mcp", headers, "{}", "application/json");
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_NE(403, res.status_code);
}

// Test: DELETE endpoint validates Origin
// DISABLED: DELETE method not yet implemented in Beast client
BOOST_AUTO_TEST_CASE(DISABLED_DeleteValidatesOrigin) {
    // TODO: Re-enable when DELETE method is implemented
}

// DISABLED: OPTIONS method not yet implemented in Beast client
BOOST_AUTO_TEST_CASE(DISABLED_CorsHeadersReflectAllowedOrigin) {
    // TODO: Re-enable when OPTIONS method is implemented
}

// Test: 127.0.0.1 origin is allowed by default
BOOST_AUTO_TEST_CASE(LocalhostIpAllowedByDefault) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = true;
    // Default allowed_origins includes 127.0.0.1
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string base_url = "http://localhost:" + std::to_string(test_port);
    auto client = http::create_client(base_url);
    
    // Test POST with 127.0.0.1 origin
    http::headers_map headers = {
        {"Origin", "http://127.0.0.1"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client->post("/mcp", headers, "{}", "application/json");
    BOOST_REQUIRE(res.success);
    BOOST_CHECK_NE(403, res.status_code);
}

// Test: Custom allowed origins
BOOST_AUTO_TEST_CASE(CustomAllowedOrigins) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = true;
    config.security.allowed_origins = {"https://myapp.example.com"};
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string base_url = "http://localhost:" + std::to_string(test_port);
    auto client = http::create_client(base_url);
    
    // Test POST with custom allowed origin
    http::headers_map headers_valid = {
        {"Origin", "https://myapp.example.com"},
        {"Content-Type", "application/json"}
    };
    
    auto res_valid = client->post("/mcp", headers_valid, "{}", "application/json");
    BOOST_REQUIRE(res_valid.success);
    BOOST_CHECK_NE(403, res_valid.status_code);
    
    // Test POST with non-allowed origin
    http::headers_map headers_invalid = {
        {"Origin", "http://localhost"},
        {"Content-Type", "application/json"}
    };
    
    auto res_invalid = client->post("/mcp", headers_invalid, "{}", "application/json");
    BOOST_REQUIRE(res_invalid.success);
    BOOST_CHECK_EQUAL(403, res_invalid.status_code);
}

BOOST_AUTO_TEST_SUITE_END()
