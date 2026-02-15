# httplib Migration Inventory for cpp-mcp

**Version:** 1.0  
**Date:** 2026-02-15  
**Purpose:** Comprehensive analysis of httplib usage in preparation for boost::beast migration

---

## Executive Summary

This document inventories all usage of the cpp-httplib library in the cpp-mcp project. The cpp-httplib library (version 0.19.0) is currently used to provide HTTP server and client functionality for the Model Context Protocol (MCP) implementation. This inventory identifies all call sites, public API dependencies, and implementation details to facilitate a potential migration to boost::beast.

### Key Statistics

- **Total Files Using httplib:** 10
  - Source files: 2 (mcp_server.cpp, mcp_sse_client.cpp)
  - Header files: 2 (mcp_server.h, mcp_sse_client.h)
  - Test files: 4 (streamable_http_transport_test.cpp, http_security_test.cpp, mcp_test.cpp, lifecycle_compliance_test.cpp)
  - Example files: 1 (agent_example.cpp)
  - Library header: 1 (common/httplib.h)

- **Direct httplib:: Usage Count:**
  - Source implementations: 28 usages
  - Test files: 42 usages
  - Header files: 14 usages

- **Critical Dependencies:**
  - Server-Sent Events (SSE) streaming with chunked transfer encoding
  - Dual HTTP client pattern (separate clients for JSON-RPC and SSE)
  - SSL/TLS support (optional compilation flag)

---

## 1. httplib Library Overview

### 1.1 Library Information

- **Library:** cpp-httplib
- **Version:** 0.19.0
- **License:** MIT License
- **Copyright:** 2025 Yuji Hirose
- **Location:** `common/httplib.h` (single-header library, 350,546 bytes)
- **Include Pattern:** `#include "httplib.h"` (with relative path to common/)

### 1.2 Key httplib Classes and Types

Based on analysis of `common/httplib.h`:

#### Core Classes
```cpp
namespace httplib {
    class Server;              // Line 939  - HTTP server
    class SSLServer;           // Line 1943 - HTTPS server (extends Server)
    class ClientImpl;          // Line 1215 - HTTP client implementation
    class Client;              // Line 1644 - HTTP client interface
    class SSLClient;           // Line 1972 - HTTPS client
}
```

#### Core Types
```cpp
namespace httplib {
    struct Request;            // HTTP request structure
    struct Response;           // HTTP response structure
    class DataSink;            // Streaming data sink for chunked responses
    
    using Headers = std::multimap<std::string, std::string>;
    using ResponseHandler = std::function<bool(const Response &response)>;
    using Logger = std::function<void(const Request &, const Response &)>;
    
    enum class Error;          // Error codes for client operations
    
    // Helper functions
    std::string to_string(Error);  // Convert error enum to string
}
```

---

## 2. Server Implementation (mcp_server)

### 2.1 File: `include/mcp_server.h`

#### Public API Exposure

**Direct httplib Types in Public API:**

1. **Member Variables (Private):**
   ```cpp
   // Line 421
   std::unique_ptr<httplib::Server> http_server_;
   ```

2. **Method Parameters (Public):**
   ```cpp
   // Line 78 - event_dispatcher::wait_event()
   bool wait_event(httplib::DataSink* sink, 
                   const std::chrono::milliseconds& timeout);
   
   // Line 411 - server::set_mount_point()
   bool set_mount_point(const std::string& mount_point, 
                       const std::string& dir, 
                       httplib::Headers headers = httplib::Headers());
   ```

3. **Handler Signatures (Private):**
   ```cpp
   // Lines 482-497 - All handle_* methods use httplib types
   void handle_sse(const httplib::Request& req, httplib::Response& res);
   void handle_jsonrpc(const httplib::Request& req, httplib::Response& res);
   void handle_mcp(const httplib::Request& req, httplib::Response& res);
   void handle_mcp_get(const httplib::Request& req, httplib::Response& res);
   void handle_mcp_post(const httplib::Request& req, httplib::Response& res);
   void handle_mcp_delete(const httplib::Request& req, httplib::Response& res);
   void handle_batch_jsonrpc(const json& batch_json, 
                            const std::string& session_id, 
                            httplib::Response& res);
   ```

4. **Helper Methods (Private):**
   ```cpp
   // Line 527
   std::string extract_session_id(const httplib::Request& req) const;
   
   // Line 530
   void set_session_id_header(httplib::Response& res, 
                             const std::string& session_id) const;
   
   // Line 581
   bool should_validate_origin(const httplib::Request& req) const;
   ```

**Impact Assessment:**
- **Public API:** 2 methods expose httplib types (`wait_event`, `set_mount_point`)
- **Private Implementation:** Heavily coupled to httplib::Request and httplib::Response
- **Migration Complexity:** HIGH - Core request/response handling throughout

### 2.2 File: `src/mcp_server.cpp`

#### httplib Class Instantiation

**Lines 38-45: Server Construction**
```cpp
#ifdef MCP_SSL        
    http_server_ = std::make_unique<httplib::SSLServer>(
        conf.ssl.server_cert_path->c_str(),
        conf.ssl.server_private_key_path->c_str()
    );
#else
    http_server_ = std::make_unique<httplib::Server>();
#endif
```

**Instantiation Pattern:**
- Conditional compilation for SSL support
- SSL: `httplib::SSLServer(cert_path, key_path)`
- Non-SSL: `httplib::Server()`
- Stored as `std::unique_ptr<httplib::Server>` (polymorphism)

#### HTTP Method Registration

**Lines 61-114: Route Registration**
```cpp
// CORS OPTIONS handler
http_server_->Options(".*", [](const httplib::Request& req, httplib::Response& res) { ... });

// MCP unified endpoint (2025-03-26 spec)
http_server_->Get(mcp_endpoint_.c_str(), [](const httplib::Request& req, httplib::Response& res) { ... });
http_server_->Post(mcp_endpoint_.c_str(), [](const httplib::Request& req, httplib::Response& res) { ... });
http_server_->Delete(mcp_endpoint_.c_str(), [](const httplib::Request& req, httplib::Response& res) { ... });

// Legacy endpoints (deprecated)
http_server_->Post(msg_endpoint_.c_str(), [](const httplib::Request& req, httplib::Response& res) { ... });
http_server_->Get(sse_endpoint_.c_str(), [](const httplib::Request& req, httplib::Response& res) { ... });
```

