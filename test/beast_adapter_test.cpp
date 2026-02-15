/**
 * @file beast_adapter_test.cpp
 * @brief Tests for Boost.Beast HTTP adapter implementation
 * 
 * Tests the beast adapter implementation that uses boost::beast to match
 * the HTTP abstraction interfaces. 
 * 
 * Following TDD: Write tests first, then implement to pass tests.
 */

#include <boost/test/unit_test.hpp>
#include "mcp_http_beast_adapter.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <sstream>

using namespace mcp::http;
using namespace mcp::http::beast_adapter;
namespace net = boost::asio;
using tcp = net::ip::tcp;

BOOST_AUTO_TEST_SUITE(BeastDataSinkTest)

/**
 * Test beast_data_sink wrapper
 * 
 * This test validates that beast_data_sink correctly wraps Beast's
 * write operations to match our streaming_data_sink interface.
 */
BOOST_AUTO_TEST_CASE(WritesChunksCorrectly) {
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
    
    BOOST_CHECK(write_success);
    
    // Verify: Read from client socket and check chunked format
    std::array<char, 256> buffer;
    boost::system::error_code ec;
    size_t bytes_read = client_socket.read_some(net::buffer(buffer), ec);
    
    BOOST_CHECK(!ec);
    BOOST_CHECK_GT(bytes_read, 0);
    
    // Parse chunk format: <hex-size>\r\n<data>\r\n
    std::string received(buffer.data(), bytes_read);
    
    // Should contain hex size
    std::stringstream expected;
    expected << std::hex << test_data.size() << "\r\n" << test_data << "\r\n";
    
    BOOST_CHECK_EQUAL(received, expected.str());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(BeastResponseBuilderTest)

/**
 * Test beast_response_builder wrapper
 */
BOOST_AUTO_TEST_CASE(SetStatus) {
    boost::beast::http::response<boost::beast::http::string_body> res;
    beast_response_builder builder(res);
    
    builder.set_status(200);
    BOOST_CHECK_EQUAL(res.result_int(), 200);
    
    builder.set_status(404);
    BOOST_CHECK_EQUAL(res.result_int(), 404);
    
    builder.set_status(500);
    BOOST_CHECK_EQUAL(res.result_int(), 500);
}

BOOST_AUTO_TEST_CASE(SetHeader) {
    boost::beast::http::response<boost::beast::http::string_body> res;
    beast_response_builder builder(res);
    
    builder.set_header("Content-Type", "application/json");
    BOOST_CHECK_EQUAL(res["Content-Type"], "application/json");
    
    builder.set_header("X-Custom-Header", "test-value");
    BOOST_CHECK_EQUAL(res["X-Custom-Header"], "test-value");
}

BOOST_AUTO_TEST_CASE(SetContent) {
    boost::beast::http::response<boost::beast::http::string_body> res;
    beast_response_builder builder(res);
    
    std::string body = "{\"status\":\"ok\"}";
    builder.set_content(body, "application/json");
    
    BOOST_CHECK_EQUAL(res.body(), body);
    BOOST_CHECK_EQUAL(res["Content-Type"], "application/json");
}

BOOST_AUTO_TEST_CASE(DISABLED_SetChunkedContentProvider) {
    // TODO Phase 2: Implement test
    // Should test:
    // - Setting up chunked transfer encoding
    // - Streaming data via callback
    // - Proper chunk formatting (hex size + CRLF + data + CRLF)
    // - Final chunk (0\r\n\r\n)
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(BeastServerTest)

/**
 * Test beast_server wrapper
 */
BOOST_AUTO_TEST_CASE(RegisterGetHandler) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    bool handler_called = false;
    server->register_get("/test", [&](const request_data& req, response_builder& res) {
        handler_called = true;
        BOOST_CHECK_EQUAL(req.method, "GET");
        BOOST_CHECK_EQUAL(req.path, "/test");
        res.set_status(200);
        res.set_content("{\"message\":\"success\"}", "application/json");
    });
    
    BOOST_CHECK(server->listen("127.0.0.1", 9998));
    
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
        
        BOOST_CHECK_EQUAL(res.result_int(), 200);
        BOOST_CHECK_EQUAL(res.body(), "{\"message\":\"success\"}");
        BOOST_CHECK(handler_called);
        
    } catch (std::exception& e) {
        BOOST_FAIL("Exception: " + std::string(e.what()));
    }
    
    server->stop();
}

BOOST_AUTO_TEST_CASE(RegisterPostHandler) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    std::string received_body;
    server->register_post("/data", [&](const request_data& req, response_builder& res) {
        BOOST_CHECK_EQUAL(req.method, "POST");
        BOOST_CHECK_EQUAL(req.path, "/data");
        received_body = req.body;
        res.set_status(201);
        res.set_content("{\"created\":true}", "application/json");
    });
    
    BOOST_CHECK(server->listen("127.0.0.1", 9997));
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
        
        BOOST_CHECK_EQUAL(res.result_int(), 201);
        BOOST_CHECK_EQUAL(received_body, "{\"test\":\"data\"}");
        
    } catch (std::exception& e) {
        BOOST_FAIL("Exception: " + std::string(e.what()));
    }
    
    server->stop();
}

