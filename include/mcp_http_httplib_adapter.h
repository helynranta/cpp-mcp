/**
 * @file mcp_http_httplib_adapter.h
 * @brief httplib adapter for HTTP abstraction layer
 * 
 * This file provides implementations of the HTTP abstractions using cpp-httplib.
 * This adapter wraps existing httplib code to match the abstraction interfaces.
 * 
 * This is Phase 1 of the httplib → boost::beast migration.
 * See MIGRATION_PLAN.md for details.
 * 
 * NOTE: This adapter will be REMOVED after migration to boost::beast is complete.
 */

#ifndef MCP_HTTP_HTTPLIB_ADAPTER_H
#define MCP_HTTP_HTTPLIB_ADAPTER_H

#include "mcp_http_abstraction.h"
#include "httplib.h"
#include <memory>

namespace mcp {
namespace http {
namespace httplib_adapter {

/**
 * @brief httplib implementation of streaming_data_sink
 * 
 * Wraps httplib::DataSink to match our abstraction.
 */
class httplib_data_sink : public streaming_data_sink {
public:
    explicit httplib_data_sink(httplib::DataSink& sink) : sink_(sink) {}
    
    bool write(const char* data, size_t size) override {
        return sink_.write(data, size);
    }
    
private:
    httplib::DataSink& sink_;
};

/**
 * @brief httplib implementation of response_builder
 * 
 * Wraps httplib::Response to match our abstraction.
 */
class httplib_response_builder : public response_builder {
public:
    explicit httplib_response_builder(httplib::Response& response) 
        : response_(response) {}
    
    void set_status(int code) override {
        response_.status = code;
    }
    
    void set_header(const std::string& name, const std::string& value) override {
        response_.set_header(name, value);
    }
    
    void set_content(const std::string& body, const std::string& content_type) override {
        response_.set_content(body, content_type);
    }
    
    void set_chunked_content_provider(
        const std::string& content_type,
        std::function<bool(size_t offset, streaming_data_sink& sink)> provider
    ) override {
        // Wrap our abstraction callback to work with httplib::DataSink
        response_.set_chunked_content_provider(
            content_type,
            [provider = std::move(provider)](size_t offset, httplib::DataSink& httplib_sink) -> bool {
                httplib_data_sink wrapped_sink(httplib_sink);
                return provider(offset, wrapped_sink);
            }
        );
    }
    
private:
    httplib::Response& response_;
};

/**
 * @brief httplib implementation of server_interface
 * 
 * Wraps httplib::Server to match our abstraction.
 */
class httplib_server : public server_interface {
public:
    httplib_server(bool use_ssl, const std::string& cert_path, const std::string& key_path) {
#ifdef MCP_SSL
        if (use_ssl) {
            server_ = std::make_unique<httplib::SSLServer>(cert_path.c_str(), key_path.c_str());
        } else {
            server_ = std::make_unique<httplib::Server>();
        }
#else
        server_ = std::make_unique<httplib::Server>();
#endif
    }
    
    void register_get(const std::string& pattern, request_handler handler) override {
        server_->Get(pattern, [handler](const httplib::Request& req, httplib::Response& res) {
            handle_request(req, res, handler);
        });
    }
    
    void register_post(const std::string& pattern, request_handler handler) override {
        server_->Post(pattern, [handler](const httplib::Request& req, httplib::Response& res) {
            handle_request(req, res, handler);
        });
    }
    
    void register_delete(const std::string& pattern, request_handler handler) override {
        server_->Delete(pattern, [handler](const httplib::Request& req, httplib::Response& res) {
            handle_request(req, res, handler);
        });
    }
    
    void register_options(const std::string& pattern, request_handler handler) override {
        server_->Options(pattern, [handler](const httplib::Request& req, httplib::Response& res) {
            handle_request(req, res, handler);
        });
    }
    
    bool set_mount_point(const std::string& mount_point, 
                        const std::string& dir,
                        const headers_map& headers) override {
        // Convert headers_map to httplib::Headers
        httplib::Headers httplib_headers;
        for (const auto& [name, value] : headers) {
            httplib_headers.emplace(name, value);
        }
        return server_->set_mount_point(mount_point, dir, httplib_headers);
    }
    