**Methods Used:**
- `Server::Options(route, handler)` - CORS preflight
- `Server::Get(route, handler)` - SSE connections
- `Server::Post(route, handler)` - JSON-RPC messages
- `Server::Delete(route, handler)` - Session termination

**Handler Signature:**
```cpp
std::function<void(const httplib::Request& req, httplib::Response& res)>
```

#### Server Lifecycle

**Lines 147-180: Server Start**
```cpp
// Blocking mode
server_thread_ = std::make_unique<std::thread>([this]() {
    if (!http_server_->listen(host_.c_str(), port_)) {
        LOG_ERROR("Failed to start server on ", host_, ":", port_);
    }
});

// Non-blocking mode  
if (!http_server_->listen(host_.c_str(), port_)) {
    LOG_ERROR("Failed to start server on ", host_, ":", port_);
    return false;
}
```

**Methods Used:**
- `Server::listen(host, port)` - Blocking server start
- Returns `bool` indicating success/failure

**Lines 302-320: Server Stop**
```cpp
if (http_server_) {
    http_server_->stop();
}
```

**Methods Used:**
- `Server::stop()` - Graceful shutdown

#### Request/Response Handling Patterns

**Request Object Usage:**
```cpp
// Header access
auto it = req.headers.find("Origin");
std::string origin = it->second;

// Query parameter access
auto it = req.params.find("session_id");
std::string session_id = it != req.params.end() ? it->second : "";

// Request metadata
req.method          // HTTP method string
req.path            // Request path
req.remote_addr     // Client IP address
req.remote_port     // Client port
req.body            // Request body (POST data)
```

**Response Object Usage:**
```cpp
// Status code
res.status = 200;           // Success
res.status = 204;           // No Content
res.status = 400;           // Bad Request
res.status = 403;           // Forbidden
res.status = 404;           // Not Found
res.status = 500;           // Internal Server Error

// Headers
res.set_header("Content-Type", "application/json");
res.set_header("Access-Control-Allow-Origin", "*");
res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
res.set_header("Access-Control-Allow-Headers", "Content-Type, Mcp-Session-Id");
res.set_header("Mcp-Session-Id", session_id);

// Content
res.set_content(json_string, "application/json");
```

#### Server-Sent Events (SSE) Implementation

**CRITICAL PATTERN - Lines 587-618 and 1076-1098:**
```cpp
res.set_chunked_content_provider(
    "text/event-stream",
    [this, session_id, session_dispatcher](
        size_t /* offset */, 
        httplib::DataSink& sink
    ) -> bool {
        try {
            // Check if session is closed
            if (session_dispatcher->is_closed()) {
                return false;  // Stop streaming
            }
            
            // Update activity timestamp
            session_dispatcher->update_activity();
            
            // Wait for event (blocking with timeout)
            bool result = session_dispatcher->wait_event(&sink);
            if (!result) {
                LOG_WARNING("Failed to wait for event, closing connection");
                close_session(session_id);
                return false;  // Stop streaming
            }
            
            // Update activity after successful send
            session_dispatcher->update_activity();
            return true;  // Continue streaming
        } catch (const std::exception& e) {
            LOG_ERROR("SSE content provider exception: ", e.what());
            close_session(session_id);
            return false;  // Stop streaming
        }
    }
);
```

**SSE Pattern Details:**
- **Method:** `Response::set_chunked_content_provider(content_type, provider_lambda)`
- **Content-Type:** `"text/event-stream"` (SSE standard)
- **Provider Signature:** `std::function<bool(size_t offset, httplib::DataSink& sink)>`
- **Return Value:** `bool` (true = continue, false = stop streaming)
- **DataSink Usage:** `sink.write(data, size)` in event_dispatcher::wait_event()

**SSE Data Writing (event_dispatcher::wait_event in mcp_server.h lines 78-125):**
```cpp
bool wait_event(httplib::DataSink* sink, const std::chrono::milliseconds& timeout) {
    // ... wait for event data ...
    
    try {
        if (!message_copy.empty()) {
            if (!sink->write(message_copy.data(), message_copy.size())) {
                close();
                return false;
            }
        }
        return true;
    } catch (...) {
        close();
        return false;
    }
}
```

**SSE Message Format (Line 779):**
```cpp
std::string sse_message = "event: message\r\ndata: " + message.dump() + "\r\n\r\n";
```

#### Additional Server Methods

**Line 1835: Mount Point for Static Files**
```cpp
bool server::set_mount_point(const std::string& mount_point, 
                             const std::string& dir, 
                             httplib::Headers headers) {
    if (!http_server_) {
        return false;
    }
    return http_server_->set_mount_point(mount_point.c_str(), dir.c_str(), headers);
}
```

**Methods Used:**
- `Server::set_mount_point(path, directory, headers)` - Serve static files

---

## 3. Client Implementation (mcp_sse_client)

### 3.1 File: `include/mcp_sse_client.h`

#### Public API Exposure

**Direct httplib Types in Public API:**

1. **Member Variables (Private):**
   ```cpp
   // Line 211
   std::unique_ptr<httplib::Client> http_client_;
   
   // Line 214
   std::unique_ptr<httplib::Client> sse_client_;
   ```

**Impact Assessment:**
- **Public API:** No direct exposure of httplib types
- **Private Implementation:** Two separate httplib::Client instances
- **Migration Complexity:** MEDIUM - Internal implementation only

### 3.2 File: `src/mcp_sse_client.cpp`

#### httplib Class Instantiation

