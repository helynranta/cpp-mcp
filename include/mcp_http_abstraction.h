/**
 * @file mcp_http_abstraction.h
 * @brief HTTP abstraction layer for MCP
 * 
 * This file provides library-agnostic HTTP interfaces that decouple MCP from
 * the underlying HTTP implementation (transitioning from httplib to boost::beast).
 * 
 * This is Phase 1 of the httplib → boost::beast migration.
 * See MIGRATION_PLAN.md for details.
 */

#ifndef MCP_HTTP_ABSTRACTION_H
#define MCP_HTTP_ABSTRACTION_H

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <optional>

namespace mcp {
namespace http {

/**
 * @brief HTTP headers container (multimap for duplicate keys)
 */
using headers_map = std::multimap<std::string, std::string>;

/**
 * @brief HTTP request data structure
 * 
 * Simple POD structure containing HTTP request information.
 * Decouples MCP code from httplib::Request or boost::beast::http::request.
 */
struct request_data {
    std::string method;        ///< HTTP method (GET, POST, DELETE, OPTIONS)
    std::string path;          ///< Request path/target
    std::string body;          ///< Request body
    std::string remote_addr;   ///< Client IP address
    int remote_port = 0;       ///< Client port
    headers_map headers;       ///< HTTP headers
    
    /**
     * @brief Get header value by name
     * @param name Header name (case-sensitive)
     * @return Header value if found, empty optional otherwise
     */
    std::optional<std::string> get_header(const std::string& name) const {
        auto it = headers.find(name);
        if (it != headers.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

/**
 * @brief Data sink for streaming responses (SSE)
 * 
 * Abstract interface for writing chunked data in streaming responses.
 * Replaces httplib::DataSink.
 */
class streaming_data_sink {
public:
    virtual ~streaming_data_sink() = default;
    
    /**
     * @brief Write data to the stream
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @return true if write succeeded, false if connection closed or error
     */
    virtual bool write(const char* data, size_t size) = 0;
};

/**
 * @brief HTTP response builder interface
 * 
 * Abstract interface for building HTTP responses.
 * Decouples MCP code from httplib::Response or boost::beast::http::response.
 */
class response_builder {
public:
    virtual ~response_builder() = default;
    
    /**
     * @brief Set HTTP status code
     * @param code HTTP status code (200, 404, 500, etc.)
     */
    virtual void set_status(int code) = 0;
    
    /**
     * @brief Set response header
     * @param name Header name
     * @param value Header value
     */
    virtual void set_header(const std::string& name, const std::string& value) = 0;
    
    /**
     * @brief Set response body with content type
     * @param body Response body content
     * @param content_type Content-Type header value
     */
    virtual void set_content(const std::string& body, const std::string& content_type) = 0;
    
    /**
     * @brief Set chunked content provider for streaming responses (SSE)
     * 
     * Used for Server-Sent Events and other streaming responses.
     * Replaces httplib::Response::set_chunked_content_provider.
     * 
     * @param content_type Content-Type header value
     * @param provider Callback that writes data to sink. Returns true to continue, false to end stream.
     */
    virtual void set_chunked_content_provider(
        const std::string& content_type,
        std::function<bool(size_t offset, streaming_data_sink& sink)> provider
    ) = 0;
};

/**
 * @brief HTTP request handler callback type
 * 
 * Handler receives request data and response builder.
 * Handler should configure the response using the builder.
 */
using request_handler = std::function<void(const request_data&, response_builder&)>;

/**
 * @brief HTTP server interface
 * 
 * Abstract interface for HTTP servers.
 * Can be implemented using httplib (current) or boost::beast (future).
 */
class server_interface {
public:
    virtual ~server_interface() = default;
    
    /**
     * @brief Register GET handler for a route
     * @param pattern Route pattern/path
     * @param handler Request handler callback
     */
    virtual void register_get(const std::string& pattern, request_handler handler) = 0;
    
    /**
     * @brief Register POST handler for a route
     * @param pattern Route pattern/path
     * @param handler Request handler callback
     */
    virtual void register_post(const std::string& pattern, request_handler handler) = 0;
    
    /**
     * @brief Register DELETE handler for a route
     * @param pattern Route pattern/path
     * @param handler Request handler callback
     */
    virtual void register_delete(const std::string& pattern, request_handler handler) = 0;
    
    /**
     * @brief Register OPTIONS handler for a route
     * @param pattern Route pattern/path
     * @param handler Request handler callback
     */
    virtual void register_options(const std::string& pattern, request_handler handler) = 0;
    
    /**
     * @brief Set mount point for static file serving
     * @param mount_point URL path prefix
     * @param dir Directory path on filesystem
     * @param headers Additional headers to send with static files
     * @return true if successful
     */
    virtual bool set_mount_point(const std::string& mount_point, 
                                const std::string& dir,
                                const headers_map& headers = headers_map()) = 0;
    
    /**
     * @brief Start server (blocking call)
     * @param host Host address to bind to
     * @param port Port number to listen on
     * @return true if server started successfully
     */
    virtual bool listen(const std::string& host, int port) = 0;
    
    /**
     * @brief Stop the server
     */
    virtual void stop() = 0;
};

/**
 * @brief HTTP client result structure
 * 
 * Contains the result of an HTTP client request.
 * Replaces httplib::Result.
 */
struct client_result {
    bool success = false;      ///< Whether request succeeded (no network/connection error)
    int status_code = 0;       ///< HTTP status code (200, 404, etc.)
    std::string body;          ///< Response body
    std::string error_message; ///< Error message if !success
    headers_map headers;       ///< Response headers
    
    /**
     * @brief Check if request was successful (2xx status)
     */
    bool is_ok() const {
        return success && (status_code >= 200 && status_code < 300);
    }
    
    /**
     * @brief Implicit bool conversion (checks success)
     */
    operator bool() const {
        return success;
    }
};

/**
 * @brief Streaming callback for client-side SSE
 * 
 * Called for each chunk of data received during streaming.
 * Replaces httplib callback in Client::Get().
 * 
 * @param data Pointer to received data chunk
 * @param size Size of data chunk in bytes
 * @return true to continue receiving, false to stop
 */
using streaming_callback = std::function<bool(const char* data, size_t size)>;

/**
 * @brief HTTP client interface
 * 
 * Abstract interface for HTTP clients.
 * Can be implemented using httplib (current) or boost::beast (future).
 */
class client_interface {
public:
    virtual ~client_interface() = default;
    
    /**
     * @brief Send GET request
     * @param path Request path
     * @return Client result with response data
     */
    virtual client_result get(const std::string& path) = 0;
    
    /**
     * @brief Send GET request with streaming callback (for SSE)
     * @param path Request path
     * @param callback Function called for each data chunk received
     * @return Client result (status after stream completes)
     */
    virtual client_result get_stream(const std::string& path, streaming_callback callback) = 0;
    
    /**
     * @brief Send POST request
     * @param path Request path
     * @param headers Request headers
     * @param body Request body
     * @param content_type Content-Type header value
     * @return Client result with response data
     */
    virtual client_result post(const std::string& path,
                              const headers_map& headers,
                              const std::string& body,
                              const std::string& content_type) = 0;
    
    /**
     * @brief Set connection timeout
     * @param seconds Timeout in seconds
     */
    virtual void set_connection_timeout(int seconds) = 0;
    
    /**
     * @brief Set read timeout
     * @param seconds Timeout in seconds
     */
    virtual void set_read_timeout(int seconds) = 0;
    
    /**
     * @brief Set write timeout
     * @param seconds Timeout in seconds
     */
    virtual void set_write_timeout(int seconds) = 0;
    
    /**
     * @brief Set default headers for all requests
     * @param headers Headers to include in all requests
     */
    virtual void set_default_headers(const headers_map& headers) = 0;
    
    /**
     * @brief Enable/disable server certificate verification (SSL/TLS)
     * @param enable true to enable verification, false to disable
     */
    virtual void set_certificate_verification(bool enable) = 0;
    
    /**
     * @brief Set CA certificate path for SSL/TLS verification
     * @param path Path to CA certificate file
     */
    virtual void set_ca_cert_path(const std::string& path) = 0;
    
    /**
     * @brief Stop any active requests
     */
    virtual void stop() = 0;
};

/**
 * @brief Factory function to create HTTP server
 * 
 * Creates the appropriate server implementation based on configuration.
 * Currently returns httplib-based server, will transition to boost::beast.
 * 
 * @param use_ssl Whether to use SSL/TLS
 * @param cert_path Path to SSL certificate (required if use_ssl=true)
 * @param key_path Path to SSL private key (required if use_ssl=true)
 * @return Unique pointer to server instance
 */
std::unique_ptr<server_interface> create_server(
    bool use_ssl = false,
    const std::string& cert_path = "",
    const std::string& key_path = ""
);

/**
 * @brief Factory function to create HTTP client
 * 
 * Creates the appropriate client implementation based on configuration.
 * Currently returns httplib-based client, will transition to boost::beast.
 * 
 * @param scheme_host_port Base URL (e.g., "http://localhost:8080")
 * @return Unique pointer to client instance
 */
std::unique_ptr<client_interface> create_client(const std::string& scheme_host_port);

} // namespace http
} // namespace mcp

#endif // MCP_HTTP_ABSTRACTION_H
