/**
 * @file beast_adapter_test.cpp
 * @brief Tests for Boost.Beast HTTP adapter implementation
 * 
 * Tests the beast adapter implementation that uses boost::beast to match
 * the HTTP abstraction interfaces. 
 * 
 * Following TDD: Write tests first, then implement to pass tests.
 */

#include <gtest/gtest.h>
#include "mcp_http_beast_adapter.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <sstream>

using namespace mcp::http;
using namespace mcp::http::beast_adapter;
namespace net = boost::asio;
using tcp = net::ip::tcp;

/**
 * Test beast_data_sink wrapper
 * 
 * This test validates that beast_data_sink correctly wraps Beast's
 * write operations to match our streaming_data_sink interface.
 */
TEST(BeastDataSinkTest, WritesChunksCorrectly) {
    // Setup: Create a socket pair for testing
    net::io_context ioc;
    tcp::socket server_socket{ioc};
    tcp::socket client_socket{ioc};
    
    // Create connected socket pair using local endpoint
    tcp::acceptor acceptor{ioc, {net::ip::make_address("127.0.0.1"), 0}};
    auto local_endpoint = acceptor.local_endpoint();
    
    // Start async accept
    std::thread accept_thread([&]() {
        boost::system::error_code ec;
        acceptor.accept(server_socket, ec);
    });
    
    // Connect client
    client_socket.connect(local_endpoint);
    accept_thread.join();
    
    // Test: Create beast_data_sink and write data
    beast_data_sink sink(server_socket);
    
    std::string test_data = "Hello, Beast!";
    bool write_success = sink.write(test_data.c_str(), test_data.size());
    
    EXPECT_TRUE(write_success);
    
    // Verify: Read from client socket and check chunked format
    std::array<char, 256> buffer;
    boost::system::error_code ec;
    size_t bytes_read = client_socket.read_some(net::buffer(buffer), ec);
    
    EXPECT_FALSE(ec);
    EXPECT_GT(bytes_read, 0);
    
    // Parse chunk format: <hex-size>\r\n<data>\r\n
    std::string received(buffer.data(), bytes_read);
    
    // Should contain hex size
    std::stringstream expected;
    expected << std::hex << test_data.size() << "\r\n" << test_data << "\r\n";
    
    EXPECT_EQ(received, expected.str());
}

/**
 * Test beast_response_builder wrapper
 */
TEST(BeastResponseBuilderTest, SetStatus) {
    boost::beast::http::response<boost::beast::http::string_body> res;
    beast_response_builder builder(res);
    
    builder.set_status(200);
    EXPECT_EQ(res.result_int(), 200);
    
    builder.set_status(404);
    EXPECT_EQ(res.result_int(), 404);
    
    builder.set_status(500);
    EXPECT_EQ(res.result_int(), 500);
}

TEST(BeastResponseBuilderTest, SetHeader) {
    boost::beast::http::response<boost::beast::http::string_body> res;
    beast_response_builder builder(res);
    
    builder.set_header("Content-Type", "application/json");
    EXPECT_EQ(res["Content-Type"], "application/json");
    
    builder.set_header("X-Custom-Header", "test-value");
    EXPECT_EQ(res["X-Custom-Header"], "test-value");
}

TEST(BeastResponseBuilderTest, SetContent) {
    boost::beast::http::response<boost::beast::http::string_body> res;
    beast_response_builder builder(res);
    
    std::string body = "{\"status\":\"ok\"}";
    builder.set_content(body, "application/json");
    
    EXPECT_EQ(res.body(), body);
    EXPECT_EQ(res["Content-Type"], "application/json");
}

TEST(BeastResponseBuilderTest, DISABLED_SetChunkedContentProvider) {
    // TODO Phase 2: Implement test
    // Should test:
    // - Setting up chunked transfer encoding
    // - Streaming data via callback
    // - Proper chunk formatting (hex size + CRLF + data + CRLF)
    // - Final chunk (0\r\n\r\n)
}

/**
 * Test beast_server wrapper
 */
TEST(BeastServerTest, RegisterGetHandler) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    bool handler_called = false;
    server->register_get("/test", [&](const request_data& req, response_builder& res) {
        handler_called = true;
        EXPECT_EQ(req.method, "GET");
        EXPECT_EQ(req.path, "/test");
        res.set_status(200);
        res.set_content("{\"message\":\"success\"}", "application/json");
    });
    
    EXPECT_TRUE(server->listen("127.0.0.1", 9998));
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Make request
    try {
        net::io_context ioc;
        tcp::resolver resolver{ioc};
        auto const results = resolver.resolve("127.0.0.1", "9998");
        
        tcp::socket socket{ioc};
        net::connect(socket, results.begin(), results.end());
        
        beast::http::request<beast::http::string_body> req{beast::http::verb::get, "/test", 11};
        req.set(beast::http::field::host, "127.0.0.1");
        beast::http::write(socket, req);
        
        beast::flat_buffer buffer;
        beast::http::response<beast::http::string_body> res;
        beast::http::read(socket, buffer, res);
        
        EXPECT_EQ(res.result_int(), 200);
        EXPECT_EQ(res.body(), "{\"message\":\"success\"}");
        EXPECT_TRUE(handler_called);
        
    } catch (std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
    
    server->stop();
}