**Lines 26-44: Dual Client Pattern**
```cpp
void sse_client::init_client(const std::string& scheme_host_port, 
                             bool validate_certificates,
                             const std::string& ca_cert_path) {
    // Client for JSON-RPC POST requests
    http_client_ = std::make_unique<httplib::Client>(scheme_host_port.c_str());
    
    // Separate client for SSE GET streaming
    sse_client_ = std::make_unique<httplib::Client>(scheme_host_port.c_str());

    // Configure timeouts for JSON-RPC client
    http_client_->set_connection_timeout(timeout_seconds_, 0);
    http_client_->set_read_timeout(timeout_seconds_, 0);
    http_client_->set_write_timeout(timeout_seconds_, 0);
    
    // Configure timeouts for SSE client (longer connection timeout)
    sse_client_->set_connection_timeout(timeout_seconds_ * 2, 0);
    sse_client_->set_write_timeout(timeout_seconds_, 0);

    #ifdef MCP_SSL
    http_client_->enable_server_certificate_verification(validate_certificates);
    sse_client_->enable_server_certificate_verification(validate_certificates);
    if (!ca_cert_path.empty()) {
        http_client_->set_ca_cert_path(ca_cert_path.c_str());
        sse_client_->set_ca_cert_path(ca_cert_path.c_str());
    }
    #endif
}
```

**Instantiation Pattern:**
- `httplib::Client(scheme_host_port)` where scheme_host_port is "http://host:port" or "https://host:port"
- **Two separate instances** to avoid blocking issues (SSE streaming blocks the connection)
- Separate timeout configurations for different use cases

#### Client Configuration Methods

**Timeout Configuration:**
```cpp
Client::set_connection_timeout(seconds, microseconds)
Client::set_read_timeout(seconds, microseconds)
Client::set_write_timeout(seconds, microseconds)
```

**SSL Configuration:**
```cpp
Client::enable_server_certificate_verification(bool)
Client::set_ca_cert_path(const char* path)
```

**Header Configuration:**
```cpp
// Lines 133, 136
http_client_->set_default_headers(headers);
sse_client_->set_default_headers(headers);
```
- Type: `httplib::Headers` (multimap of string pairs)

#### HTTP Request Patterns

**POST Request (JSON-RPC):**
```cpp
// Line 512-544 (inferred from code structure)
httplib::Headers headers;
headers.emplace("Content-Type", "application/json");

auto res = http_client_->Post(msg_endpoint_.c_str(), 
                              headers,
                              req_json.dump(), 
                              "application/json");

if (!res) {
    std::string error_msg = httplib::to_string(res.error());
    throw mcp_exception(error_code::internal_error, error_msg);
}

if (res->status / 100 != 2) {
    std::string error_msg = httplib::to_string(res.error());
    throw mcp_exception(error_code::internal_error, error_msg);
}
```

**Return Type:** `httplib::Result` (optional-like wrapper)
- Check success: `if (!res)` or `if (res)`
- Access response: `res->status`, `res->headers`, `res->body`
- Get error: `res.error()` returns `httplib::Error` enum

#### SSE Streaming Pattern

**CRITICAL PATTERN - Lines 266-336:**
```cpp
sse_thread_ = std::make_unique<std::thread>([this]() {
    int retry_count = 0;
    const int max_retries = 5;
    const int retry_delay_base = 1000;  // ms
    
    while (sse_running_) {
        try {
            std::string buffer;
            
            // SSE GET request with streaming callback
            auto res = sse_client_->Get(
                sse_endpoint_, 
                [&, this](const char* data, size_t data_length) -> bool {
                    // Accumulate data
                    buffer.append(data, data_length);
                    
                    // Normalize CRLF to LF
                    size_t crlf_pos = buffer.find("\r\n");
                    while (crlf_pos != std::string::npos) {
                        buffer.replace(crlf_pos, 2, "\n");
                        crlf_pos = buffer.find("\r\n", crlf_pos + 1);
                    }
                    
                    // Process complete SSE events (delimited by \n\n)
                    size_t start_pos = 0;
                    while ((start_pos = buffer.find("\n\n", start_pos)) != std::string::npos) {
                        size_t end_pos = start_pos + 2;
                        std::string event = buffer.substr(0, start_pos);
                        buffer.erase(0, end_pos);
                        start_pos = 0;
                        
                        if (!parse_sse_data(event.data(), event.size())) {
                            LOG_ERROR("Failed to parse SSE event");
                        }
                    }
                    
                    // Return true to continue streaming, false to stop
                    return sse_running_.load();
                }
            );
            
            // Check result
            if (!res || res->status / 100 != 2) {
                std::string error_msg = "SSE connection failed: ";
                error_msg += httplib::to_string(res.error());
                throw std::runtime_error(error_msg);
            }
            
            retry_count = 0;  // Reset on success
        } catch (const std::exception& e) {
            // Retry logic with exponential backoff
            if (!sse_running_) break;
            
            if (++retry_count > max_retries) {
                LOG_ERROR("Maximum retry count reached");
                break;
            }
            
            int delay = retry_delay_base * (1 << (retry_count - 1));
            
            // Sleep with periodic checks for shutdown
            const int check_interval = 100;
            for (int waited = 0; waited < delay && sse_running_; waited += check_interval) {
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval));
            }
        }
    }
});
```

**SSE Client Callback Signature:**
```cpp
std::function<bool(const char* data, size_t data_length)>
```
- **Parameters:** Raw data chunk and length
- **Return:** `bool` (true = continue, false = stop)
- **Threading:** Callback runs in the same thread as `Get()`
- **Blocking:** `Get()` blocks until complete or callback returns false

#### Error Handling

**Error Code Conversion:**
```cpp
// Lines 295, 524, 544
std::string error_msg = httplib::to_string(res.error());
```

**Function:** `httplib::to_string(httplib::Error)` - Convert error enum to string

**Error Checking Pattern:**
```cpp
if (!res) {
    // Connection failed, use res.error()
}

if (res->status / 100 != 2) {
    // HTTP error status (4xx, 5xx)
}
```

---

## 4. Test Files

### 4.1 File: `test/streamable_http_transport_test.cpp`

**httplib Usage Count:** 13 instances

