/**
 * @file boost_integration_test.cpp
 * @brief Test to verify Boost.Beast integration
 * 
 * This test ensures that Boost.Beast headers are properly installed
 * and can be included in the project.
 */

#include <gtest/gtest.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

/**
 * Test that Boost.Beast headers can be included
 */
TEST(BoostIntegrationTest, HeadersAvailable) {
    // Verify we can access Boost.Beast version
    EXPECT_GT(BOOST_BEAST_VERSION, 0);
    
    // Verify we can create basic Boost.Beast types
    beast::error_code ec;
    EXPECT_FALSE(ec);
}

/**
 * Test that we can create basic HTTP message types
 */
TEST(BoostIntegrationTest, BasicHttpTypes) {
    // Create an HTTP request
    http::request<http::string_body> req{http::verb::get, "/", 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    
    EXPECT_EQ(req.method(), http::verb::get);
    EXPECT_EQ(req.target(), "/");
    EXPECT_EQ(req.version(), 11);
    
    // Create an HTTP response
    http::response<http::string_body> res{http::status::ok, 11};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.body() = "Hello, World!";
    res.prepare_payload();
    
    EXPECT_EQ(res.result(), http::status::ok);
    EXPECT_EQ(res.body(), "Hello, World!");
}

/**
 * Test that we can create Boost.Asio I/O context
 */
TEST(BoostIntegrationTest, AsioContext) {
    // Create an I/O context
    net::io_context ioc;
    
    // Verify we can run it (should return immediately with no work)
    std::size_t handlers_run = ioc.run();
    EXPECT_EQ(handlers_run, 0);
    
    // Verify we can reset and poll
    ioc.restart();
    std::size_t polled = ioc.poll();
    EXPECT_EQ(polled, 0);
}
