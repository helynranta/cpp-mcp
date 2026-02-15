/**
 * @file http_abstraction_test.cpp
 * @brief Tests for HTTP abstraction layer interfaces
 * 
 * Tests the core HTTP abstraction interfaces defined in mcp_http_abstraction.h.
 * These tests validate the abstractions work correctly independent of implementation.
 */

#include <gtest/gtest.h>
#include "mcp_http_abstraction.h"
#include "http_test_utilities.h"
#include <memory>

using namespace mcp::http;
using namespace mcp::http::test;

/**
 * Test request_data structure and helper methods
 */
class RequestDataTest : public ::testing::Test {
protected:
    request_data req;
};

TEST_F(RequestDataTest, DefaultConstruction) {
    EXPECT_TRUE(req.method.empty());
    EXPECT_TRUE(req.path.empty());
    EXPECT_TRUE(req.body.empty());
    EXPECT_TRUE(req.remote_addr.empty());
    EXPECT_EQ(0, req.remote_port);
    EXPECT_TRUE(req.headers.empty());
}

TEST_F(RequestDataTest, BasicFields) {
    req.method = "GET";
    req.path = "/api/test";
    req.body = "test body";
    req.remote_addr = "127.0.0.1";
    req.remote_port = 12345;
    
    EXPECT_EQ("GET", req.method);
    EXPECT_EQ("/api/test", req.path);
    EXPECT_EQ("test body", req.body);
    EXPECT_EQ("127.0.0.1", req.remote_addr);
    EXPECT_EQ(12345, req.remote_port);
}

TEST_F(RequestDataTest, HeaderManagement) {
    req.headers.insert({"Content-Type", "application/json"});
    req.headers.insert({"Accept", "text/html"});
    req.headers.insert({"X-Custom", "value1"});
    req.headers.insert({"X-Custom", "value2"}); // Duplicate key
    
    EXPECT_EQ(4, req.headers.size());
    
    // Test get_header for existing header
    auto content_type = req.get_header("Content-Type");
    ASSERT_TRUE(content_type.has_value());
    EXPECT_EQ("application/json", content_type.value());
    
    // Test get_header for non-existent header
    auto missing = req.get_header("Missing-Header");
    EXPECT_FALSE(missing.has_value());
    
    // Test multiple values (get_header returns first)
    auto custom = req.get_header("X-Custom");
    ASSERT_TRUE(custom.has_value());
    EXPECT_TRUE(custom.value() == "value1" || custom.value() == "value2");
}

/**
 * Test client_result structure and helper methods
 */
class ClientResultTest : public ::testing::Test {
protected:
    client_result result;
};

TEST_F(ClientResultTest, DefaultConstruction) {
    EXPECT_FALSE(result.success);
    EXPECT_EQ(0, result.status_code);
    EXPECT_TRUE(result.body.empty());
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(result.headers.empty());
}

TEST_F(ClientResultTest, SuccessfulRequest) {
    result.success = true;
    result.status_code = 200;
    result.body = "{\"status\": \"ok\"}";
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(static_cast<bool>(result)); // Test implicit conversion
}

TEST_F(ClientResultTest, FailedRequest) {
    result.success = false;
    result.error_message = "Connection timeout";
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.is_ok());
    EXPECT_FALSE(static_cast<bool>(result));
}

TEST_F(ClientResultTest, IsOkWithVariousStatusCodes) {
    // 2xx status codes should be OK
    result.success = true;
    result.status_code = 200;
    EXPECT_TRUE(result.is_ok());
    
    result.status_code = 201;
    EXPECT_TRUE(result.is_ok());
    
    result.status_code = 204;
    EXPECT_TRUE(result.is_ok());
    
    // 3xx redirects should not be OK
    result.status_code = 301;
    EXPECT_FALSE(result.is_ok());
    
    result.status_code = 302;
    EXPECT_FALSE(result.is_ok());
    
    // 4xx client errors should not be OK
    result.status_code = 400;
    EXPECT_FALSE(result.is_ok());
    
    result.status_code = 404;
    EXPECT_FALSE(result.is_ok());
    
    // 5xx server errors should not be OK
    result.status_code = 500;
    EXPECT_FALSE(result.is_ok());
    
    result.status_code = 503;
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ClientResultTest, IsOkRequiresBothSuccessAndStatus) {
    // Success but wrong status
    result.success = true;
    result.status_code = 404;
    EXPECT_FALSE(result.is_ok());
    
    // Right status but no success
    result.success = false;
    result.status_code = 200;
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ClientResultTest, HeadersInResult) {
    result.success = true;
    result.status_code = 200;
    result.headers.insert({"Content-Type", "application/json"});
    result.headers.insert({"Cache-Control", "no-cache"});
    
    EXPECT_EQ(2, result.headers.size());
    
    auto it = result.headers.find("Content-Type");
    ASSERT_NE(it, result.headers.end());
    EXPECT_EQ("application/json", it->second);
}

