/**
 * @file http_test_utilities.h
 * @brief Shared test utilities for HTTP abstraction testing
 * 
 * This file provides mock implementations of HTTP abstraction interfaces
 * that can be reused across multiple test files.
 */

#ifndef HTTP_TEST_UTILITIES_H
#define HTTP_TEST_UTILITIES_H

#include "mcp_http_abstraction.h"
#include <string>
#include <map>
#include <functional>

namespace mcp {
namespace http {
namespace test {

/**
 * @brief Mock implementation of streaming_data_sink for testing
 */
class MockDataSink : public streaming_data_sink {
public:
    std::string written_data;
    bool should_succeed = true;
    
    bool write(const char* data, size_t size) override {
        if (should_succeed) {
            written_data.append(data, size);
        }
        return should_succeed;
    }
};

/**
 * @brief Mock implementation of response_builder for testing
 */
class MockResponseBuilder : public response_builder {
public:
    int status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string content_type;
    bool has_chunked_provider = false;
    std::function<bool(size_t, streaming_data_sink&)> chunked_provider;
    
    void set_status(int code) override {
        status = code;
    }
    
    void set_header(const std::string& name, const std::string& value) override {
        headers[name] = value;
    }
    
    void set_content(const std::string& content, const std::string& type) override {
        body = content;
        content_type = type;
    }
    
    void set_chunked_content_provider(
        const std::string& type,
        std::function<bool(size_t, streaming_data_sink&)> provider) override {
        content_type = type;
        chunked_provider = provider;
        has_chunked_provider = true;
    }
};

} // namespace test
} // namespace http
} // namespace mcp

#endif // HTTP_TEST_UTILITIES_H