**Test Client Pattern:**
```cpp
// Line 59, 92
http_client = std::make_unique<httplib::Client>("localhost", port_);
http_client->set_read_timeout(5, 0);  // 5 seconds timeout
```

**SSE Test Pattern (Lines 92-143):**
```cpp
auto sse_client = std::make_shared<httplib::Client>("localhost", port_);
std::atomic<httplib::Client*> client_ptr{nullptr};

std::thread sse_thread([&, sse_client]() {
    client_ptr.store(sse_client.get(), std::memory_order_release);
    
    auto res = sse_client->Get("/mcp", 
        [&](const char* data, size_t len) {
            std::string response(data, len);
            
            // Process SSE data
            if (response.find("endpoint") != std::string::npos) {
                // Extract and store endpoint
                // ...
                return false;  // Stop reading
            }
            return true;  // Continue reading
        });
    
    client_ptr.store(nullptr, std::memory_order_release);
});

// Wait for result...

// IMPORTANT: Stop client before thread cleanup
auto client = client_ptr.load(std::memory_order_acquire);
if (client) {
    client->stop();
}

if (sse_thread.joinable()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (sse_thread.joinable()) {
        sse_thread.detach();
    }
}
```

**Test Assertions:**
```cpp
// Line 76 - Extract session ID from response
httplib::Result res = http_client->Post(...);
ASSERT_TRUE(res);
EXPECT_EQ(res->status, 200);

auto it = res->headers.find("Mcp-Session-Id");
ASSERT_NE(it, res->headers.end());
std::string session_id = it->second;
```

**Header Construction:**
```cpp
// Lines 179-181
httplib::Headers headers = {
    {"Mcp-Session-Id", session_id},
    {"Content-Type", "application/json"}
};
```

### 4.2 File: `test/http_security_test.cpp`

**httplib Usage Count:** 17 instances

**Similar patterns to streamable_http_transport_test.cpp:**
- Client instantiation: `httplib::Client(host, port)`
- Result checking: `httplib::Result`
- Header access: `res->headers.find()`
- Status checking: `res->status`

### 4.3 File: `test/mcp_test.cpp`

**httplib Usage Count:** 8 instances

**Minimal direct usage:**
- Mostly uses `mcp::sse_client` which wraps httplib
- Some direct `httplib::Client` usage for edge case testing

### 4.4 File: `test/lifecycle_compliance_test.cpp`

**httplib Usage Count:** 4 instances

**Usage:**
- Test client for protocol compliance verification
- Similar patterns to other test files

---

## 5. Example Files

### 5.1 File: `examples/agent_example.cpp`

**httplib Usage Count:** 2 instances

**Usage:**
```cpp
#include "httplib.h"  // Line 1

// Used indirectly through mcp::server and mcp::sse_client
// No direct httplib API calls
```

**Impact:** Minimal - only includes header for dependency

---

## 6. Public API Surface Analysis

### 6.1 Types Exposed in Public Headers

#### mcp_server.h Public API

**Directly Exposed httplib Types:**

1. **`httplib::DataSink*`** (Line 78)
   - In: `event_dispatcher::wait_event()`
   - Visibility: Public method of public class
   - Usage: Write SSE data to client
   - **Migration Impact:** HIGH - Public API break

2. **`httplib::Headers`** (Line 411)
   - In: `server::set_mount_point()`
   - Visibility: Public method
   - Usage: Optional headers for static file serving
   - **Migration Impact:** MEDIUM - Public API break, but optional feature

**Indirectly Exposed (Private members/methods):**

3. **`httplib::Server*`** (Line 421)
   - In: `server::http_server_` member
   - Visibility: Private
   - **Migration Impact:** LOW - Internal only

4. **`httplib::Request&`** (Lines 482-581)
   - In: Multiple private handler methods
   - Visibility: Private
   - **Migration Impact:** LOW - Internal only

5. **`httplib::Response&`** (Lines 482-581)
   - In: Multiple private handler methods
   - Visibility: Private
   - **Migration Impact:** LOW - Internal only

#### mcp_sse_client.h Public API

**Directly Exposed httplib Types:** NONE

**Indirectly Exposed (Private members):**

1. **`httplib::Client*`** (Lines 211, 214)
   - In: `sse_client::http_client_` and `sse_client::sse_client_` members
   - Visibility: Private
   - **Migration Impact:** LOW - Internal only

### 6.2 API Break Analysis

#### Breaking Changes Required

**Public API breaks (users must update code):**

1. **`event_dispatcher::wait_event(httplib::DataSink* sink, ...)`**
   - Current: Takes `httplib::DataSink*`
   - Required change: Abstract sink interface or use boost::beast::http::serializer
   - **Workaround:** Create adapter/wrapper type

2. **`server::set_mount_point(..., httplib::Headers headers)`**
   - Current: Takes `httplib::Headers` (multimap<string, string>)
   - Required change: Use generic header container or boost::beast equivalent
   - **Workaround:** Change signature to `std::multimap<std::string, std::string>`

#### Non-Breaking Changes (Internal only)

All other httplib usage is internal to implementation files and can be changed without affecting public API.

---

## 7. Implementation Patterns and Idioms

### 7.1 Server-Side Patterns

#### Pattern 1: Route Registration with Lambda Handlers

```cpp
http_server_->Get("/path", [this](const httplib::Request& req, httplib::Response& res) {
    // Handle request
    res.status = 200;
    res.set_content(body, "application/json");
});
```

**boost::beast Equivalent:**
- Manual routing logic required
- Handler pattern: `void handle_request(beast::http::request<Body>, beast::http::response<Body>&)`

#### Pattern 2: Chunked Content Provider (SSE)

```cpp
res.set_chunked_content_provider(
    "text/event-stream",
    [](size_t offset, httplib::DataSink& sink) -> bool {
        sink.write(data, size);
        return true;  // Continue
    }
);
```

**boost::beast Equivalent:**
- Use `beast::http::response<beast::http::buffer_body>` or custom body type
- Manual chunked transfer encoding with `beast::http::chunk_encode()`
- More complex implementation required