/**
 * Test mock implementations of abstraction interfaces
 */
TEST(StreamingDataSinkTest, MockWriteSuccess) {
    MockDataSink sink;
    sink.should_succeed = true;
    
    const char* data = "Hello, World!";
    bool result = sink.write(data, 13);
    
    EXPECT_TRUE(result);
    EXPECT_EQ("Hello, World!", sink.written_data);
}

TEST(StreamingDataSinkTest, MockWriteFailure) {
    MockDataSink sink;
    sink.should_succeed = false;
    
    const char* data = "Hello, World!";
    bool result = sink.write(data, 13);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(sink.written_data.empty());
}

TEST(StreamingDataSinkTest, MultipleWrites) {
    MockDataSink sink;
    
    sink.write("First ", 6);
    sink.write("Second ", 7);
    sink.write("Third", 5);
    
    EXPECT_EQ("First Second Third", sink.written_data);
}

TEST(ResponseBuilderTest, SetStatus) {
    MockResponseBuilder builder;
    
    builder.set_status(200);
    EXPECT_EQ(200, builder.status);
    
    builder.set_status(404);
    EXPECT_EQ(404, builder.status);
}

TEST(ResponseBuilderTest, SetHeaders) {
    MockResponseBuilder builder;
    
    builder.set_header("Content-Type", "text/plain");
    builder.set_header("Cache-Control", "no-cache");
    
    EXPECT_EQ(2, builder.headers.size());
    EXPECT_EQ("text/plain", builder.headers["Content-Type"]);
    EXPECT_EQ("no-cache", builder.headers["Cache-Control"]);
}

TEST(ResponseBuilderTest, SetContent) {
    MockResponseBuilder builder;
    
    builder.set_content("{\"status\": \"ok\"}", "application/json");
    
    EXPECT_EQ("{\"status\": \"ok\"}", builder.body);
    EXPECT_EQ("application/json", builder.content_type);
}

TEST(ResponseBuilderTest, SetChunkedContentProvider) {
    MockResponseBuilder builder;
    
    bool provider_called = false;
    auto provider = [&provider_called](size_t offset, streaming_data_sink& sink) -> bool {
        provider_called = true;
        sink.write("data: test\n\n", 12);
        return false; // Done
    };
    
    builder.set_chunked_content_provider("text/event-stream", provider);
    
    EXPECT_TRUE(builder.has_chunked_provider);
    EXPECT_EQ("text/event-stream", builder.content_type);
    
    // Test calling the provider
    MockDataSink sink;
    builder.chunked_provider(0, sink);
    
    EXPECT_TRUE(provider_called);
    EXPECT_EQ("data: test\n\n", sink.written_data);
}

TEST(ResponseBuilderTest, ChunkedProviderMultipleChunks) {
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
    EXPECT_TRUE(continue1);
    
    bool continue2 = builder.chunked_provider(7, sink);
    EXPECT_TRUE(continue2);
    
    bool continue3 = builder.chunked_provider(14, sink);
    EXPECT_FALSE(continue3);
    
    EXPECT_EQ("chunk1\nchunk2\nchunk3\n", sink.written_data);
    EXPECT_EQ(3, call_count);
}

/**
 * Test headers_map type alias
 */
TEST(HeadersMapTest, MultipleValuesForSameKey) {
    headers_map headers;
    
    headers.insert({"Set-Cookie", "cookie1=value1"});
    headers.insert({"Set-Cookie", "cookie2=value2"});
    headers.insert({"Content-Type", "text/html"});
    
    EXPECT_EQ(3, headers.size());
    
    // Count occurrences of Set-Cookie
    auto range = headers.equal_range("Set-Cookie");
    int cookie_count = std::distance(range.first, range.second);
    EXPECT_EQ(2, cookie_count);
}

TEST(HeadersMapTest, CaseSensitivity) {
    headers_map headers;
    
    headers.insert({"Content-Type", "text/html"});
    headers.insert({"content-type", "application/json"}); // Different case
    
    // std::multimap is case-sensitive by default
    EXPECT_EQ(2, headers.size());
    
    auto it1 = headers.find("Content-Type");
    ASSERT_NE(it1, headers.end());
    EXPECT_EQ("text/html", it1->second);
    
    auto it2 = headers.find("content-type");
    ASSERT_NE(it2, headers.end());
    EXPECT_EQ("application/json", it2->second);
}