BOOST_AUTO_TEST_CASE(Returns404ForUnmatchedRoute) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    server->register_get("/exists", [](const request_data& req, response_builder& res) {
        res.set_status(200);
        res.set_content("OK", "text/plain");
    });
    
    BOOST_CHECK(server->listen("127.0.0.1", 9996));
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
        
        BOOST_CHECK_EQUAL(res.result_int(), 404);
        
    } catch (std::exception& e) {
        BOOST_FAIL("Exception: " + std::string(e.what()));
    }
    
    server->stop();
}

BOOST_AUTO_TEST_CASE(SSEStreaming) {
    auto server = std::make_unique<beast_server>(false, "", "");
    
    server->register_get("/sse", [](const request_data& req, response_builder& res) {
        BOOST_CHECK_EQUAL(req.method, "GET");
        BOOST_CHECK_EQUAL(req.path, "/sse");
        
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
    
    BOOST_CHECK(server->listen("127.0.0.1", 9995));
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
        BOOST_CHECK_EQUAL(res.result(), beast::http::status::ok);
        BOOST_CHECK_EQUAL(res[beast::http::field::content_type], "text/event-stream");
        BOOST_CHECK(res.chunked());
        
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
        
        BOOST_CHECK_EQUAL(events.size(), 2);
        BOOST_CHECK(events[0].find("Message 1") != std::string::npos);
        BOOST_CHECK(events[1].find("Message 2") != std::string::npos);
        
    } catch (std::exception& e) {
        BOOST_FAIL("Exception: " + std::string(e.what()));
    }
    
    server->stop();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(BeastClientTest)

/**
 * Test beast_client wrapper
 */
BOOST_AUTO_TEST_CASE(GetRequest) {
    // Start a simple server
    auto server = std::make_unique<beast_server>(false, "", "");
    server->register_get("/test", [](const request_data& req, response_builder& res) {
        res.set_status(200);
        res.set_header("X-Test-Header", "test-value");
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    BOOST_CHECK(server->listen("127.0.0.1", 9994));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Test client
    beast_client client("http://127.0.0.1:9994");
    auto result = client.get("/test");
    
    BOOST_CHECK(result.success);
    BOOST_CHECK_EQUAL(result.status_code, 200);
    BOOST_CHECK_EQUAL(result.body, "{\"status\":\"ok\"}");
    
    auto header_it = result.headers.find("X-Test-Header");
    BOOST_CHECK_NE(header_it, result.headers.end());
    if (header_it != result.headers.end()) {
        BOOST_CHECK_EQUAL(header_it->second, "test-value");
    }
    
    server->stop();
}

BOOST_AUTO_TEST_CASE(PostRequest) {
    // Start a simple server
    auto server = std::make_unique<beast_server>(false, "", "");
    std::string received_body;
    server->register_post("/submit", [&](const request_data& req, response_builder& res) {
        received_body = req.body;
        res.set_status(201);
        res.set_content("{\"created\":true}", "application/json");
    });
    BOOST_CHECK(server->listen("127.0.0.1", 9993));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Test client
    beast_client client("http://127.0.0.1:9993");
    headers_map custom_headers;
    custom_headers.emplace("X-Custom", "value");
    
    auto result = client.post("/submit", custom_headers, "{\"data\":\"test\"}", "application/json");
    
    BOOST_CHECK(result.success);
    BOOST_CHECK_EQUAL(result.status_code, 201);
    BOOST_CHECK_EQUAL(received_body, "{\"data\":\"test\"}");
    
    server->stop();
}

BOOST_AUTO_TEST_CASE(GetStreamRequest) {
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
    BOOST_CHECK(server->listen("127.0.0.1", 9992));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Test client streaming
    beast_client client("http://127.0.0.1:9992");
    std::vector<std::string> received_chunks;
    
    auto result = client.get_stream("/stream", 
        [&](const char* data, size_t size) -> bool {
            received_chunks.push_back(std::string(data, size));
            return true; // Continue streaming
        });
    
    BOOST_CHECK(result.success);
    BOOST_CHECK_EQUAL(result.status_code, 200);
    BOOST_CHECK_EQUAL(received_chunks.size(), 3);
    
    for (size_t i = 0; i < received_chunks.size(); i++) {
        BOOST_CHECK(received_chunks[i].find("Event " + std::to_string(i)) != std::string::npos);
    }
    
    server->stop();
}

BOOST_AUTO_TEST_CASE(ConnectionFailure) {
    beast_client client("http://127.0.0.1:9876"); // Non-existent server
    auto result = client.get("/test");
    
    BOOST_CHECK(!result.success);
    BOOST_CHECK(!result.error_message.empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(BeastIntegrationTest)

/**
 * Integration test: Beast client + server
 */
BOOST_AUTO_TEST_CASE(ClientServerCommunication) {
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
    
    BOOST_CHECK(server->listen("127.0.0.1", 9991));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Create client
    beast_client client("http://127.0.0.1:9991");
    
    // Test GET
    auto get_result = client.get("/health");
    BOOST_CHECK(get_result.success);
    BOOST_CHECK_EQUAL(get_result.status_code, 200);
    BOOST_CHECK_EQUAL(get_result.body, "{\"status\":\"healthy\"}");
    
    // Test POST
    headers_map headers;
    auto post_result = client.post("/echo", headers, "{\"test\":\"data\"}", "application/json");
    BOOST_CHECK(post_result.success);
    BOOST_CHECK_EQUAL(post_result.status_code, 200);
    BOOST_CHECK_EQUAL(post_result.body, "{\"test\":\"data\"}");
    
    server->stop();
}

BOOST_AUTO_TEST_SUITE_END()