#### Pattern 3: Request Header Access

```cpp
auto it = req.headers.find("Header-Name");
if (it != req.headers.end()) {
    std::string value = it->second;
}
```

**boost::beast Equivalent:**
- `request.find("Header-Name")` or `request["Header-Name"]`
- Similar API, minimal changes

#### Pattern 4: Response Configuration

```cpp
res.status = 200;
res.set_header("Content-Type", "application/json");
res.set_content(body, "application/json");
```

**boost::beast Equivalent:**
```cpp
response.result(beast::http::status::ok);
response.set(beast::http::field::content_type, "application/json");
response.body() = body;
response.prepare_payload();
```

### 7.2 Client-Side Patterns

#### Pattern 1: Dual Client Architecture

```cpp
http_client_ = std::make_unique<httplib::Client>(url);  // For POST
sse_client_ = std::make_unique<httplib::Client>(url);   // For GET/SSE
```

**Rationale:** Separate clients prevent blocking issues when one connection is streaming

**boost::beast Equivalent:**
- Use separate `beast::tcp_stream` or `beast::ssl_stream` instances
- Manual async I/O required (boost::asio)

#### Pattern 2: Streaming GET with Callback

```cpp
auto res = client->Get(endpoint, 
    [](const char* data, size_t len) -> bool {
        process_chunk(data, len);
        return true;  // Continue
    }
);
```

**boost::beast Equivalent:**
- Use `beast::http::response_parser` with custom body type
- Async read operations: `beast::http::async_read_some()`
- More complex implementation

#### Pattern 3: Result Checking

```cpp
if (!res) {
    std::string error = httplib::to_string(res.error());
    throw exception(error);
}

if (res->status / 100 != 2) {
    throw exception("HTTP error");
}
```

**boost::beast Equivalent:**
- Separate error handling with `boost::system::error_code`
- Status: `response.result_int()`
- Different error model

### 7.3 SSL/TLS Patterns

#### Pattern 1: Conditional SSL Compilation

```cpp
#ifdef MCP_SSL
    http_server_ = std::make_unique<httplib::SSLServer>(cert, key);
#else
    http_server_ = std::make_unique<httplib::Server>();
#endif
```

**boost::beast Equivalent:**
- `beast::ssl_stream<beast::tcp_stream>` wrapper
- Conditional compilation still needed
- Requires boost::asio::ssl::context

#### Pattern 2: Certificate Verification

```cpp
client->enable_server_certificate_verification(true);
client->set_ca_cert_path(ca_cert);
```

**boost::beast Equivalent:**
```cpp
ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);
ssl_context.load_verify_file(ca_cert);
```

---

## 8. Test Coverage Analysis

### 8.1 Existing HTTP Tests

#### Test Suite: `StreamableHttpTransportTest`
**File:** `test/streamable_http_transport_test.cpp`

**Coverage:**
- ✅ SSE connection establishment (GET /mcp)
- ✅ Session ID header exchange
- ✅ JSON-RPC POST with session
- ✅ SSE streaming with callbacks
- ✅ Client stop() during streaming

**httplib-Specific Tests:**
- Threading model with client->stop()
- Streaming callback return values
- Result type checking

#### Test Suite: `HttpSecurityTest`
**File:** `test/http_security_test.cpp`

**Coverage:**
- ✅ Origin header validation
- ✅ CORS headers
- ✅ Tool confirmation hooks
- ✅ Security configuration

**httplib-Specific Tests:**
- Header validation and access
- Status code handling

#### Test Suite: `LifecycleComplianceTest`
**File:** `test/lifecycle_compliance_test.cpp`

**Coverage:**
- ✅ MCP protocol lifecycle
- ✅ Initialize/initialized flow

**httplib-Specific Tests:**
- Minimal direct usage

#### Test Suite: Core Tests
**File:** `test/mcp_test.cpp`

**Coverage:**
- ✅ Basic server/client functionality
- ✅ Tool registration and calling
- ✅ Resource handling
- ✅ Batch requests

**httplib-Specific Tests:**
- Indirect through mcp::sse_client wrapper

### 8.2 Test Gaps for Migration

#### Critical Gaps

1. **No explicit httplib API boundary tests**
   - Tests focus on MCP protocol, not HTTP layer
   - Need tests that explicitly verify HTTP behavior

2. **No performance/stress tests for streaming**
   - SSE long-running connections
   - Multiple concurrent clients
   - Large message handling

3. **No explicit timeout tests**
   - Connection timeout behavior
   - Read/write timeout behavior
   - SSE keepalive/heartbeat

4. **No SSL/TLS test coverage**
   - Certificate validation
   - SSL handshake errors
   - Mixed HTTP/HTTPS

5. **No error path coverage**
   - Network disconnection during SSE
   - Malformed HTTP responses
   - Client abort scenarios

#### Recommended New Tests

**For boost::beast Migration:**

1. **HTTP Compliance Tests:**
   ```cpp
   TEST(HttpLayer, ChunkedTransferEncoding)
   TEST(HttpLayer, KeepAliveConnections)
   TEST(HttpLayer, RequestPipelining)
   TEST(HttpLayer, HeaderSizeLimits)
   ```

2. **SSE Streaming Tests:**
   ```cpp
   TEST(SSE, LongRunningConnection)
   TEST(SSE, MultipleClients)
   TEST(SSE, ClientDisconnection)
   TEST(SSE, ServerShutdownDuringStream)
   TEST(SSE, HeartbeatTiming)
   ```

3. **Error Handling Tests:**
   ```cpp
   TEST(ErrorHandling, NetworkDisconnect)
   TEST(ErrorHandling, TimeoutBehavior)
   TEST(ErrorHandling, MalformedRequests)
   TEST(ErrorHandling, ResourceExhaustion)
   ```

4. **Performance Tests:**
   ```cpp
   TEST(Performance, ConcurrentConnections)
   TEST(Performance, LargeMessageHandling)
   TEST(Performance, StreamingThroughput)
   ```

