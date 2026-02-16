/**
 * @file boost_integration_test.cpp
 * @brief Test to verify Boost.Beast integration
 *
 * This test ensures that Boost.Beast headers are properly installed
 * and can be included in the project.
 */

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/test/unit_test.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

// HTTP version constant for HTTP/1.1
constexpr int HTTP_1_1 = 11;

BOOST_AUTO_TEST_SUITE(BoostIntegrationTest)

/**
 * Test that Boost.Beast headers can be included
 */
BOOST_AUTO_TEST_CASE(HeadersAvailable) {
    // Verify we can access Boost.Beast version
    BOOST_CHECK_GT(BOOST_BEAST_VERSION, 0);

    // Verify we can create basic Boost.Beast types
    beast::error_code ec;
    BOOST_CHECK(!ec);
}

/**
 * Test that we can create basic HTTP message types
 */
BOOST_AUTO_TEST_CASE(BasicHttpTypes) {
    // Create an HTTP request
    http::request<http::string_body> req{http::verb::get, "/", HTTP_1_1};
    req.set(http::field::host, "localhost");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    BOOST_CHECK_EQUAL(req.method(), http::verb::get);
    BOOST_CHECK_EQUAL(req.target(), "/");
    BOOST_CHECK_EQUAL(req.version(), HTTP_1_1);

    // Create an HTTP response
    http::response<http::string_body> res{http::status::ok, HTTP_1_1};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.body() = "Hello, World!";
    res.prepare_payload();

    BOOST_CHECK_EQUAL(res.result(), http::status::ok);
    BOOST_CHECK_EQUAL(res.body(), "Hello, World!");
}

/**
 * Test that we can create Boost.Asio I/O context
 */
BOOST_AUTO_TEST_CASE(AsioContext) {
    // Create an I/O context
    net::io_context ioc;

    // Verify we can run it (should return immediately with no work)
    std::size_t handlers_run = ioc.run();
    BOOST_CHECK_EQUAL(handlers_run, 0);

    // Verify we can reset and poll
    ioc.restart();
    std::size_t polled = ioc.poll();
    BOOST_CHECK_EQUAL(polled, 0);
}

BOOST_AUTO_TEST_SUITE_END()