TEST(BeastServerTest, RegisterPostHandler) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    std::string received_body;
    server->register_post("/data", [&](const request_data& req, response_builder& res) {
        EXPECT_EQ(req.method, "POST");
        EXPECT_EQ(req.path, "/data");
        received_body = req.body;
        res.set_status(201);
        res.set_content("{\"created\":true}", "application/json");
    });
    
    EXPECT_TRUE(server->listen("127.0.0.1", 9997));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    try {
        net::io_context ioc;
        tcp::resolver resolver{ioc};
        auto const results = resolver.resolve("127.0.0.1", "9997");
        
        tcp::socket socket{ioc};
        net::connect(socket, results.begin(), results.end());
        
        beast::http::request<beast::http::string_body> req{beast::http::verb::post, "/data", 11};
        req.set(beast::http::field::host, "127.0.0.1");
        req.set(beast::http::field::content_type, "application/json");
        req.body() = "{\"test\":\"data\"}";
        req.prepare_payload();
        beast::http::write(socket, req);
        
        beast::flat_buffer buffer;
        beast::http::response<beast::http::string_body> res;
        beast::http::read(socket, buffer, res);
        
        EXPECT_EQ(res.result_int(), 201);
        EXPECT_EQ(received_body, "{\"test\":\"data\"}");
        
    } catch (std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
    
    server->stop();
}

TEST(BeastServerTest, Returns404ForUnmatchedRoute) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    server->register_get("/exists", [](const request_data& req, response_builder& res) {
        res.set_status(200);
        res.set_content("OK", "text/plain");
    });
    
    EXPECT_TRUE(server->listen("127.0.0.1", 9996));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    try {
        net::io_context ioc;
        tcp::resolver resolver{ioc};
        auto const results = resolver.resolve("127.0.0.1", "9996");
        
        tcp::socket socket{ioc};
        net::connect(socket, results.begin(), results.end());
        
        beast::http::request<beast::http::string_body> req{beast::http::verb::get, "/notfound", 11};
        req.set(beast::http::field::host, "127.0.0.1");
        beast::http::write(socket, req);
        
        beast::flat_buffer buffer;
        beast::http::response<beast::http::string_body> res;
        beast::http::read(socket, buffer, res);
        
        EXPECT_EQ(res.result_int(), 404);
        
    } catch (std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
    
    server->stop();
}

TEST(BeastServerTest, SSEStreaming) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    server->register_get("/sse", [](const request_data& req, response_builder& res) {
        EXPECT_EQ(req.method, "GET");
        EXPECT_EQ(req.path, "/sse");
        
        res.set_chunked_content_provider("text/event-stream", 
            [](size_t offset, streaming_data_sink& sink) -> bool {
                if (offset == 0) {
                    std::string event = "event: test\ndata: Message 1\n\n";
                    return sink.write(event.c_str(), event.size());
                } else if (offset == 1) {
                    std::string event = "event: test\ndata: Message 2\n\n";
                    return sink.write(event.c_str(), event.size());
                } else {
                    // End of stream
                    return false;
                }
            });
    });
    
    EXPECT_TRUE(server->listen("127.0.0.1", 9995));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    try {
        net::io_context ioc;
        tcp::resolver resolver{ioc};
        auto const results = resolver.resolve("127.0.0.1", "9995");
        
        tcp::socket socket{ioc};
        net::connect(socket, results.begin(), results.end());
        
        beast::http::request<beast::http::string_body> req{beast::http::verb::get, "/sse", 11};
        req.set(beast::http::field::host, "127.0.0.1");
        beast::http::write(socket, req);
        
        // Read response headers
        beast::flat_buffer buffer;
        beast::http::response_parser<beast::http::string_body> parser;
        parser.body_limit(std::numeric_limits<std::uint64_t>::max());
        beast::http::read_header(socket, buffer, parser);
        
        auto& res = parser.get();
        EXPECT_EQ(res.result(), beast::http::status::ok);
        EXPECT_EQ(res[beast::http::field::content_type], "text/event-stream");
        EXPECT_TRUE(res.chunked());
        
        // Read chunks
        std::vector<std::string> events;
        boost::system::error_code ec;
        
        while (!ec && events.size() < 2) {
            // Read chunk size line
            std::array<char, 1> byte;
            std::string line;
            while (true) {
                size_t n = socket.read_some(net::buffer(byte), ec);
                if (ec || n == 0) break;
                line += byte[0];
                if (line.size() >= 2 && line.substr(line.size()-2) == "\r\n") {
                    break;
                }
            }
            
            if (ec) break;
            
            // Parse chunk size
            std::string size_hex = line.substr(0, line.size()-2);
            size_t chunk_size = std::stoull(size_hex, nullptr, 16);
            
            if (chunk_size == 0) break; // Final chunk
            
            // Read chunk data
            std::vector<char> chunk_data(chunk_size);
            net::read(socket, net::buffer(chunk_data), ec);
            if (ec) break;
            
            events.push_back(std::string(chunk_data.begin(), chunk_data.end()));
            
            // Read trailing \r\n
            net::read(socket, net::buffer(byte), ec);
            net::read(socket, net::buffer(byte), ec);
        }
        
        EXPECT_EQ(events.size(), 2);
        EXPECT_TRUE(events[0].find("Message 1") != std::string::npos);
        EXPECT_TRUE(events[1].find("Message 2") != std::string::npos);
        
    } catch (std::exception& e) {
        FAIL() << "Exception: " << e.what();
    }
    
    server->stop();
}