---

## 9. Migration Complexity Assessment

### 9.1 Complexity by Component

| Component | Lines of Code | httplib Usage | Complexity | Effort (Days) |
|-----------|---------------|---------------|------------|---------------|
| mcp_server.cpp | 1,969 | 22 instances | **HIGH** | 8-12 |
| mcp_sse_client.cpp | 624 | 6 instances | **HIGH** | 5-8 |
| mcp_server.h | 586 | 9 instances | **MEDIUM** | 3-5 |
| mcp_sse_client.h | 259 | 5 instances | **LOW** | 1-2 |
| Test files | ~2,000 | 42 instances | **MEDIUM** | 5-8 |
| **TOTAL** | **~5,438** | **84 instances** | **HIGH** | **22-35 days** |

### 9.2 Critical Path Items

#### Highest Risk

1. **SSE Chunked Content Provider** (mcp_server.cpp)
   - No direct equivalent in boost::beast
   - Requires custom implementation
   - Critical for MCP protocol compliance

2. **Dual Client Architecture** (mcp_sse_client.cpp)
   - Separate blocking/streaming clients
   - boost::beast requires async I/O model
   - Significant refactoring needed

3. **Threading Model**
   - httplib handles threading internally
   - boost::beast requires manual thread management
   - Must preserve thread safety

#### Medium Risk

4. **Request/Response Abstraction**
   - Many handler functions use httplib::Request/Response
   - Need wrapper types or direct boost::beast types
   - Affects internal API surface

5. **Error Handling**
   - Different error models (httplib::Error vs boost::system::error_code)
   - Async error propagation with boost::asio
   - Exception vs error code patterns

6. **SSL/TLS Configuration**
   - Different API (httplib vs boost::asio::ssl)
   - Certificate loading and validation
   - Conditional compilation preserved

#### Lower Risk

7. **Static File Serving** (set_mount_point)
   - Optional feature
   - Can implement separately or defer
   - Not critical for MCP protocol

8. **Header Handling**
   - Similar APIs
   - Mostly mechanical changes
   - Well-defined behavior

---

## 10. Recommended Migration Strategy

### 10.1 Phase 1: Preparation (5-7 days)

1. **Create Abstraction Layer**
   - Define generic HTTP request/response interfaces
   - Abstract away httplib-specific types
   - Start with wrappers, migrate incrementally

   ```cpp
   namespace mcp::http {
       struct request_interface {
           virtual std::string method() const = 0;
           virtual std::string path() const = 0;
           virtual std::string header(const std::string& name) const = 0;
           virtual std::string body() const = 0;
           // ...
       };
       
       struct response_interface {
           virtual void set_status(int code) = 0;
           virtual void set_header(const std::string& name, const std::string& value) = 0;
           virtual void set_body(const std::string& content, const std::string& type) = 0;
           // ...
       };
       
       struct streaming_sink_interface {
           virtual bool write(const char* data, size_t size) = 0;
       };
   }
   ```

2. **Implement httplib Adapters**
   - Create adapter classes wrapping httplib types
   - Implement interfaces with httplib backend
   - Test compatibility with existing code

3. **Expand Test Coverage**
   - Add HTTP layer tests (see section 8.2)
   - Add SSE streaming edge cases
   - Add error path coverage

### 10.2 Phase 2: boost::beast Implementation (10-14 days)

4. **Implement boost::beast Server**
   - Create server using boost::asio and boost::beast
   - Implement routing and handler dispatch
   - Implement SSE chunked streaming

5. **Implement boost::beast Client**
   - Create async client with boost::asio
   - Implement SSE streaming callback pattern
   - Maintain dual-client architecture

6. **SSL/TLS Support**
   - Add boost::asio::ssl::context
   - Certificate loading and validation
   - Maintain conditional compilation

### 10.3 Phase 3: Integration and Testing (7-10 days)

7. **Replace httplib with boost::beast**
   - Swap httplib adapters for boost::beast adapters
   - Update server and client implementations
   - Fix compilation errors

8. **Comprehensive Testing**
   - Run all existing tests
   - Run new HTTP layer tests
   - Performance and stress testing

9. **API Compatibility Review**
   - Verify public API surface
   - Document breaking changes
   - Update examples and documentation

### 10.4 Phase 4: Cleanup and Optimization (2-4 days)

10. **Remove httplib Dependencies**
    - Remove common/httplib.h
    - Update CMakeLists.txt
    - Update vcpkg.json dependencies

11. **Performance Optimization**
    - Profile boost::beast implementation
    - Optimize async I/O patterns
    - Tune thread pool and buffers

12. **Documentation**
    - Update README with new dependencies
    - Document API changes
    - Migration guide for users

### 10.5 Total Effort Estimate

**Best Case:** 24 working days (4.8 weeks)  
**Expected:** 35 working days (7 weeks)  
**Worst Case:** 45 working days (9 weeks)

**Assumptions:**
- 1 experienced C++ developer
- Familiarity with boost::beast and boost::asio
- Existing test infrastructure
- No major scope changes

---

## 11. Alternative Strategies

### 11.1 Keep httplib for Client, Migrate Server Only

**Rationale:**
- Server has more complex SSE requirements
- Client is simpler, less risk
- Partial migration reduces effort

**Pros:**
- Lower effort: ~15-20 days
- Reduced risk
- Incremental approach

**Cons:**
- Mixed dependencies (httplib + boost::beast)
- Inconsistent codebase
- Larger binary size

### 11.2 Abstract HTTP Layer with Plugin Architecture

**Rationale:**
- Support multiple HTTP backends
- Allow runtime selection
- Future-proof for other libraries

**Pros:**
- Flexibility
- No vendor lock-in
- Easier testing

**Cons:**
- Higher complexity
- Performance overhead
- More code to maintain

### 11.3 Defer Migration Until boost::beast Matures

**Rationale:**
- httplib is working well
- boost::beast is still evolving
- Wait for better documentation/examples

**Pros:**
- Zero effort now
- Less risk
- More mature boost::beast later

