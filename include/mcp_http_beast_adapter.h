/**
 * @file mcp_http_beast_adapter.h
 * @brief Boost.Beast adapter for HTTP abstraction layer
 * 
 * This file provides implementations of the HTTP abstractions using Boost.Beast.
 * This adapter will REPLACE the httplib adapter after migration is complete.
 * 
 * This is Phase 2 of the httplib → boost::beast migration.
 * See MIGRATION_PLAN.md for details.
 * 
 * STATUS: NOT YET IMPLEMENTED
 * TODO: Implement all interfaces using Boost.Beast
 */

#ifndef MCP_HTTP_BEAST_ADAPTER_H
#define MCP_HTTP_BEAST_ADAPTER_H

#include "mcp_http_abstraction.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <sstream>
#include <thread>
#include <atomic>
#include <map>

// Note: We use beast::http to avoid namespace conflicts with mcp::http
namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace mcp {
namespace http {
namespace beast_adapter {

/**
 * @brief Boost.Beast implementation of streaming_data_sink
 * 
 * Wraps a Beast TCP socket and writes chunked-encoded data for SSE streaming.
 */
class beast_data_sink : public streaming_data_sink {
public:
    explicit beast_data_sink(tcp::socket& socket) : socket_(socket) {}
    
    bool write(const char* data, size_t size) override {
        try {
            // Format: <size in hex>\r\n<data>\r\n
            std::ostringstream chunk;
            chunk << std::hex << size << "\r\n";
            chunk.write(data, size);
            chunk << "\r\n";
            
            std::string chunk_str = chunk.str();
            boost::system::error_code ec;
            net::write(socket_, net::buffer(chunk_str), ec);
            
            return !ec;
        } catch (...) {
            return false;
        }
    }
    
private:
    tcp::socket& socket_;
};

/**
 * @brief Boost.Beast implementation of response_builder
 * 
 * Wraps a Beast HTTP response to match our response_builder interface.
 */
class beast_response_builder : public response_builder {
public:
    explicit beast_response_builder(beast::http::response<beast::http::string_body>& response) 
        : response_(response) {}
    
    void set_status(int code) override {
        response_.result(static_cast<beast::http::status>(code));
    }
    
    void set_header(const std::string& name, const std::string& value) override {
        response_.set(name, value);
    }
    
    void set_content(const std::string& body, const std::string& content_type) override {
        response_.body() = body;
        response_.set(beast::http::field::content_type, content_type);
        response_.prepare_payload();
    }
    
    void set_chunked_content_provider(
        const std::string& content_type,
        std::function<bool(size_t offset, streaming_data_sink& sink)> provider
    ) override {
        // This is handled by the server during connection handling
        // Store the provider for later use
        chunked_provider_ = provider;
        chunked_content_type_ = content_type;
    }
    
    // Accessors for server to use
    bool has_chunked_provider() const { return chunked_provider_ != nullptr; }
    const std::function<bool(size_t, streaming_data_sink&)>& get_chunked_provider() const {
        return chunked_provider_;
    }
    const std::string& get_chunked_content_type() const { return chunked_content_type_; }
    
private:
    beast::http::response<beast::http::string_body>& response_;
    std::function<bool(size_t, streaming_data_sink&)> chunked_provider_;
    std::string chunked_content_type_;
};

/**
 * @brief Boost.Beast implementation of server_interface
 * 
 * Implements HTTP server using Boost.Beast with async I/O
 */
class beast_server : public server_interface {
public:
    beast_server(bool use_ssl, const std::string& cert_path, const std::string& key_path) 
        : use_ssl_(use_ssl)
        , cert_path_(cert_path)
        , key_path_(key_path)
        , running_(false)
    {
        // SSL support will be added later
        if (use_ssl) {
            throw std::runtime_error("SSL/TLS support not yet implemented for beast_server. Use create_httplib_server() for SSL support or disable SSL for now.");
        }
    }
    
    ~beast_server() {
        stop();
    }
    
    void register_get(const std::string& pattern, request_handler handler) override {
        routes_["GET:" + pattern] = handler;
    }
    
    void register_post(const std::string& pattern, request_handler handler) override {
        routes_["POST:" + pattern] = handler;
    }
    
    void register_delete(const std::string& pattern, request_handler handler) override {
        routes_["DELETE:" + pattern] = handler;
    }
    
    void register_options(const std::string& pattern, request_handler handler) override {
        routes_["OPTIONS:" + pattern] = handler;
    }
    
    bool set_mount_point(const std::string& mount_point, 
                        const std::string& dir,
                        const headers_map& headers) override {
        // Static file serving not implemented yet
        return false;
    }
    
    bool listen(const std::string& host, int port) override {
        try {
            running_ = true;
            server_thread_ = std::thread([this, host, port]() {
                this->run_server(host, port);
            });
            
            // Give server time to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return true;
        } catch (...) {
            return false;
        }
    }
    
    void stop() override {
        running_ = false;
        if (io_context_) {
            io_context_->stop();
        }
        // Give active connections time to finish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
private:
    void run_server(const std::string& host, int port) {
        try {
            io_context_ = std::make_unique<net::io_context>(1);
            tcp::acceptor acceptor{*io_context_, 
                {net::ip::make_address(host), static_cast<unsigned short>(port)}};
            
            acceptor.non_blocking(true);
            
            while (running_) {
                tcp::socket socket{*io_context_};
                boost::system::error_code ec;
                acceptor.accept(socket, ec);
                
                if (!ec) {
                    // Handle connection in a new thread
                    std::thread([this, socket = std::move(socket)]() mutable {
                        this->handle_connection(std::move(socket));
                    }).detach();
                } else if (ec == boost::asio::error::would_block) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        } catch (...) {
            // Server error
        }
    }
    
    void handle_connection(tcp::socket socket) {
        try {
            beast::flat_buffer buffer;
            beast::http::request<beast::http::string_body> req;
            
            // Read request
            beast::http::read(socket, buffer, req);
            
            // Convert to request_data
            request_data req_data;
            req_data.method = std::string(req.method_string());
            std::string full_target = std::string(req.target());
            
            // Parse path and query parameters
            size_t query_pos = full_target.find('?');
            if (query_pos != std::string::npos) {
                req_data.path = full_target.substr(0, query_pos);
                std::string query_string = full_target.substr(query_pos + 1);
                
                // Parse query parameters
                size_t start = 0;
                while (start < query_string.length()) {
                    size_t amp_pos = query_string.find('&', start);
                    size_t end = (amp_pos != std::string::npos) ? amp_pos : query_string.length();
                    std::string param = query_string.substr(start, end - start);
                    
                    size_t eq_pos = param.find('=');
                    if (eq_pos != std::string::npos) {
                        std::string key = param.substr(0, eq_pos);
                        std::string value = param.substr(eq_pos + 1);
                        req_data.params[key] = value;
                    }
                    
                    start = (amp_pos != std::string::npos) ? amp_pos + 1 : query_string.length();
                }
            } else {
                req_data.path = full_target;
            }
            
            req_data.body = req.body();
            
            // Copy headers
            for (auto const& field : req) {
                req_data.headers.emplace(std::string(field.name_string()), 
                                        std::string(field.value()));
            }
            
            // Find matching route
            std::string route_key = req_data.method + ":" + req_data.path;
            auto it = routes_.find(route_key);
            
            if (it != routes_.end()) {
                // Create response
                beast::http::response<beast::http::string_body> res{
                    beast::http::status::ok, req.version()};
                beast_response_builder builder(res);
                
                // Call handler
                it->second(req_data, builder);
                
                // Check if we need to do chunked streaming
                if (builder.has_chunked_provider()) {
                    handle_chunked_response(socket, req, builder);
                } else {
                    // Send regular response
                    res.prepare_payload();
                    beast::http::write(socket, res);
                }
            } else {
                // 404 Not Found
                beast::http::response<beast::http::string_body> res{
                    beast::http::status::not_found, req.version()};
                res.set(beast::http::field::content_type, "text/plain");
                res.body() = "Not Found";
                res.prepare_payload();
                beast::http::write(socket, res);
            }
            
        } catch (...) {
            // Connection error
        }
    }
    
    void handle_chunked_response(tcp::socket& socket, 
                                 const beast::http::request<beast::http::string_body>& req,
                                 beast_response_builder& builder) {
        try {
            // Send chunked response headers
            beast::http::response<beast::http::empty_body> res{
                beast::http::status::ok, req.version()};
            res.set(beast::http::field::content_type, builder.get_chunked_content_type());
            res.set(beast::http::field::cache_control, "no-cache");
            res.set(beast::http::field::connection, "keep-alive");
            res.chunked(true);
            
            // Write headers
            beast::http::response_serializer<beast::http::empty_body> sr{res};
            beast::http::write_header(socket, sr);
            
            // Create data sink and call provider
            beast_data_sink sink(socket);
            size_t offset = 0;
            while (builder.get_chunked_provider()(offset, sink)) {
                offset++;
            }
            
            // Send final chunk
            boost::system::error_code ec;
            net::write(socket, net::buffer("0\r\n\r\n"), ec);
            
        } catch (...) {
            // Streaming error
        }
    }
    
    bool use_ssl_;
    std::string cert_path_;
    std::string key_path_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::unique_ptr<net::io_context> io_context_;
    std::map<std::string, request_handler> routes_;
};

/**
 * @brief Boost.Beast implementation of client_interface
 * 
 * Implements HTTP client using Boost.Beast
 */
class beast_client : public client_interface {
public:
    explicit beast_client(const std::string& scheme_host_port) {
        // Parse URL: http://host:port or https://host:port
        size_t scheme_end = scheme_host_port.find("://");
        if (scheme_end != std::string::npos) {
            scheme_ = scheme_host_port.substr(0, scheme_end);
            std::string host_port = scheme_host_port.substr(scheme_end + 3);
            
            size_t port_pos = host_port.find(':');
            if (port_pos != std::string::npos) {
                host_ = host_port.substr(0, port_pos);
                port_ = host_port.substr(port_pos + 1);
            } else {
                host_ = host_port;
                port_ = (scheme_ == "https") ? "443" : "80";
            }
        } else {
            // Assume http://host:port format without scheme
            size_t port_pos = scheme_host_port.find(':');
            if (port_pos != std::string::npos) {
                host_ = scheme_host_port.substr(0, port_pos);
                port_ = scheme_host_port.substr(port_pos + 1);
            } else {
                host_ = scheme_host_port;
                port_ = "80";
            }
            scheme_ = "http";
        }
        
        if (scheme_ == "https") {
            throw std::runtime_error("HTTPS support not yet implemented for beast_client. Use create_httplib_client() for HTTPS support or use HTTP for now.");
        }
    }
    
    client_result get(const std::string& path) override {
        try {
            net::io_context ioc;
            tcp::resolver resolver{ioc};
            auto const results = resolver.resolve(host_, port_);
            
            tcp::socket socket{ioc};
            net::connect(socket, results.begin(), results.end());
            
            beast::http::request<beast::http::string_body> req{
                beast::http::verb::get, path, 11};
            req.set(beast::http::field::host, host_);
            req.set(beast::http::field::user_agent, "beast-client");
            
            // Add default headers
            for (const auto& [name, value] : default_headers_) {
                req.set(name, value);
            }
            
            beast::http::write(socket, req);
            
            beast::flat_buffer buffer;
            beast::http::response<beast::http::string_body> res;
            beast::http::read(socket, buffer, res);
            
            client_result result;
            result.success = true;
            result.status_code = res.result_int();
            result.body = res.body();
            
            for (auto const& field : res) {
                result.headers.emplace(std::string(field.name_string()), 
                                      std::string(field.value()));
            }
            
            return result;
        } catch (std::exception& e) {
            client_result result;
            result.success = false;
            result.error_message = e.what();
            return result;
        }
    }
    
    client_result get_stream(const std::string& path, streaming_callback callback) override {
        try {
            net::io_context ioc;
            tcp::resolver resolver{ioc};
            auto const results = resolver.resolve(host_, port_);
            
            tcp::socket socket{ioc};
            net::connect(socket, results.begin(), results.end());
            
            beast::http::request<beast::http::string_body> req{
                beast::http::verb::get, path, 11};
            req.set(beast::http::field::host, host_);
            req.set(beast::http::field::user_agent, "beast-client");
            
            for (const auto& [name, value] : default_headers_) {
                req.set(name, value);
            }
            
            beast::http::write(socket, req);
            
            // Read response headers
            beast::flat_buffer buffer;
            beast::http::response_parser<beast::http::string_body> parser;
            parser.body_limit(std::numeric_limits<std::uint64_t>::max());
            beast::http::read_header(socket, buffer, parser);
            
            auto& res = parser.get();
            
            client_result result;
            result.success = true;
            result.status_code = res.result_int();
            
            for (auto const& field : res) {
                result.headers.emplace(std::string(field.name_string()), 
                                      std::string(field.value()));
            }
            
            // Stream chunks if chunked encoding
            if (res.chunked()) {
                boost::system::error_code ec;
                while (!ec) {
                    // Read chunk size
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
                    
                    std::string size_hex = line.substr(0, line.size()-2);
                    if (size_hex.empty()) continue;
                    
                    size_t chunk_size = std::stoull(size_hex, nullptr, 16);
                    if (chunk_size == 0) break; // Final chunk
                    
                    // Read chunk data
                    std::vector<char> chunk_data(chunk_size);
                    net::read(socket, net::buffer(chunk_data), ec);
                    if (ec) break;
                    
                    // Call callback
                    if (!callback(chunk_data.data(), chunk_size)) {
                        break; // Callback requested stop
                    }
                    
                    // Read trailing \r\n
                    net::read(socket, net::buffer(byte), ec);
                    net::read(socket, net::buffer(byte), ec);
                }
            }
            
            return result;
        } catch (std::exception& e) {
            client_result result;
            result.success = false;
            result.error_message = e.what();
            return result;
        }
    }
    
    client_result post(const std::string& path,
                      const headers_map& headers,
                      const std::string& body,
                      const std::string& content_type) override {
        try {
            net::io_context ioc;
            tcp::resolver resolver{ioc};
            auto const results = resolver.resolve(host_, port_);
            
            tcp::socket socket{ioc};
            net::connect(socket, results.begin(), results.end());
            
            beast::http::request<beast::http::string_body> req{
                beast::http::verb::post, path, 11};
            req.set(beast::http::field::host, host_);
            req.set(beast::http::field::user_agent, "beast-client");
            req.set(beast::http::field::content_type, content_type);
            
            // Add custom headers
            for (const auto& [name, value] : headers) {
                req.set(name, value);
            }
            
            // Add default headers
            for (const auto& [name, value] : default_headers_) {
                req.set(name, value);
            }
            
            req.body() = body;
            req.prepare_payload();
            
            beast::http::write(socket, req);
            
            beast::flat_buffer buffer;
            beast::http::response<beast::http::string_body> res;
            beast::http::read(socket, buffer, res);
            
            client_result result;
            result.success = true;
            result.status_code = res.result_int();
            result.body = res.body();
            
            for (auto const& field : res) {
                result.headers.emplace(std::string(field.name_string()), 
                                      std::string(field.value()));
            }
            
            return result;
        } catch (std::exception& e) {
            client_result result;
            result.success = false;
            result.error_message = e.what();
            return result;
        }
    }
    
    void set_connection_timeout(int seconds) override {
        connection_timeout_seconds_ = seconds;
        // TODO: Implement timeout using deadline timer
    }
    
    void set_read_timeout(int seconds) override {
        read_timeout_seconds_ = seconds;
        // TODO: Implement timeout using deadline timer
    }
    
    void set_write_timeout(int seconds) override {
        write_timeout_seconds_ = seconds;
        // TODO: Implement timeout using deadline timer
    }
    
    void set_default_headers(const headers_map& headers) override {
        default_headers_ = headers;
    }
    
    void set_certificate_verification(bool enable) override {
        verify_certificate_ = enable;
        // TODO: Configure SSL context verification mode
    }
    
    void set_ca_cert_path(const std::string& path) override {
        ca_cert_path_ = path;
        // TODO: Load CA certificate into SSL context
    }
    
    void stop() override {
        // No persistent connections to stop in current implementation
    }
    
private:
    std::string scheme_;
    std::string host_;
    std::string port_;
    headers_map default_headers_;
    int connection_timeout_seconds_ = 10;
    int read_timeout_seconds_ = 10;
    int write_timeout_seconds_ = 10;
    bool verify_certificate_ = true;
    std::string ca_cert_path_;
};

} // namespace beast_adapter

/**
 * @brief Factory functions using Beast adapter
 * 
 * These can be used to create server and client instances using Boost.Beast.
 */
inline std::unique_ptr<server_interface> create_beast_server(
    bool use_ssl = false,
    const std::string& cert_path = "",
    const std::string& key_path = ""
) {
    return std::make_unique<beast_adapter::beast_server>(use_ssl, cert_path, key_path);
}

inline std::unique_ptr<client_interface> create_beast_client(const std::string& scheme_host_port) {
    return std::make_unique<beast_adapter::beast_client>(scheme_host_port);
}

} // namespace http
} // namespace mcp

#endif // MCP_HTTP_BEAST_ADAPTER_H
