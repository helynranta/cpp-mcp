/**
 * @file http_abstraction_test.cpp
 * @brief Tests for HTTP abstraction layer interfaces
 * 
 * Tests the core HTTP abstraction interfaces defined in mcp_http_abstraction.h.
 * These tests validate the abstractions work correctly independent of implementation.
 */

#include <boost/test/unit_test.hpp>
#include "mcp_http_abstraction.h"
#include "http_test_utilities.h"
#include <memory>

using namespace mcp::http;
using namespace mcp::http::test;

/**
 * Test request_data structure and helper methods
 */
struct RequestDataTest {
    request_data req;
};

BOOST_FIXTURE_TEST_SUITE(RequestDataTestSuite, RequestDataTest)

BOOST_AUTO_TEST_CASE(DefaultConstruction) {
    BOOST_CHECK(req.method.empty());
    BOOST_CHECK(req.path.empty());
    BOOST_CHECK(req.body.empty());
    BOOST_CHECK(req.remote_addr.empty());
    BOOST_CHECK_EQUAL(0, req.remote_port);
    BOOST_CHECK(req.headers.empty());
}

BOOST_AUTO_TEST_CASE(BasicFields) {
    req.method = "GET";
    req.path = "/api/test";
    req.body = "test body";
    req.remote_addr = "127.0.0.1";
    req.remote_port = 12345;
    
    BOOST_CHECK_EQUAL("GET", req.method);
    BOOST_CHECK_EQUAL("/api/test", req.path);
    BOOST_CHECK_EQUAL("test body", req.body);
    BOOST_CHECK_EQUAL("127.0.0.1", req.remote_addr);
    BOOST_CHECK_EQUAL(12345, req.remote_port);
}

BOOST_AUTO_TEST_CASE(HeaderManagement) {
    req.headers.insert({"Content-Type", "application/json"});
    req.headers.insert({"Accept", "text/html"});
    req.headers.insert({"X-Custom", "value1"});
    req.headers.insert({"X-Custom", "value2"}); // Duplicate key
    
    BOOST_CHECK_EQUAL(4, req.headers.size());
    
    // Test get_header for existing header
    auto content_type = req.get_header("Content-Type");
    BOOST_REQUIRE(content_type.has_value());
    BOOST_CHECK_EQUAL("application/json", content_type.value());
    
    // Test get_header for non-existent header
    auto missing = req.get_header("Missing-Header");
    BOOST_CHECK(!missing.has_value());
    
    // Test multiple values (get_header returns first)
    auto custom = req.get_header("X-Custom");
    BOOST_REQUIRE(custom.has_value());
    BOOST_CHECK(custom.value() == "value1" || custom.value() == "value2");
}

BOOST_AUTO_TEST_SUITE_END()

/**
 * Test client_result structure and helper methods
 */
struct ClientResultTest {
    client_result result;
};

BOOST_FIXTURE_TEST_SUITE(ClientResultTestSuite, ClientResultTest)

BOOST_AUTO_TEST_CASE(DefaultConstruction) {
    BOOST_CHECK(!result.success);
    BOOST_CHECK_EQUAL(0, result.status_code);
    BOOST_CHECK(result.body.empty());
    BOOST_CHECK(result.error_message.empty());
    BOOST_CHECK(result.headers.empty());
}

BOOST_AUTO_TEST_CASE(SuccessfulRequest) {
    result.success = true;
    result.status_code = 200;
    result.body = "{\"status\": \"ok\"}";
    
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.is_ok());
    BOOST_CHECK(static_cast<bool>(result)); // Test implicit conversion
}

BOOST_AUTO_TEST_CASE(FailedRequest) {
    result.success = false;
    result.error_message = "Connection timeout";
    
    BOOST_CHECK(!result.success);
    BOOST_CHECK(!result.is_ok());
    BOOST_CHECK(!static_cast<bool>(result));
}

BOOST_AUTO_TEST_CASE(IsOkWithVariousStatusCodes) {
    // 2xx status codes should be OK
    result.success = true;
    result.status_code = 200;
    BOOST_CHECK(result.is_ok());
    
    result.status_code = 201;
    BOOST_CHECK(result.is_ok());
    
    result.status_code = 204;
    BOOST_CHECK(result.is_ok());
    
    // 3xx redirects should not be OK
    result.status_code = 301;
    BOOST_CHECK(!result.is_ok());
    
    result.status_code = 302;
    BOOST_CHECK(!result.is_ok());
    
    // 4xx client errors should not be OK
    result.status_code = 400;
    BOOST_CHECK(!result.is_ok());
    
    result.status_code = 404;
    BOOST_CHECK(!result.is_ok());
    
    // 5xx server errors should not be OK
    result.status_code = 500;
    BOOST_CHECK(!result.is_ok());
    
    result.status_code = 503;
    BOOST_CHECK(!result.is_ok());
}

BOOST_AUTO_TEST_CASE(IsOkRequiresBothSuccessAndStatus) {
    // Success but wrong status
    result.success = true;
    result.status_code = 404;
    BOOST_CHECK(!result.is_ok());
    
    // Right status but no success
    result.success = false;
    result.status_code = 200;
    BOOST_CHECK(!result.is_ok());
}

BOOST_AUTO_TEST_CASE(HeadersInResult) {
    result.success = true;
    result.status_code = 200;
    result.headers.insert({"Content-Type", "application/json"});
    result.headers.insert({"Cache-Control", "no-cache"});
    
    BOOST_CHECK_EQUAL(2, result.headers.size());
    
    auto it = result.headers.find("Content-Type");
    BOOST_REQUIRE(it != result.headers.end());
    BOOST_CHECK_EQUAL("application/json", it->second);
}