**Cons:**
- Technical debt accumulates
- httplib may become unmaintained
- Migration becomes harder over time

---

## 12. Dependency Analysis

### 12.1 Current Dependencies

**From vcpkg.json:**
```json
{
  "dependencies": [
    "nlohmann-json",
    // httplib is header-only in common/, not in vcpkg.json
  ]
}
```

**httplib Details:**
- **Type:** Header-only library
- **Source:** `common/httplib.h` (copied into project)
- **Version:** 0.19.0
- **Size:** 350 KB (single file)
- **Dependencies:** Standard library only (OpenSSL optional for SSL)

### 12.2 boost::beast Dependencies

**Required:**
```json
{
  "dependencies": [
    "boost-beast",      // HTTP/WebSocket library
    "boost-asio",       // Async I/O (included with beast)
    "boost-system",     // Error codes (included with beast)
    "boost-core",       // Core utilities (included with beast)
    "openssl"           // For SSL/TLS support (optional but needed)
  ]
}
```

**Size Impact:**
- boost::beast + boost::asio: ~500 KB headers + compiled libs
- Increased build time
- Larger binary size

**Platform Support:**
- ✅ Linux (tested)
- ✅ Windows (tested)
- ❌ macOS (no longer supported)
- ✅ Same as current httplib support

---

## 13. Performance Considerations

### 13.1 Expected Performance Changes

**httplib Characteristics:**
- Synchronous blocking I/O
- Thread-per-connection model (server)
- Blocking calls (client)
- Good for low-concurrency scenarios

**boost::beast Characteristics:**
- Asynchronous non-blocking I/O
- Event-driven model
- Better scalability
- More complex code

**Expected Impacts:**

| Metric | httplib | boost::beast | Change |
|--------|---------|--------------|--------|
| Max Concurrent Connections | ~100-200 | ~1000+ | ✅ Better |
| Latency (low load) | ~1ms | ~1-2ms | ≈ Similar |
| Throughput | Moderate | High | ✅ Better |
| Memory per Connection | ~1MB | ~10KB | ✅ Better |
| CPU Usage (idle) | Low | Very Low | ✅ Better |
| CPU Usage (high load) | High | Moderate | ✅ Better |
| Code Complexity | Low | High | ❌ Worse |

### 13.2 Benchmarking Requirements

**Before Migration:**
1. Measure current performance baseline
   - Requests per second
   - Latency distribution
   - Memory usage
   - CPU usage

2. Test scenarios:
   - Single client
   - 10 concurrent clients
   - 100 concurrent clients
   - Long-running SSE connections
   - Large message payloads

**After Migration:**
- Re-run same benchmarks
- Compare results
- Identify regressions
- Optimize if needed

---

## 14. Risk Assessment

### 14.1 Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| SSE streaming incompatibility | **HIGH** | **HIGH** | Prototype early, add tests |
| Thread safety issues | **MEDIUM** | **HIGH** | Code review, stress testing |
| Performance regression | **LOW** | **MEDIUM** | Benchmarking, profiling |
| API breaking changes | **HIGH** | **MEDIUM** | Versioning, migration guide |
| SSL/TLS configuration issues | **MEDIUM** | **MEDIUM** | Early testing, documentation |
| boost::beast bugs | **LOW** | **HIGH** | Use stable version, testing |
| Timeline overrun | **MEDIUM** | **MEDIUM** | Phased approach, buffer time |

### 14.2 Mitigation Strategies

1. **Prototype Critical Features First**
   - SSE streaming
   - Dual client pattern
   - Threading model

2. **Comprehensive Testing**
   - Unit tests for each component
   - Integration tests for full stack
   - Stress tests for edge cases

3. **Gradual Rollout**
   - Feature flag for httplib vs beast
   - Run both in parallel initially
   - Gradual migration of users

4. **Documentation**
   - Clear migration guide
   - API change documentation
   - Troubleshooting guide

---

## 15. File-by-File Impact Summary

### 15.1 High Impact Files (Require Significant Changes)

| File | LOC | httplib Usage | Changes Required | Effort |
|------|-----|---------------|------------------|--------|
| `src/mcp_server.cpp` | 1,969 | 22 | Server impl, SSE streaming, routing | **8-12 days** |
| `src/mcp_sse_client.cpp` | 624 | 6 | Client impl, SSE callback, dual client | **5-8 days** |
| `include/mcp_server.h` | 586 | 9 | Type changes, adapters | **3-5 days** |

### 15.2 Medium Impact Files (Moderate Changes)

| File | LOC | httplib Usage | Changes Required | Effort |
|------|-----|---------------|------------------|--------|
| `test/streamable_http_transport_test.cpp` | ~500 | 13 | Test client updates | **2-3 days** |
| `test/http_security_test.cpp` | ~400 | 17 | Test client updates | **2-3 days** |
| `test/mcp_test.cpp` | ~1,200 | 8 | Indirect, minimal changes | **1-2 days** |
| `test/lifecycle_compliance_test.cpp` | ~300 | 4 | Minimal changes | **1 day** |

### 15.3 Low Impact Files (Minor Changes)

| File | LOC | httplib Usage | Changes Required | Effort |
|------|-----|---------------|------------------|--------|
| `include/mcp_sse_client.h` | 259 | 5 | Type changes | **1-2 days** |
| `examples/agent_example.cpp` | ~500 | 2 | Include changes only | **< 1 day** |

### 15.4 Files to Remove

| File | Size | Purpose |
|------|------|---------|
| `common/httplib.h` | 350 KB | httplib library (to be removed) |

### 15.5 Files to Add

| File | Purpose | Estimated Size |
|------|---------|----------------|
| `include/mcp_http_interface.h` | HTTP abstraction layer | ~200 lines |
| `src/mcp_http_beast_impl.cpp` | boost::beast implementation | ~800-1,200 lines |
| `test/http_layer_test.cpp` | HTTP layer tests | ~400-600 lines |

---

## 16. Recommendations

### 16.1 Go/No-Go Decision Criteria