/**
 * Test beast_client wrapper
 */
TEST(BeastClientTest, GetRequest) {
    // Start a simple server
    auto server = std::make_unique<beast_server>(false, "", "");
    server->register_get("/test", [](const request_data& req, response_builder& res) {
        res.set_status(200);
        res.set_header("X-Test-Header", "test-value");
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    EXPECT_TRUE(server->listen("127.0.0.1", 9994));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Test client
    beast_client client("http://127.0.0.1:9994");
    auto result = client.get("/test");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.body, "{\"status\":\"ok\"}");
    
    auto header_it = result.headers.find("X-Test-Header");
    EXPECT_NE(header_it, result.headers.end());
    if (header_it != result.headers.end()) {
        EXPECT_EQ(header_it->second, "test-value");
    }
    
    server->stop();
}

TEST(BeastClientTest, PostRequest) {
    // Start a simple server
    auto server = std::make_unique<beast_server>(false, "", "");
    std::string received_body;
    server->register_post("/submit", [&](const request_data& req, response_builder& res) {
        received_body = req.body;
        res.set_status(201);
        res.set_content("{\"created\":true}", "application/json");
    });
    EXPECT_TRUE(server->listen("127.0.0.1", 9993));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Test client
    beast_client client("http://127.0.0.1:9993");
    headers_map custom_headers;
    custom_headers.emplace("X-Custom", "value");
    
    auto result = client.post("/submit", custom_headers, "{\"data\":\"test\"}", "application/json");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status_code, 201);
    EXPECT_EQ(received_body, "{\"data\":\"test\"}");
    
    server->stop();
}

TEST(BeastClientTest, GetStreamRequest) {
    // Start a server with SSE streaming
    auto server = std::make_unique<beast_server>(false, "", "");
    server->register_get("/stream", [](const request_data& req, response_builder& res) {
        res.set_chunked_content_provider("text/event-stream", 
            [](size_t offset, streaming_data_sink& sink) -> bool {
                if (offset < 3) {
                    std::string event = "data: Event " + std::to_string(offset) + "\n\n";
                    return sink.write(event.c_str(), event.size());
                }
                return false;
            });
    });
    EXPECT_TRUE(server->listen("127.0.0.1", 9992));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Test client streaming
    beast_client client("http://127.0.0.1:9992");
    std::vector<std::string> received_chunks;
    
    auto result = client.get_stream("/stream", 
        [&](const char* data, size_t size) -> bool {
            received_chunks.push_back(std::string(data, size));
            return true; // Continue streaming
        });
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(received_chunks.size(), 3);
    
    for (size_t i = 0; i < received_chunks.size(); i++) {
        EXPECT_TRUE(received_chunks[i].find("Event " + std::to_string(i)) != std::string::npos);
    }
    
    server->stop();
}

TEST(BeastClientTest, ConnectionFailure) {
    beast_client client("http://127.0.0.1:9876"); // Non-existent server
    auto result = client.get("/test");
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

/**
 * Integration test: Beast client + server
 */
TEST(BeastIntegrationTest, ClientServerCommunication) {
    // Create server
    auto server = std::make_unique<beast_server>(false, "", "");
    
    server->register_get("/health", [](const request_data& req, response_builder& res) {
        res.set_status(200);
        res.set_content("{\"status\":\"healthy\"}", "application/json");
    });
    
    server->register_post("/echo", [](const request_data& req, response_builder& res) {
        res.set_status(200);
        res.set_content(req.body, "application/json");
    });
    
    EXPECT_TRUE(server->listen("127.0.0.1", 9991));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Create client
    beast_client client("http://127.0.0.1:9991");
    
    // Test GET
    auto get_result = client.get("/health");
    EXPECT_TRUE(get_result.success);
    EXPECT_EQ(get_result.status_code, 200);
    EXPECT_EQ(get_result.body, "{\"status\":\"healthy\"}");
    
    // Test POST
    headers_map headers;
    auto post_result = client.post("/echo", headers, "{\"test\":\"data\"}", "application/json");
    EXPECT_TRUE(post_result.success);
    EXPECT_EQ(post_result.status_code, 200);
    EXPECT_EQ(post_result.body, "{\"test\":\"data\"}");
    
    server->stop();
}
