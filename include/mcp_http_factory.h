/**
 * @file mcp_http_factory.h
 * @brief Default HTTP factory implementations
 * 
 * This file provides the default factory implementations for creating
 * HTTP server and client instances. Uses Boost.Beast.
 */

#ifndef MCP_HTTP_FACTORY_H
#define MCP_HTTP_FACTORY_H

#include "mcp_http_abstraction.h"
#include "mcp_http_beast_adapter.h"

namespace mcp {
namespace http {

/**
 * @brief Create HTTP server using Boost.Beast (default)
 * 
 * This is the default server implementation. Uses Boost.Beast for
 * modern async I/O and better performance.
 * 
 * @param use_ssl Whether to use SSL/TLS
 * @param cert_path Path to SSL certificate (required if use_ssl=true)
 * @param key_path Path to SSL private key (required if use_ssl=true)
 * @return Unique pointer to server instance
 */
inline std::unique_ptr<server_interface> create_server(
    bool use_ssl,
    const std::string& cert_path,
    const std::string& key_path
) {
    // Phase 3: Now testing with Beast adapter
    return create_beast_server(use_ssl, cert_path, key_path);
}

/**
 * @brief Create HTTP client using Boost.Beast (default)
 * 
 * This is the default client implementation. Uses Boost.Beast for
 * modern async I/O and better performance.
 * 
 * @param scheme_host_port Base URL (e.g., "http://localhost:8080")
 * @return Unique pointer to client instance
 */
inline std::unique_ptr<client_interface> create_client(const std::string& scheme_host_port) {
    return create_beast_client(scheme_host_port);
}

} // namespace http
} // namespace mcp

#endif // MCP_HTTP_FACTORY_H