**Proceed with Migration IF:**
- ✅ Need better scalability (100+ concurrent connections)
- ✅ Want to reduce dependency on unmaintained libraries
- ✅ Have 6-8 weeks of development time available
- ✅ Can afford API breaking changes
- ✅ Need boost for other features anyway

**Delay Migration IF:**
- ❌ Current performance is acceptable
- ❌ Limited development resources
- ❌ Need API stability for external users
- ❌ Project is in maintenance mode
- ❌ httplib continues to be maintained

### 16.2 Short-Term Actions (If Proceeding)

1. **Week 1-2: Proof of Concept**
   - Implement SSE streaming with boost::beast
   - Verify dual client pattern works
   - Benchmark simple server/client

2. **Week 3-4: Abstraction Layer**
   - Define HTTP interface
   - Implement httplib adapter
   - Update one component to use abstraction

3. **Week 5-6: Main Implementation**
   - Implement boost::beast server
   - Implement boost::beast client
   - Add SSL/TLS support

4. **Week 7-8: Testing and Integration**
   - Full test suite
   - Performance testing
   - Bug fixing

5. **Week 9: Documentation and Release**
   - Migration guide
   - API documentation
   - Release notes

### 16.3 Alternative: Stay with httplib

**If deciding NOT to migrate:**

1. **Vendor the Library Properly**
   - Keep httplib in common/ (current approach is fine)
   - Track version explicitly
   - Monitor for security updates

2. **Add Abstraction Layer Anyway**
   - Future-proof for potential migration
   - Easier testing
   - Cleaner code

3. **Expand Test Coverage**
   - Add HTTP layer tests
   - Test edge cases
   - Performance benchmarks

4. **Monitor httplib Project**
   - Watch for security issues
   - Track maintenance status
   - Prepare migration plan if abandoned

---

## 17. Conclusion

The cpp-mcp project has **moderate to high coupling** with cpp-httplib, with 84 direct usages across 10 files. The most critical dependencies are:

1. **SSE Chunked Streaming** - Custom implementation required for boost::beast
2. **Dual Client Architecture** - Pattern must be preserved
3. **Public API Surface** - 2 methods expose httplib types directly

**Migration to boost::beast is feasible but complex**, requiring an estimated **6-9 weeks** of focused development effort. The primary challenge is reimplementing the SSE streaming pattern without direct equivalent in boost::beast.

**Recommendation:** 
- If **scalability** or **boost ecosystem** is a priority: Proceed with migration
- If **stability** and **minimal effort** are priorities: Stay with httplib but add abstraction layer

This inventory provides the foundation for making an informed decision and executing a successful migration if chosen.

---

## Appendices

### Appendix A: httplib API Reference (Used Features)

**Server APIs:**
```cpp
class Server {
    bool listen(const char* host, int port);
    void stop();
    Server& Options(const char* pattern, Handler handler);
    Server& Get(const char* pattern, Handler handler);
    Server& Post(const char* pattern, Handler handler);
    Server& Delete(const char* pattern, Handler handler);
    bool set_mount_point(const char* mount_point, const char* dir, Headers headers);
};

using Handler = std::function<void(const Request&, Response&)>;

struct Request {
    std::string method;
    std::string path;
    Headers headers;
    std::string body;
    Params params;
    std::string remote_addr;
    int remote_port;
};

struct Response {
    int status;
    Headers headers;
    std::string body;
    
    void set_header(const char* key, const char* val);
    void set_content(const std::string& s, const char* content_type);
    void set_chunked_content_provider(
        const char* content_type,
        ContentProvider provider
    );
};

using ContentProvider = std::function<bool(size_t offset, DataSink& sink)>;

class DataSink {
    bool write(const char* d, size_t l);
};
```

**Client APIs:**
```cpp
class Client {
    Client(const char* scheme_host_port);
    
    void set_connection_timeout(time_t sec, time_t usec);
    void set_read_timeout(time_t sec, time_t usec);
    void set_write_timeout(time_t sec, time_t usec);
    void set_default_headers(Headers headers);
    
    Result Get(const char* path);
    Result Get(const char* path, ContentReceiver receiver);
    Result Post(const char* path, const Headers& headers, 
               const std::string& body, const char* content_type);
    
    void stop();
    
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    void enable_server_certificate_verification(bool enabled);
    void set_ca_cert_path(const char* ca_cert_path);
    #endif
};

using ContentReceiver = std::function<bool(const char* data, size_t data_length)>;

class Result {
    explicit operator bool() const;
    Response* operator->();
    Error error();
};

std::string to_string(Error err);
```

### Appendix B: Complete File List

**Source Files:**
1. `src/mcp_server.cpp` - Server implementation
2. `src/mcp_sse_client.cpp` - Client implementation

**Header Files:**
3. `include/mcp_server.h` - Server public API
4. `include/mcp_sse_client.h` - Client public API

**Test Files:**
5. `test/streamable_http_transport_test.cpp` - Transport tests
6. `test/http_security_test.cpp` - Security tests
7. `test/mcp_test.cpp` - Core functionality tests
8. `test/lifecycle_compliance_test.cpp` - Protocol compliance tests

**Example Files:**
9. `examples/agent_example.cpp` - Agent example

**Library Files:**
10. `common/httplib.h` - cpp-httplib library (350 KB)

### Appendix C: Useful Resources

**cpp-httplib:**
- GitHub: https://github.com/yhirose/cpp-httplib
- Documentation: https://github.com/yhirose/cpp-httplib/blob/master/README.md

**boost::beast:**
- Documentation: https://www.boost.org/doc/libs/release/libs/beast/
- Examples: https://www.boost.org/doc/libs/release/libs/beast/doc/html/beast/examples.html
- Tutorial: https://www.boost.org/doc/libs/release/libs/beast/doc/html/beast/quick_start.html

**Relevant Standards:**
- HTTP/1.1: RFC 7230-7235
- Server-Sent Events: https://html.spec.whatwg.org/multipage/server-sent-events.html
- WebSocket (for future): RFC 6455

---

**End of Document**