BOOST_AUTO_TEST_SUITE_END()

/**
 * Test mock implementations of abstraction interfaces
 */
BOOST_AUTO_TEST_SUITE(StreamingDataSinkTestSuite)

BOOST_AUTO_TEST_CASE(MockWriteSuccess) {
    MockDataSink sink;
    sink.should_succeed = true;
    
    const char* data = "Hello, World!";
    bool result = sink.write(data, 13);
    
    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL("Hello, World!", sink.written_data);
}

BOOST_AUTO_TEST_CASE(MockWriteFailure) {
    MockDataSink sink;
    sink.should_succeed = false;
    
    const char* data = "Hello, World!";
    bool result = sink.write(data, 13);
    
    BOOST_CHECK(!result);
    BOOST_CHECK(sink.written_data.empty());
}

BOOST_AUTO_TEST_CASE(MultipleWrites) {
    MockDataSink sink;
    
    sink.write("First ", 6);
    sink.write("Second ", 7);
    sink.write("Third", 5);
    
    BOOST_CHECK_EQUAL("First Second Third", sink.written_data);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ResponseBuilderTestSuite)

BOOST_AUTO_TEST_CASE(SetStatus) {
    MockResponseBuilder builder;
    
    builder.set_status(200);
    BOOST_CHECK_EQUAL(200, builder.status);
    
    builder.set_status(404);
    BOOST_CHECK_EQUAL(404, builder.status);
}

BOOST_AUTO_TEST_CASE(SetHeaders) {
    MockResponseBuilder builder;
    
    builder.set_header("Content-Type", "text/plain");
    builder.set_header("Cache-Control", "no-cache");
    
    BOOST_CHECK_EQUAL(2, builder.headers.size());
    BOOST_CHECK_EQUAL("text/plain", builder.headers["Content-Type"]);
    BOOST_CHECK_EQUAL("no-cache", builder.headers["Cache-Control"]);
}

BOOST_AUTO_TEST_CASE(SetContent) {
    MockResponseBuilder builder;
    
    builder.set_content("{\"status\": \"ok\"}", "application/json");
    
    BOOST_CHECK_EQUAL("{\"status\": \"ok\"}", builder.body);
    BOOST_CHECK_EQUAL("application/json", builder.content_type);
}

BOOST_AUTO_TEST_CASE(SetChunkedContentProvider) {
    MockResponseBuilder builder;
    
    bool provider_called = false;
    auto provider = [&provider_called](size_t offset, streaming_data_sink& sink) -> bool {
        provider_called = true;
        sink.write("data: test\n\n", 12);
        return false; // Done
    };
    
    builder.set_chunked_content_provider("text/event-stream", provider);
    
    BOOST_CHECK(builder.has_chunked_provider);
    BOOST_CHECK_EQUAL("text/event-stream", builder.content_type);
    
    // Test calling the provider
    MockDataSink sink;
    builder.chunked_provider(0, sink);
    
    BOOST_CHECK(provider_called);
    BOOST_CHECK_EQUAL("data: test\n\n", sink.written_data);
}

BOOST_AUTO_TEST_CASE(ChunkedProviderMultipleChunks) {
    MockResponseBuilder builder;
    
    int call_count = 0;
    auto provider = [&call_count](size_t offset, streaming_data_sink& sink) -> bool {
        call_count++;
        if (call_count == 1) {
            sink.write("chunk1\n", 7);
            return true; // Continue
        } else if (call_count == 2) {
            sink.write("chunk2\n", 7);
            return true; // Continue
        } else {
            sink.write("chunk3\n", 7);
            return false; // Done
        }
    };
    
    builder.set_chunked_content_provider("text/event-stream", provider);
    
    MockDataSink sink;
    
    // Call provider multiple times
    bool continue1 = builder.chunked_provider(0, sink);
    BOOST_CHECK(continue1);
    
    bool continue2 = builder.chunked_provider(7, sink);
    BOOST_CHECK(continue2);
    
    bool continue3 = builder.chunked_provider(14, sink);
    BOOST_CHECK(!continue3);
    
    BOOST_CHECK_EQUAL("chunk1\nchunk2\nchunk3\n", sink.written_data);
    BOOST_CHECK_EQUAL(3, call_count);
}

BOOST_AUTO_TEST_SUITE_END()

/**
 * Test headers_map type alias
 */
BOOST_AUTO_TEST_SUITE(HeadersMapTestSuite)

BOOST_AUTO_TEST_CASE(MultipleValuesForSameKey) {
    headers_map headers;
    
    headers.insert({"Set-Cookie", "cookie1=value1"});
    headers.insert({"Set-Cookie", "cookie2=value2"});
    headers.insert({"Content-Type", "text/html"});
    
    BOOST_CHECK_EQUAL(3, headers.size());
    
    // Count occurrences of Set-Cookie
    auto range = headers.equal_range("Set-Cookie");
    int cookie_count = std::distance(range.first, range.second);
    BOOST_CHECK_EQUAL(2, cookie_count);
}

BOOST_AUTO_TEST_CASE(CaseSensitivity) {
    headers_map headers;
    
    headers.insert({"Content-Type", "text/html"});
    headers.insert({"content-type", "application/json"}); // Different case
    
    // std::multimap is case-sensitive by default
    BOOST_CHECK_EQUAL(2, headers.size());
    
    auto it1 = headers.find("Content-Type");
    BOOST_REQUIRE(it1 != headers.end());
    BOOST_CHECK_EQUAL("text/html", it1->second);
    
    auto it2 = headers.find("content-type");
    BOOST_REQUIRE(it2 != headers.end());
    BOOST_CHECK_EQUAL("application/json", it2->second);
}

BOOST_AUTO_TEST_SUITE_END()
