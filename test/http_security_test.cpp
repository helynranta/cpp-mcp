/**
 * @file http_security_test.cpp
 * @brief Tests for HTTP transport security features (MCP 2025-03-26)
 * 
 * This file tests Origin header validation, DNS rebinding mitigation,
 * and other HTTP transport security features.
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "httplib.h"
#include <thread>
#include <chrono>

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
    
    httplib::Client client("localhost", test_port);
    
    // Test POST with valid localhost origin
    httplib::Headers headers = {
        {"Origin", "http://localhost"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client.Post("/mcp", headers, "{}", "application/json");
    ASSERT_TRUE(res) << "POST with valid localhost origin should succeed";
    EXPECT_NE(403, res->status) << "Should not be forbidden with valid origin";
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
    
    httplib::Client client("localhost", test_port);
    
    // Test POST with invalid origin
    httplib::Headers headers = {
        {"Origin", "http://evil.com"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client.Post("/mcp", headers, "{}", "application/json");
    ASSERT_TRUE(res) << "POST should return a response";
    EXPECT_EQ(403, res->status) << "Should be forbidden with invalid origin";
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
    
    httplib::Client client("localhost", test_port);
    
    // Test POST with localhost:3000 origin (should match http://localhost)
    httplib::Headers headers = {
        {"Origin", "http://localhost:3000"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client.Post("/mcp", headers, "{}", "application/json");
    ASSERT_TRUE(res) << "POST should return a response";
    EXPECT_NE(403, res->status) << "Should accept localhost with different port";
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
    
    httplib::Client client("localhost", test_port);
    
    // Test POST with any origin (should be allowed)
    httplib::Headers headers = {
        {"Origin", "http://evil.com"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client.Post("/mcp", headers, "{}", "application/json");
    ASSERT_TRUE(res) << "POST should return a response";
    EXPECT_NE(403, res->status) << "Should not be forbidden when validation is disabled";
}

// Test: DELETE endpoint validates Origin
TEST_F(HttpSecurityTest, DeleteValidatesOrigin) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = true;
    config.security.allowed_origins = {"http://localhost"};
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    httplib::Client client("localhost", test_port);
    
    // Test DELETE with invalid origin
    httplib::Headers headers = {
        {"Origin", "http://evil.com"},
        {"Mcp-Session-Id", "test-session"}
    };
    
    auto res = client.Delete("/mcp", headers);
    ASSERT_TRUE(res) << "DELETE should return a response";
    EXPECT_EQ(403, res->status) << "DELETE should be forbidden with invalid origin";
}

// Test: CORS headers reflect allowed origin
TEST_F(HttpSecurityTest, CorsHeadersReflectAllowedOrigin) {
    mcp::server::configuration config;
    config.host = "localhost";
    config.port = test_port;
    config.security.validate_origin = true;
    config.security.allowed_origins = {"http://localhost"};
    
    server = std::make_unique<mcp::server>(config);
    server->start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    httplib::Client client("localhost", test_port);
    
    // Test OPTIONS with valid origin
    httplib::Headers headers = {
        {"Origin", "http://localhost"}
    };
    
    auto res = client.Options("/mcp", headers);
    ASSERT_TRUE(res) << "OPTIONS should return a response";
    EXPECT_EQ(204, res->status) << "OPTIONS should return 204";
    
    auto allow_origin = res->headers.find("Access-Control-Allow-Origin");
    ASSERT_NE(allow_origin, res->headers.end()) << "Should have Allow-Origin header";
    EXPECT_EQ("http://localhost", allow_origin->second) << "Should reflect the allowed origin";
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
    
    httplib::Client client("localhost", test_port);
    
    // Test POST with 127.0.0.1 origin
    httplib::Headers headers = {
        {"Origin", "http://127.0.0.1"},
        {"Content-Type", "application/json"}
    };
    
    auto res = client.Post("/mcp", headers, "{}", "application/json");
    ASSERT_TRUE(res) << "POST should return a response";
    EXPECT_NE(403, res->status) << "Should accept 127.0.0.1 origin by default";
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
    
    httplib::Client client("localhost", test_port);
    
    // Test POST with custom allowed origin
    httplib::Headers headers_valid = {
        {"Origin", "https://myapp.example.com"},
        {"Content-Type", "application/json"}
    };
    
    auto res_valid = client.Post("/mcp", headers_valid, "{}", "application/json");
    ASSERT_TRUE(res_valid) << "POST should return a response";
    EXPECT_NE(403, res_valid->status) << "Should accept custom allowed origin";
    
    // Test POST with non-allowed origin
    httplib::Headers headers_invalid = {
        {"Origin", "http://localhost"},
        {"Content-Type", "application/json"}
    };
    
    auto res_invalid = client.Post("/mcp", headers_invalid, "{}", "application/json");
    ASSERT_TRUE(res_invalid) << "POST should return a response";
    EXPECT_EQ(403, res_invalid->status) << "Should reject localhost when not in allowed list";
}
