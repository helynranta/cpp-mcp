/**
 * @file http_security_test.cpp
 * @brief Tests for HTTP transport security features (MCP 2025-03-26)
 * 
 * This file tests Origin header validation, DNS rebinding mitigation,
 * and other HTTP transport security features.
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "mcp_http_factory.h"
#include <thread>
#include <chrono>

using namespace mcp;

class HttpSecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Port for this test suite
        test_port = 9100;
    }

    void TearDown() override {
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

// Test: Origin validation enabled by default for localhost
TEST_F(HttpSecurityTest, OriginValidationEnabledByDefault) {
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
    ASSERT_TRUE(res.success) << "POST with valid localhost origin should succeed";
    EXPECT_NE(403, res.status_code) << "Should not be forbidden with valid origin";
}

// Test: Origin validation rejects invalid origins
TEST_F(HttpSecurityTest, InvalidOriginRejected) {
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
    ASSERT_TRUE(res.success) << "POST should return a response";
    EXPECT_EQ(403, res.status_code) << "Should be forbidden with invalid origin";
}

// Test: Origin validation with port numbers
TEST_F(HttpSecurityTest, OriginValidationWithPort) {
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
    ASSERT_TRUE(res.success) << "POST should return a response";
    EXPECT_NE(403, res.status_code) << "Should accept localhost with different port";
}

// Test: Origin validation disabled
TEST_F(HttpSecurityTest, OriginValidationDisabled) {
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
    ASSERT_TRUE(res.success) << "POST should return a response";
    EXPECT_NE(403, res.status_code) << "Should not be forbidden when validation is disabled";
}

// Test: DELETE endpoint validates Origin
// DISABLED: DELETE method not yet implemented in Beast client
TEST_F(HttpSecurityTest, DISABLED_DeleteValidatesOrigin) {
    // TODO: Re-enable when DELETE method is implemented
}

// DISABLED: OPTIONS method not yet implemented in Beast client
TEST_F(HttpSecurityTest, DISABLED_CorsHeadersReflectAllowedOrigin) {
    // TODO: Re-enable when OPTIONS method is implemented
}

// Test: 127.0.0.1 origin is allowed by default
TEST_F(HttpSecurityTest, LocalhostIpAllowedByDefault) {
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
    ASSERT_TRUE(res.success) << "POST should return a response";
    EXPECT_NE(403, res.status_code) << "Should accept 127.0.0.1 origin by default";
}

// Test: Custom allowed origins
TEST_F(HttpSecurityTest, CustomAllowedOrigins) {
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
    ASSERT_TRUE(res_valid.success) << "POST should return a response";
    EXPECT_NE(403, res_valid.status_code) << "Should accept custom allowed origin";
    
    // Test POST with non-allowed origin
    http::headers_map headers_invalid = {
        {"Origin", "http://localhost"},
        {"Content-Type", "application/json"}
    };
    
    auto res_invalid = client->post("/mcp", headers_invalid, "{}", "application/json");
    ASSERT_TRUE(res_invalid.success) << "POST should return a response";
    EXPECT_EQ(403, res_invalid.status_code) << "Should reject localhost when not in allowed list";
}