    bool listen(const std::string& host, int port) override {
        return server_->listen(host, port);
    }
    
    void stop() override {
        server_->stop();
    }
    
private:
    static void handle_request(const httplib::Request& req, httplib::Response& res, 
                              const request_handler& handler) {
        // Convert httplib::Request to request_data
        request_data req_data;
        req_data.method = req.method;
        req_data.path = req.path;
        req_data.body = req.body;
        req_data.remote_addr = req.remote_addr;
        req_data.remote_port = req.remote_port;
        
        // Convert headers
        for (const auto& [name, value] : req.headers) {
            req_data.headers.emplace(name, value);
        }
        
        // Create response builder
        httplib_response_builder res_builder(res);
        
        // Call handler
        handler(req_data, res_builder);
    }
    
    std::unique_ptr<httplib::Server> server_;
};

/**
 * @brief httplib implementation of client_interface
 * 
 * Wraps httplib::Client to match our abstraction.
 */
class httplib_client : public client_interface {
public:
    explicit httplib_client(const std::string& scheme_host_port)
        : client_(std::make_unique<httplib::Client>(scheme_host_port)) {}
    
    client_result get(const std::string& path) override {
        auto result = client_->Get(path);
        return convert_result(result);
    }
    
    client_result get_stream(const std::string& path, streaming_callback callback) override {
        auto result = client_->Get(path, 
            [callback](const char* data, size_t size) -> bool {
                return callback(data, size);
            }
        );
        return convert_result(result);
    }
    
    client_result post(const std::string& path,
                      const headers_map& headers,
                      const std::string& body,
                      const std::string& content_type) override {
        // Convert headers
        httplib::Headers httplib_headers;
        for (const auto& [name, value] : headers) {
            httplib_headers.emplace(name, value);
        }
        
        auto result = client_->Post(path, httplib_headers, body, content_type);
        return convert_result(result);
    }
    
    void set_connection_timeout(int seconds) override {
        client_->set_connection_timeout(seconds, 0);
    }
    
    void set_read_timeout(int seconds) override {
        client_->set_read_timeout(seconds, 0);
    }
    
    void set_write_timeout(int seconds) override {
        client_->set_write_timeout(seconds, 0);
    }
    
    void set_default_headers(const headers_map& headers) override {
        httplib::Headers httplib_headers;
        for (const auto& [name, value] : headers) {
            httplib_headers.emplace(name, value);
        }
        client_->set_default_headers(httplib_headers);
    }
    
    void set_certificate_verification(bool enable) override {
#ifdef MCP_SSL
        client_->enable_server_certificate_verification(enable);
#endif
    }
    
    void set_ca_cert_path(const std::string& path) override {
#ifdef MCP_SSL
        client_->set_ca_cert_path(path);
#endif
    }
    
    void stop() override {
        client_->stop();
    }
    
private:
    static client_result convert_result(const httplib::Result& result) {
        client_result cr;
        if (result) {
            cr.success = true;
            cr.status_code = result->status;
            cr.body = result->body;
            
            // Convert headers
            for (const auto& [name, value] : result->headers) {
                cr.headers.emplace(name, value);
            }
        } else {
            cr.success = false;
            cr.error_message = httplib::to_string(result.error());
        }
        return cr;
    }
    
    std::unique_ptr<httplib::Client> client_;
};

} // namespace httplib_adapter

// Factory implementations (temporary - using httplib adapter)
inline std::unique_ptr<server_interface> create_server(
    bool use_ssl,
    const std::string& cert_path,
    const std::string& key_path
) {
    return std::make_unique<httplib_adapter::httplib_server>(use_ssl, cert_path, key_path);
}

inline std::unique_ptr<client_interface> create_client(const std::string& scheme_host_port) {
    return std::make_unique<httplib_adapter::httplib_client>(scheme_host_port);
}

} // namespace http
} // namespace mcp

#endif // MCP_HTTP_HTTPLIB_ADAPTER_H
