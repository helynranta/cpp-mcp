# Complete httplib Inventory & TDD Coverage Plan

**Version:** 2.0  
**Date:** 2026-02-15  
**Purpose:** Comprehensive inventory of all httplib usage with TDD coverage plan for boost::beast migration

---

## Executive Summary

This document provides a complete inventory of all httplib usage across the cpp-mcp repository, including production code, tests, and examples. It also outlines a comprehensive Test-Driven Development (TDD) plan for migrating to boost::beast.

### Quick Statistics

| Category | Count |
|----------|-------|
| **Total Files Using httplib** | 10 files |
| **Production Files** | 4 (2 headers, 2 implementations) |
| **Test Files** | 5 |
| **Example Files** | 1 |
| **Total httplib:: References** | 84+ occurrences |
| **Public API Methods Exposing httplib** | 2 (BREAKING) |
| **HTTP Abstraction Tests** | 0 (MISSING - Critical Gap) |

---

## Table of Contents

1. [Production Code Inventory](#1-production-code-inventory)
2. [Test Code Inventory](#2-test-code-inventory)
3. [Example Code Inventory](#3-example-code-inventory)
4. [HTTP Abstraction Layer Status](#4-http-abstraction-layer-status)
5. [Current Test Coverage Analysis](#5-current-test-coverage-analysis)
6. [TDD Plan for Client Refactor](#6-tdd-plan-for-client-refactor)
7. [TDD Plan for Server Refactor](#7-tdd-plan-for-server-refactor)
8. [Test Gap Analysis](#8-test-gap-analysis)
9. [Recommendations](#9-recommendations)

---

## 1. Production Code Inventory

### 1.1 Server Implementation

#### File: `include/mcp_server.h` (586 LOC)

**httplib Usage Count:** 9 occurrences

**Public API Exposure (BREAKING CHANGES):**

1. **Line 78:** `event_dispatcher::wait_event()` 
   ```cpp
   bool wait_event(httplib::DataSink* sink, 
                   const std::chrono::milliseconds& timeout);
   ```
   - **Impact:** PUBLIC API - requires breaking change
   - **Migration:** Replace with `mcp::http::streaming_data_sink*`
   - **Used by:** All SSE streaming endpoints

2. **Line 411:** `server::set_mount_point()`
   ```cpp
   bool set_mount_point(const std::string& path, 
                       const std::string& dir,
                       httplib::Headers headers = httplib::Headers());
   ```
   - **Impact:** PUBLIC API - requires breaking change
   - **Migration:** Replace with `mcp::http::headers_map`
   - **Used by:** Static file serving

**Private Members:**

3. **Line 421:** `std::unique_ptr<httplib::Server> http_server_`
   - **Impact:** Internal implementation detail
   - **Migration:** Replace with `std::unique_ptr<mcp::http::server_interface>`

**Header Include:**

4. **Line 21:** `#include "httplib.h"`
   - **Impact:** Exposes httplib to all includers
   - **Migration:** Replace with `#include "mcp_http_abstraction.h"`

#### File: `src/mcp_server.cpp` (1,969 LOC)

**httplib Usage Count:** 22 occurrences

**Critical Patterns:**

1. **Server Instantiation (Lines 38-44):**
   ```cpp
   #ifdef MCP_SSL
   if (conf.ssl.server_cert_path && conf.ssl.server_private_key_path) {
       http_server_ = std::make_unique<httplib::SSLServer>(
           conf.ssl.server_cert_path->c_str(),
           conf.ssl.server_private_key_path->c_str());
   } else {
       http_server_ = std::make_unique<httplib::Server>();
   }
   #else
   http_server_ = std::make_unique<httplib::Server>();
   #endif
   ```
   - **Impact:** Core server initialization
   - **Migration:** Use factory: `mcp::http::create_server()`

2. **Route Registration (Lines 61-100):**
   ```cpp
   http_server_->Options(".*", [this](const httplib::Request& req, 
                                      httplib::Response& res) { ... });
   http_server_->Get(mcp_endpoint_.c_str(), [this](...) { ... });
   http_server_->Post(mcp_endpoint_.c_str(), [this](...) { ... });
   http_server_->Delete(mcp_endpoint_.c_str(), [this](...) { ... });
   ```
   - **Impact:** All HTTP routing
   - **Migration:** Use `server_interface::register_get/post/delete()`

3. **SSE Streaming Pattern (Lines 587-618, 1076-1098):**
   ```cpp
   res.set_chunked_content_provider(
       "text/event-stream",
       [this, session_id](size_t offset, httplib::DataSink& sink) -> bool {
           auto& dispatcher = sessions_[session_id].dispatcher;
           if (!dispatcher->wait_event(&sink, timeout)) {
               return false;  // Stop streaming
           }
           return true;  // Continue streaming
       }
   );
   ```
   - **Impact:** CRITICAL - Core SSE implementation
   - **Migration:** Use `response_builder::set_streaming_content()`
   - **Complexity:** HIGH - No direct beast equivalent

4. **Server Lifecycle:**
   ```cpp
   // Line ~180: Start server
   http_server_->listen(host_.c_str(), port_)
   
   // Line ~250: Stop server
   http_server_->stop()
   ```
   - **Impact:** Server startup/shutdown
   - **Migration:** Use `server_interface::start()` and `stop()`

**All httplib References in mcp_server.cpp:**
- Line 38: `httplib::SSLServer` (constructor)
- Line 41: `httplib::Server` (constructor)
- Line 44: `httplib::Server` (constructor)
- Line 61: `httplib::Request` (handler parameter)
- Line 61: `httplib::Response` (handler parameter)
- Line 89: `httplib::Request` (handler parameter)
- Line 89: `httplib::Response` (handler parameter)
- Line 94: `httplib::Request` (handler parameter)
- Line 94: `httplib::Response` (handler parameter)
- Line 99: `httplib::Request` (handler parameter)
- Line 99: `httplib::Response` (handler parameter)
- Multiple more in request handlers...
- Line 587: `httplib::DataSink` (SSE streaming)
- Line 1076: `httplib::DataSink` (SSE streaming)

### 1.2 Client Implementation

#### File: `include/mcp_sse_client.h` (180 LOC)

**httplib Usage Count:** 5 occurrences

**Header Include:**
- **Line 18:** `#include "httplib.h"`

**Private Members:**
```cpp
// Lines not shown in excerpt, but from memory analysis:
std::unique_ptr<httplib::Client> http_client_;  // For POST requests
std::unique_ptr<httplib::Client> sse_client_;   // For GET/SSE streaming
```
- **Impact:** Dual client pattern - prevents blocking
- **Migration:** Replace with `mcp::http::client_interface`

#### File: `src/mcp_sse_client.cpp` (624 LOC)

**httplib Usage Count:** 6 occurrences

**Critical Patterns:**

1. **Dual Client Initialization (Lines 26-27):**
   ```cpp
   http_client_ = std::make_unique<httplib::Client>(scheme_host_port.c_str());
   sse_client_ = std::make_unique<httplib::Client>(scheme_host_port.c_str());
   ```
   - **Impact:** CRITICAL - Architectural pattern
   - **Why:** Prevents SSE blocking normal requests
   - **Migration:** Must preserve dual connection pattern with async I/O

2. **Timeout Configuration (Lines 29-34):**
   ```cpp
   http_client_->set_connection_timeout(timeout_seconds_, 0);
   http_client_->set_read_timeout(timeout_seconds_, 0);
   http_client_->set_write_timeout(timeout_seconds_, 0);
   sse_client_->set_connection_timeout(timeout_seconds_ * 2, 0);
   sse_client_->set_write_timeout(timeout_seconds_, 0);
   ```
   - **Impact:** Important for reliability
   - **Migration:** Use `client_interface::set_timeout()`

3. **SSL Configuration (Lines 37-42):**
   ```cpp
   #ifdef MCP_SSL
   http_client_->enable_server_certificate_verification(validate_certificates);
   sse_client_->enable_server_certificate_verification(validate_certificates);
   if (!ca_cert_path.empty()) {
       http_client_->set_ca_cert_path(ca_cert_path.c_str());
       sse_client_->set_ca_cert_path(ca_cert_path.c_str());
   }
   #endif
   ```
   - **Impact:** Security configuration
   - **Migration:** Use `client_interface::enable_ssl()`

4. **SSE Streaming with Callback (Lines 266-336):**
   ```cpp
   auto res = sse_client_->Get(endpoint,
       [&](const char* data, size_t len) -> bool {
           // Process SSE chunks
           buffer.append(data, len);
           // Parse events...
           return true;  // Continue streaming
       }
   );
   ```
   - **Impact:** CRITICAL - SSE client pattern
   - **Migration:** Use `client_interface::get_streaming()`
   - **Complexity:** HIGH - Synchronous to async pattern change

5. **POST Requests:**
   ```cpp
   auto res = http_client_->Post(msg_endpoint_.c_str(), 
                                  headers, 
                                  req_str, 
                                  "application/json");
   ```
   - **Impact:** Standard JSON-RPC requests
   - **Migration:** Use `client_interface::post()`

---

## 2. Test Code Inventory

### 2.1 HTTP Security Tests

#### File: `test/http_security_test.cpp` (~400 LOC)

**httplib Usage Count:** 17 occurrences

**Purpose:** Tests HTTP security features (CORS, Origin validation, authentication)

**Pattern Used:**
```cpp
httplib::Client client("localhost", test_port);
httplib::Headers headers = {
    {"Content-Type", "application/json"},
    {"Origin", "http://trusted-origin.com"}
};
auto res = client.Post("/mcp", headers, body, "application/json");
```

**Lines with httplib::**
- Line 46: `httplib::Client` (test client)
- Line 49: `httplib::Headers` (request headers)
- Line 71: `httplib::Client`
- Line 74: `httplib::Headers`
- Line 96: `httplib::Client`
- Line 99: `httplib::Headers`
- Line 120: `httplib::Client`
- Line 123: `httplib::Headers`
- Line 145: `httplib::Client`
- Line 148: `httplib::Headers`
- Line 170: `httplib::Client`
- Line 173: `httplib::Headers`
- Line 198: `httplib::Client`
- Line 201: `httplib::Headers`
- Line 223: `httplib::Client`
- Line 226: `httplib::Headers` (valid headers)
- Line 236: `httplib::Headers` (invalid headers)

**Test Coverage:**
- ✅ CORS headers
- ✅ Origin validation
- ✅ Authentication tokens
- ✅ HTTP methods (GET, POST, DELETE, OPTIONS)

**Migration Needs:**
- Replace `httplib::Client` with test helper using abstraction
- Create `http_test_client` wrapper class
- Update all 17+ client instantiations

### 2.2 Streamable HTTP Transport Tests

#### File: `test/streamable_http_transport_test.cpp` (~500 LOC)

**httplib Usage Count:** 13 occurrences

**Purpose:** Tests streamable HTTP transport (MCP 2025-03-26 spec)

**Pattern Used:**
```cpp
std::unique_ptr<httplib::Client> http_client;
http_client = std::make_unique<httplib::Client>("localhost", port_);

// SSE streaming test
auto sse_client = std::make_shared<httplib::Client>("localhost", port_);
std::atomic<httplib::Client*> client_ptr{nullptr};
```

**Lines with httplib::**
- Line 59: `std::make_unique<httplib::Client>`
- Line 76: `httplib::Result` (return type)
- Line 92: `std::make_shared<httplib::Client>`
- Line 93: `std::atomic<httplib::Client*>`
- Line 145: `std::unique_ptr<httplib::Client>` (member variable)
- Line 179: `httplib::Headers`
- Line 216: `httplib::Headers`
- Line 241: `httplib::Headers`
- Line 273: `httplib::Headers`
- Line 337: `std::make_shared<httplib::Client>`
- Line 338: `std::atomic<httplib::Client*>`
- Line 411: `httplib::Headers`
- Line 443: `httplib::Headers`

**Test Coverage:**
- ✅ Session creation via POST
- ✅ SSE streaming connection
- ✅ Session deletion
- ✅ Multiple concurrent sessions
- ✅ Session ID handling

**Migration Needs:**
- Replace test clients with abstraction
- Update SSE streaming test pattern
- Preserve dual-client test pattern

### 2.3 Core MCP Protocol Tests

#### File: `test/mcp_test.cpp` (1,500+ LOC)

**httplib Usage Count:** 8 occurrences

**Purpose:** Core MCP protocol tests (initialization, tools, resources, etc.)

**Lines with httplib::**
- Line 218: Comment referencing `httplib::Client`
- Line 219: `std::make_unique<httplib::Client>`
- Line 232: `std::shared_ptr<std::atomic<httplib::Client*>>`
- Line 238: `std::make_shared<httplib::Client>`
- Line 399: Comment referencing `httplib::Client`
- Line 400: `std::make_unique<httplib::Client>`
- Line 413: `std::shared_ptr<std::atomic<httplib::Client*>>`
- Line 419: `std::make_shared<httplib::Client>`

**Pattern Used:**
```cpp
// For protocol version tests
std::unique_ptr<httplib::Client> http_client = 
    std::make_unique<httplib::Client>("localhost", port_);

// For SSE client management
auto sse_client_ptr = std::make_shared<std::atomic<httplib::Client*>>(nullptr);
auto sse_client = std::make_shared<httplib::Client>("localhost", test_port);
```

**Test Coverage:**
- ✅ Initialize/Initialized lifecycle
- ✅ Protocol version negotiation
- ✅ Tool listing and execution
- ✅ Resource listing
- ✅ Ping/Pong
- ✅ Batch requests

**Migration Needs:**
- Replace direct httplib usage in protocol tests
- Most tests use `sse_client` class (good abstraction)
- Only version/ping tests use raw httplib

### 2.4 Lifecycle Compliance Tests

#### File: `test/lifecycle_compliance_test.cpp` (~200 LOC)

**httplib Usage Count:** 4 occurrences

**Purpose:** Tests MCP lifecycle compliance (2025-03-26 spec)

**Lines with httplib::**
- Line 58: `std::make_unique<httplib::Client>`
- Line 61: `std::make_shared<httplib::Client>`
- Line 123: `std::unique_ptr<httplib::Client>` (member)
- Line 125: `std::atomic<httplib::Client*>` (member)

**Pattern:** Similar to streamable transport tests

**Test Coverage:**
- ✅ Lifecycle state transitions
- ✅ Initialize → Initialized flow
- ✅ Session management

**Migration Needs:**
- Replace test client instantiation
- Use abstraction layer for all HTTP calls

### 2.5 Proof of Concept Test

#### File: `test/beast_sse_proof_of_concept.cpp` (343 LOC)

**httplib Usage Count:** 0 (Uses Boost.Beast)

**Purpose:** Proves Boost.Beast can handle SSE streaming

**Status:** ✅ Complete and passing

**Tests:**
- ✅ `BeastSSEProofOfConcept.CanStreamSSE` 
- ✅ `BeastSSEProofOfConcept.DataSinkPattern`

**Value for Migration:**
- Demonstrates chunked transfer encoding with Beast
- Shows DataSink pattern can be adapted
- Reference implementation for Beast adapter

---

## 3. Example Code Inventory

### 3.1 Agent Example

#### File: `examples/agent_example.cpp` (500+ LOC)

**httplib Usage Count:** 2 occurrences

**Purpose:** Example of autonomous agent using MCP tools

**Lines with httplib::**
- Line 1: `#include "httplib.h"`
- Line 173: `static httplib::Client client(config.base_url);`
- Line 196: `httplib::to_string(res.error())`

**Usage Pattern:**
```cpp
// Uses httplib to call external LLM API
static httplib::Client client(config.base_url);
auto res = client.Post(config.endpoint, headers, body, "application/json");
if (!res) {
    std::cerr << "Failed: " << httplib::to_string(res.error());
}
```

**Migration Needs:**
- This is external API call, not MCP protocol
- Could use abstraction or keep httplib for simplicity
- Not critical for MCP migration (separate concern)

---

## 4. HTTP Abstraction Layer Status

### 4.1 Abstraction Interfaces

#### File: `include/mcp_http_abstraction.h`

**Status:** ✅ Complete (Phase 1)

**Interfaces Defined:**
1. `request_data` - HTTP request POD
2. `streaming_data_sink` - Abstract sink for SSE (replaces `httplib::DataSink`)
3. `response_builder` - Abstract response interface
4. `server_interface` - Abstract HTTP server
5. `client_interface` - Abstract HTTP client
6. `client_result` - Response wrapper

**Factory Functions:**
- `create_server(config)` - Create server instance
- `create_client(url, config)` - Create client instance

### 4.2 httplib Adapter

#### File: `include/mcp_http_httplib_adapter.h`

**Status:** ✅ Complete (Phase 1)

**Implementation:**
- `httplib_server` - Wraps `httplib::Server`
- `httplib_client` - Wraps `httplib::Client`
- `httplib_data_sink` - Wraps `httplib::DataSink`
- `httplib_response_builder` - Wraps `httplib::Response`

**Testing Status:** ❌ NO TESTS (Critical Gap!)

### 4.3 Beast Adapter

#### File: `include/mcp_http_beast_adapter.h`

**Status:** ⚠️ Stub Only (Phase 2 TODO)

**Contains:** Interface stubs with TODO comments

**Implementation Needed:**
- `beast_server` - Full async server
- `beast_client` - Async client
- `beast_data_sink` - Chunked encoding wrapper
- `beast_response_builder` - Beast response mapper

**Testing Status:** ❌ NO TESTS

---

## 5. Current Test Coverage Analysis

### 5.1 What's Tested

✅ **MCP Protocol Coverage (Excellent):**
- Initialize/Initialized lifecycle
- Tool registration and execution
- Resource listing
- Prompts
- Batch requests
- Ping/Pong
- Notifications
- Cancellation
- Progress reporting

✅ **HTTP Features Tested:**
- CORS headers
- Origin validation
- Session management
- SSE streaming (basic)
- Authentication

✅ **Security Features:**
- Origin validation
- Token authentication
- Tool confirmation

### 5.2 Critical Test Gaps

❌ **HTTP Abstraction Layer (MISSING - CRITICAL):**
- No tests for `mcp_http_abstraction.h` interfaces
- No tests for `httplib_adapter`
- No tests for `beast_adapter`
- No validation that abstractions work correctly

❌ **HTTP Transport Details (MISSING):**
- Chunked transfer encoding validation
- Keep-alive connection handling
- Connection timeout edge cases
- SSL/TLS handshake errors
- Large payload handling (>1MB)

❌ **SSE Streaming Edge Cases (PARTIAL):**
- ⚠️ Basic SSE works but missing:
  - Long-running connections (>1 hour)
  - Client disconnection during stream
  - Server shutdown during stream
  - Multiple concurrent SSE clients (>100)
  - Heartbeat timing validation
  - Reconnection logic

❌ **Error Handling (PARTIAL):**
- ⚠️ Some errors tested but missing:
  - Network disconnection mid-request
  - DNS resolution failures
  - Connection refused
  - Read/write timeout edge cases
  - Malformed HTTP responses
  - Resource exhaustion (ports, memory)

❌ **Performance/Load Tests (MISSING):**
- No load testing
- No stress testing
- No concurrent client tests
- No memory leak validation

### 5.3 Test Maturity Assessment

| Category | Coverage | Quality | Maturity |
|----------|----------|---------|----------|
| **MCP Protocol** | 90% | High | Mature |
| **HTTP Security** | 70% | Medium | Good |
| **HTTP Transport** | 40% | Medium | Developing |
| **HTTP Abstraction** | 0% | N/A | **Not Started** |
| **SSE Streaming** | 50% | Medium | Basic |
| **Error Handling** | 30% | Low | Immature |
| **Performance** | 0% | N/A | **Not Started** |

**Overall Maturity:** **Moderate** - Good MCP protocol coverage, weak HTTP layer coverage

---

## 6. TDD Plan for Client Refactor

### 6.1 Prerequisites (Week 1)

**Goal:** Establish test infrastructure for HTTP abstraction

#### Task 1: Create HTTP Abstraction Tests
**File:** `test/http_abstraction_test.cpp`

**Tests to Write:**
```cpp
// Interface contract tests
TEST(HttpAbstraction, RequestDataStructure) { }
TEST(HttpAbstraction, ResponseBuilderInterface) { }
TEST(HttpAbstraction, StreamingDataSinkInterface) { }
TEST(HttpAbstraction, ClientInterfaceContract) { }
TEST(HttpAbstraction, ClientResultStructure) { }

// Header handling
TEST(HttpAbstraction, HeaderManipulation) { }
TEST(HttpAbstraction, HeaderCaseInsensitivity) { }

// Factory functions
TEST(HttpAbstraction, CreateClientFactory) { }
TEST(HttpAbstraction, ClientConfiguration) { }
```

**Estimated Effort:** 1 day

#### Task 2: Create httplib Adapter Tests
**File:** `test/httplib_adapter_test.cpp`

**Tests to Write:**
```cpp
// Basic client operations
TEST(HttplibAdapter, ClientConstruction) { }
TEST(HttplibAdapter, SimpleGetRequest) { }
TEST(HttplibAdapter, SimplePostRequest) { }
TEST(HttplibAdapter, RequestHeaders) { }
TEST(HttplibAdapter, ResponseParsing) { }

// Timeout configuration
TEST(HttplibAdapter, SetConnectionTimeout) { }
TEST(HttplibAdapter, SetReadTimeout) { }
TEST(HttplibAdapter, SetWriteTimeout) { }
TEST(HttplibAdapter, TimeoutActuallyWorks) { }

// SSL/TLS
TEST(HttplibAdapter, EnableSSL) { }
TEST(HttplibAdapter, CertificateValidation) { }
TEST(HttplibAdapter, CACertificatePath) { }

// Streaming
TEST(HttplibAdapter, StreamingGetRequest) { }
TEST(HttplibAdapter, StreamingCallback) { }
TEST(HttplibAdapter, DataSinkWriteOperation) { }

// Error handling
TEST(HttplibAdapter, ConnectionRefused) { }
TEST(HttplibAdapter, Timeout) { }
TEST(HttplibAdapter, NetworkError) { }
TEST(HttplibAdapter, InvalidURL) { }
```

**Estimated Effort:** 2 days

### 6.2 Client Migration - Phase 1 (Week 2)

**Goal:** Migrate `sse_client` to use abstractions with httplib adapter

#### Task 1: Update Client Header
**File:** `include/mcp_sse_client.h`

**TDD Steps:**

1. **Write failing test:**
   ```cpp
   TEST(SSEClient, UsesHttpAbstraction) {
       sse_client client("http://localhost:8889");
       // Verify client uses abstraction internally
       // This will fail initially
   }
   ```

2. **Make it pass:**
   - Replace `#include "httplib.h"` with `#include "mcp_http_abstraction.h"`
   - Change `std::unique_ptr<httplib::Client> http_client_` to
     `std::unique_ptr<mcp::http::client_interface> http_client_`
   - Same for `sse_client_`

3. **Run all tests** - Ensure nothing breaks

#### Task 2: Update Client Implementation
**File:** `src/mcp_sse_client.cpp`

**TDD Steps for each method:**

**Example: `init_client()`**
1. Write test:
   ```cpp
   TEST(SSEClient, InitializesWithAbstraction) {
       sse_client client("http://localhost:8889");
       // Verify initialization works
   }
   ```

2. Refactor:
   ```cpp
   // Before:
   http_client_ = std::make_unique<httplib::Client>(url);
   http_client_->set_connection_timeout(timeout, 0);
   
   // After:
   mcp::http::client_config config;
   config.connection_timeout_ms = timeout * 1000;
   http_client_ = mcp::http::create_client(url, config);
   ```

3. Run test - should still pass

**Example: `send_request()`**
1. Write test:
   ```cpp
   TEST(SSEClient, SendsRequestViaAbstraction) {
       sse_client client("http://localhost:8889");
       client.initialize("test", "1.0");
       // Verify POST works
   }
   ```

2. Refactor:
   ```cpp
   // Before:
   auto res = http_client_->Post(endpoint, headers, body, "application/json");
   
   // After:
   mcp::http::request_data req;
   req.path = endpoint;
   req.headers = headers;
   req.body = body;
   auto res = http_client_->post(req);
   ```

3. Run test

**Example: `open_sse_connection()`**
1. Write test:
   ```cpp
   TEST(SSEClient, OpensSSEStreamViaAbstraction) {
       // Start test server
       mcp::server srv(...);
       srv.start(false);
       
       sse_client client("http://localhost:8889");
       client.initialize("test", "1.0");
       // Verify SSE connection works
   }
   ```

2. Refactor:
   ```cpp
   // Before:
   auto res = sse_client_->Get(endpoint, 
       [&](const char* data, size_t len) -> bool {
           process_chunk(data, len);
           return true;
       });
   
   // After:
   auto callback = [&](const char* data, size_t len) -> bool {
       process_chunk(data, len);
       return true;
   };
   auto res = sse_client_->get_streaming(endpoint, headers, callback);
   ```

3. Run test

#### Task 3: Run Full Test Suite
```bash
cd build && ctest -V
```

**Expected:** All existing tests should still pass

**Estimated Effort:** 3-4 days

### 6.3 Client Migration - Phase 2 (Week 3)

**Goal:** Switch to Beast adapter, fix any issues

#### Task 1: Implement Beast Client Adapter
**File:** `include/mcp_http_beast_adapter.h`

**TDD Steps:**

1. **Write tests first** (reuse httplib adapter tests):
   ```cpp
   // test/beast_adapter_test.cpp
   TEST(BeastAdapter, ClientConstruction) { }
   TEST(BeastAdapter, SimpleGetRequest) { }
   // ... copy all httplib adapter tests
   ```

2. **Implement `beast_client` class** to pass tests

3. **Run tests** - iterate until all pass

**Estimated Effort:** 5-7 days (complex async I/O)

#### Task 2: Switch Client to Beast
**File:** `src/mcp_http_abstraction.cpp` (factory)

**TDD Steps:**

1. **Update factory:**
   ```cpp
   std::unique_ptr<client_interface> create_client(
       const std::string& url,
       const client_config& config
   ) {
       #ifdef USE_BEAST_ADAPTER
       return std::make_unique<beast_client>(url, config);
       #else
       return std::make_unique<httplib_client>(url, config);
       #endif
   }
   ```

2. **Compile with Beast:**
   ```bash
   cmake -B build -DUSE_BEAST_ADAPTER=ON
   cmake --build build
   ```

3. **Run tests:**
   ```bash
   cd build && ctest -V
   ```

4. **Fix failures** - iterate

**Estimated Effort:** 2-3 days

### 6.4 Client Migration - Total Effort

**Total Time:** 3-4 weeks (15-20 working days)

**Critical Path:**
1. Week 1: Test infrastructure
2. Week 2: Migrate to abstractions (httplib adapter)
3. Week 3-4: Beast adapter implementation and switch

**Risk Factors:**
- Beast async I/O complexity
- SSE streaming edge cases
- Connection timeout handling
- SSL/TLS configuration differences

---

## 7. TDD Plan for Server Refactor

### 7.1 Prerequisites (Week 1)

**Goal:** Establish test infrastructure for server abstraction

#### Task 1: Create Server Abstraction Tests
**File:** `test/http_abstraction_test.cpp` (extend)

**Tests to Write:**
```cpp
// Server interface contract
TEST(HttpAbstraction, ServerInterfaceContract) { }
TEST(HttpAbstraction, RouteRegistration) { }
TEST(HttpAbstraction, RequestHandling) { }
TEST(HttpAbstraction, ResponseBuilding) { }

// Streaming
TEST(HttpAbstraction, StreamingContentProvider) { }
TEST(HttpAbstraction, DataSinkInterface) { }
TEST(HttpAbstraction, ChunkedTransferEncoding) { }
```

**Estimated Effort:** 1 day

#### Task 2: Create httplib Server Adapter Tests
**File:** `test/httplib_adapter_test.cpp` (extend)

**Tests to Write:**
```cpp
// Server operations
TEST(HttplibAdapter, ServerConstruction) { }
TEST(HttplibAdapter, ServerStartStop) { }
TEST(HttplibAdapter, RegisterGetRoute) { }
TEST(HttplibAdapter, RegisterPostRoute) { }
TEST(HttplibAdapter, RegisterDeleteRoute) { }
TEST(HttplibAdapter, RegisterOptionsRoute) { }

// Request routing
TEST(HttplibAdapter, RouteMatching) { }
TEST(HttplibAdapter, RouteParameters) { }
TEST(HttplibAdapter, RequestParsing) { }

// Response building
TEST(HttplibAdapter, ResponseHeaders) { }
TEST(HttplibAdapter, ResponseBody) { }
TEST(HttplibAdapter, ResponseStatus) { }

// SSE streaming
TEST(HttplibAdapter, SSEStreamingSetup) { }
TEST(HttplibAdapter, SSEDataSinkWrite) { }
TEST(HttplibAdapter, SSEStreamingContinuation) { }
TEST(HttplibAdapter, SSEStreamingTermination) { }

// Static files
TEST(HttplibAdapter, MountPoint) { }
TEST(HttplibAdapter, StaticFileServing) { }
TEST(HttplibAdapter, MountPointHeaders) { }

// SSL/TLS
TEST(HttplibAdapter, SSLServerConstruction) { }
TEST(HttplibAdapter, SSLCertificateLoading) { }
```

**Estimated Effort:** 2-3 days

### 7.2 Server Migration - Phase 1 (Week 2-3)

**Goal:** Migrate `mcp::server` to use abstractions with httplib adapter

#### Task 1: Update Server Header
**File:** `include/mcp_server.h`

**TDD Steps:**

1. **Write failing test:**
   ```cpp
   TEST(MCPServer, UsesHttpAbstraction) {
       server::configuration config;
       config.port = 8890;
       server srv(config);
       // Verify server uses abstraction
   }
   ```

2. **Update public API:**
   ```cpp
   // Add new methods (non-breaking):
   class event_dispatcher {
       // New method
       bool wait_event(mcp::http::streaming_data_sink* sink, 
                       const std::chrono::milliseconds& timeout);
       
       // Old method (mark deprecated)
       [[deprecated("Use mcp::http::streaming_data_sink instead")]]
       bool wait_event(httplib::DataSink* sink,
                       const std::chrono::milliseconds& timeout);
   };
   
   class server {
       // New method
       bool set_mount_point(const std::string& path,
                           const std::string& dir,
                           mcp::http::headers_map headers = {});
       
       // Old method (mark deprecated)
       [[deprecated("Use mcp::http::headers_map instead")]]
       bool set_mount_point(const std::string& path,
                           const std::string& dir,
                           httplib::Headers headers);
   };
   ```

3. **Update private members:**
   ```cpp
   // Before:
   std::unique_ptr<httplib::Server> http_server_;
   
   // After:
   std::unique_ptr<mcp::http::server_interface> http_server_;
   ```

4. **Run tests** - some will fail (expected)

#### Task 2: Update Server Implementation
**File:** `src/mcp_server.cpp`

**TDD Approach:** Migrate one route at a time

**Example: OPTIONS route**
1. Write test:
   ```cpp
   TEST(MCPServer, HandlesCORSPreflight) {
       server::configuration config;
       config.port = 8891;
       server srv(config);
       srv.start(false);
       
       // Send OPTIONS request
       mcp::http::client_config client_cfg;
       auto client = mcp::http::create_client("http://localhost:8891", client_cfg);
       
       mcp::http::request_data req;
       req.method = "OPTIONS";
       req.path = "/mcp";
       req.headers = {{"Origin", "http://test.com"}};
       
       auto res = client->options(req);
       
       EXPECT_EQ(res.status_code, 204);
       EXPECT_TRUE(res.headers.count("Access-Control-Allow-Origin") > 0);
   }
   ```

2. Refactor:
   ```cpp
   // Before:
   http_server_->Options(".*", [this](const httplib::Request& req, 
                                      httplib::Response& res) {
       auto origin = req.headers.find("Origin");
       if (origin != req.headers.end()) {
           res.set_header("Access-Control-Allow-Origin", origin->second);
       }
       res.status = 204;
   });
   
   // After:
   http_server_->register_options(".*", 
       [this](const mcp::http::request_data& req,
              mcp::http::response_builder& res) {
           auto origin_it = req.headers.find("Origin");
           if (origin_it != req.headers.end()) {
               res.set_header("Access-Control-Allow-Origin", origin_it->second);
           }
           res.set_status(204);
       });
   ```

3. Run test - should pass

**Example: GET route with SSE**
1. Write test:
   ```cpp
   TEST(MCPServer, HandlesSSEStreaming) {
       server::configuration config;
       config.port = 8892;
       server srv(config);
       srv.start(false);
       
       // Open SSE connection
       sse_client client("http://localhost:8892");
       EXPECT_TRUE(client.initialize("test", "1.0"));
       
       // Should receive endpoint event
       std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }
   ```

2. Refactor SSE streaming:
   ```cpp
   // Before:
   res.set_chunked_content_provider(
       "text/event-stream",
       [this, session_id](size_t offset, httplib::DataSink& sink) -> bool {
           return dispatcher->wait_event(&sink, timeout);
       });
   
   // After:
   res.set_streaming_content(
       "text/event-stream",
       [this, session_id](mcp::http::streaming_data_sink& sink) -> bool {
           return dispatcher->wait_event(&sink, timeout);
       });
   ```

3. Run test

**Repeat for all routes:**
- POST /mcp
- DELETE /mcp
- GET /sse (legacy)
- POST /message (legacy)

**Estimated Effort:** 5-7 days

#### Task 3: Update event_dispatcher
**TDD Steps:**

1. **Write adapter method:**
   ```cpp
   // Implement new method
   bool event_dispatcher::wait_event(
       mcp::http::streaming_data_sink* sink,
       const std::chrono::milliseconds& timeout
   ) {
       // New implementation using abstraction
   }
   
   // Wrap old method (backward compatibility)
   bool event_dispatcher::wait_event(
       httplib::DataSink* sink,
       const std::chrono::milliseconds& timeout
   ) {
       // Wrap httplib::DataSink in adapter, call new method
       httplib_data_sink_adapter adapter(sink);
       return wait_event(&adapter, timeout);
   }
   ```

2. **Test both methods:**
   ```cpp
   TEST(EventDispatcher, NewDataSinkInterface) { }
   TEST(EventDispatcher, OldDataSinkInterface) { }  // Deprecated but still works
   ```

**Estimated Effort:** 1 day

#### Task 4: Run Full Test Suite

```bash
cd build && ctest -V
```

**Expected:** All tests should pass (including deprecated API tests)

**Estimated Effort:** 1 day for fixing any failures

### 7.3 Server Migration - Phase 2 (Week 4-5)

**Goal:** Switch to Beast adapter

#### Task 1: Implement Beast Server Adapter
**File:** `include/mcp_http_beast_adapter.h`

**TDD Steps:**

1. **Reuse test suite:**
   ```cpp
   // test/beast_adapter_test.cpp
   // Copy all httplib server adapter tests
   TEST(BeastAdapter, ServerConstruction) { }
   TEST(BeastAdapter, ServerStartStop) { }
   // ... etc
   ```

2. **Implement `beast_server` class:**
   - Async accept loop
   - Request routing
   - Response building
   - SSE streaming with manual chunking

3. **Reference implementation:**
   - Use `test/beast_sse_proof_of_concept.cpp` as guide
   - Adapt patterns to abstraction interfaces

**Estimated Effort:** 7-10 days (complex!)

#### Task 2: Switch Server to Beast
**File:** `src/mcp_http_abstraction.cpp` (factory)

**TDD Steps:**

1. **Update factory:**
   ```cpp
   std::unique_ptr<server_interface> create_server(
       const server_config& config
   ) {
       #ifdef USE_BEAST_ADAPTER
       return std::make_unique<beast_server>(config);
       #else
       return std::make_unique<httplib_server>(config);
       #endif
   }
   ```

2. **Compile and test:**
   ```bash
   cmake -B build -DUSE_BEAST_ADAPTER=ON
   cmake --build build
   cd build && ctest -V
   ```

3. **Fix failures**

**Estimated Effort:** 3-5 days

### 7.4 Server Migration - Total Effort

**Total Time:** 5-6 weeks (25-30 working days)

**Critical Path:**
1. Week 1: Test infrastructure (3 days)
2. Week 2-3: Migrate to abstractions (7 days)
3. Week 4-5: Beast adapter (10 days)
4. Week 6: Integration and fixes (5 days)

**Risk Factors:**
- SSE streaming complexity (highest risk)
- Async I/O and threading model
- Performance degradation
- Breaking API changes
- Static file serving

---

## 8. Test Gap Analysis

### 8.1 Critical Gaps (Must Fix Before Migration)

#### Gap 1: No HTTP Abstraction Tests ❌
**Impact:** CRITICAL - Cannot validate abstraction layer works

**Required Tests:**
- Interface contract tests
- Factory function tests
- Header handling tests
- Request/response data structure tests

**Effort:** 2-3 days
**Priority:** P0 (blocker)

#### Gap 2: No httplib Adapter Tests ❌
**Impact:** CRITICAL - Cannot validate httplib wrapper works

**Required Tests:**
- Client adapter tests (18 tests)
- Server adapter tests (20 tests)
- SSE streaming tests (5 tests)
- SSL/TLS tests (3 tests)

**Effort:** 3-4 days
**Priority:** P0 (blocker)

#### Gap 3: No Beast Adapter Tests ❌
**Impact:** HIGH - Cannot validate Beast implementation

**Required Tests:**
- Copy all httplib adapter tests
- Additional async I/O tests
- Performance comparison tests

**Effort:** 2-3 days (after adapter implemented)
**Priority:** P1 (required for Phase 2)

### 8.2 High-Priority Gaps

#### Gap 4: SSE Edge Cases ⚠️
**Impact:** HIGH - Production reliability

**Missing Tests:**
- Long-running connections (>1 hour)
- Client disconnection during stream
- Server shutdown during stream
- Multiple concurrent clients (>100)
- Heartbeat timing
- Reconnection logic
- Backpressure handling

**Effort:** 3-4 days
**Priority:** P1

#### Gap 5: Error Handling ⚠️
**Impact:** MEDIUM - User experience

**Missing Tests:**
- Network disconnection mid-request
- DNS resolution failures
- Connection refused scenarios
- Timeout edge cases
- Malformed requests/responses
- Resource exhaustion

**Effort:** 2-3 days
**Priority:** P2

#### Gap 6: Performance/Load Tests ❌
**Impact:** MEDIUM - Scalability

**Missing Tests:**
- Load testing (1000 req/s)
- Concurrent client tests (100+ clients)
- Memory leak validation
- Connection pool efficiency
- CPU usage under load

**Effort:** 4-5 days
**Priority:** P2

### 8.3 Nice-to-Have Gaps

#### Gap 7: HTTP Compliance
**Impact:** LOW - Edge case handling

**Missing Tests:**
- HTTP/1.1 compliance
- Header size limits
- Keep-alive connection pooling
- Chunked encoding edge cases
- Transfer-Encoding validation

**Effort:** 2-3 days
**Priority:** P3

#### Gap 8: SSL/TLS Details
**Impact:** LOW - Security edge cases

**Missing Tests:**
- Certificate validation errors
- SSL handshake failures
- Mixed HTTP/HTTPS
- Certificate expiration
- Cipher suite negotiation

**Effort:** 2-3 days
**Priority:** P3

### 8.4 Gap Summary

| Gap | Priority | Effort | Blocker for Migration? |
|-----|----------|--------|------------------------|
| HTTP Abstraction Tests | P0 | 3 days | ✅ YES |
| httplib Adapter Tests | P0 | 4 days | ✅ YES |
| Beast Adapter Tests | P1 | 3 days | ⚠️ Phase 2 |
| SSE Edge Cases | P1 | 4 days | ⚠️ Recommended |
| Error Handling | P2 | 3 days | ❌ NO |
| Performance Tests | P2 | 5 days | ❌ NO |
| HTTP Compliance | P3 | 3 days | ❌ NO |
| SSL/TLS Details | P3 | 3 days | ❌ NO |

**Total P0 Effort:** 7 days (blocker)
**Total P0+P1 Effort:** 14 days (recommended before migration)
**Total All Gaps:** 28 days (comprehensive testing)

---

## 9. Recommendations

### 9.1 Immediate Actions (This Week)

1. **Create HTTP Abstraction Tests (P0)**
   - File: `test/http_abstraction_test.cpp`
   - ~20 tests covering all interfaces
   - 2-3 days effort
   - **BLOCKER** for migration

2. **Create httplib Adapter Tests (P0)**
   - File: `test/httplib_adapter_test.cpp` (extend)
   - ~40 tests for client + server
   - 3-4 days effort
   - **BLOCKER** for migration

3. **Validate Current httplib Adapter**
   - Run new tests with httplib adapter
   - Fix any bugs found
   - Document behavior

### 9.2 Before Client Migration (Week 2-3)

1. **Add SSE Edge Case Tests (P1)**
   - Long-running connections
   - Disconnection handling
   - Concurrent clients
   - 3-4 days effort

2. **Add Error Handling Tests (P2)**
   - Network failures
   - Timeouts
   - Invalid responses
   - 2-3 days effort

### 9.3 Before Server Migration (Week 4-5)

1. **Implement Beast Client Adapter**
   - Use test suite to validate
   - Compare with httplib adapter behavior
   - 5-7 days effort

2. **Performance Baseline**
   - Measure current performance
   - Document metrics
   - Set acceptance criteria
   - 1-2 days effort

### 9.4 After Migration (Week 6+)

1. **Remove Deprecated APIs**
   - After 1-2 releases with deprecation warnings
   - Update documentation
   - Breaking change (major version bump)

2. **Add Performance Tests**
   - Load testing
   - Concurrent clients
   - Memory profiling
   - 4-5 days effort

3. **Enhanced HTTP Compliance**
   - HTTP/1.1 edge cases
   - SSL/TLS details
   - 5-6 days effort

### 9.5 Migration Timeline Summary

```
Week 1:     HTTP Abstraction Tests + httplib Adapter Tests [P0]
Week 2-3:   Client Migration to Abstractions (httplib)
Week 4-5:   Client Migration to Beast Adapter
Week 6-7:   Server Migration to Abstractions (httplib)
Week 8-10:  Server Migration to Beast Adapter
Week 11:    Integration Testing and Fixes
Week 12:    Performance Testing and Optimization
```

**Total Estimated Time:** 10-12 weeks

### 9.6 Risk Mitigation

**Risk 1: Beast Complexity**
- Mitigation: Prototype SSE early ✅ (DONE - proof of concept works!)
- Mitigation: Use proof of concept as reference
- Mitigation: Incremental implementation with tests

**Risk 2: Breaking API Changes**
- Mitigation: Deprecation period (1-2 releases)
- Mitigation: Clear migration guide
- Mitigation: Maintain backward compatibility initially

**Risk 3: Performance Regression**
- Mitigation: Performance baseline before migration
- Mitigation: Regular benchmarking during migration
- Mitigation: Optimization phase after migration

**Risk 4: SSE Streaming Bugs**
- Mitigation: Comprehensive SSE tests (P1)
- Mitigation: Proof of concept validation ✅
- Mitigation: Gradual rollout with feature flags

**Risk 5: Test Coverage Gaps**
- Mitigation: Add P0 tests before starting (7 days)
- Mitigation: Add P1 tests during migration (4 days)
- Mitigation: Continuous testing at each step

---

## Appendix A: File Reference Matrix

| File | Type | httplib Usage | Test Coverage | Migration Priority |
|------|------|---------------|---------------|-------------------|
| `include/mcp_server.h` | Header | 9 | Indirect (via tests) | P1 (Phase 3) |
| `src/mcp_server.cpp` | Source | 22 | Good (MCP tests) | P1 (Phase 3) |
| `include/mcp_sse_client.h` | Header | 5 | Indirect | P1 (Phase 4) |
| `src/mcp_sse_client.cpp` | Source | 6 | Good (MCP tests) | P1 (Phase 4) |
| `test/http_security_test.cpp` | Test | 17 | Self-testing | P2 (update tests) |
| `test/streamable_http_transport_test.cpp` | Test | 13 | Self-testing | P2 (update tests) |
| `test/mcp_test.cpp` | Test | 8 | Self-testing | P2 (update tests) |
| `test/lifecycle_compliance_test.cpp` | Test | 4 | Self-testing | P2 (update tests) |
| `examples/agent_example.cpp` | Example | 2 | None | P3 (optional) |
| `common/httplib.h` | Library | N/A | N/A | P4 (remove) |

---

## Appendix B: Test File Template

### Template: HTTP Abstraction Test

```cpp
/**
 * @file http_abstraction_test.cpp
 * @brief Tests for HTTP abstraction layer interfaces
 */

#include <gtest/gtest.h>
#include "mcp_http_abstraction.h"

namespace mcp {
namespace http {
namespace test {

// Request data tests
TEST(HttpAbstraction, RequestDataStructure) {
    request_data req;
    req.method = "POST";
    req.path = "/test";
    req.headers = {{"Content-Type", "application/json"}};
    req.body = R"({"test": true})";
    
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.path, "/test");
    EXPECT_EQ(req.headers.at("Content-Type"), "application/json");
    EXPECT_FALSE(req.body.empty());
}

// Response builder interface
TEST(HttpAbstraction, ResponseBuilderInterface) {
    // Test implementation here
}

// Streaming data sink interface
TEST(HttpAbstraction, StreamingDataSinkInterface) {
    // Test implementation here
}

// Client interface contract
TEST(HttpAbstraction, ClientInterfaceContract) {
    // Test implementation here
}

// Server interface contract
TEST(HttpAbstraction, ServerInterfaceContract) {
    // Test implementation here
}

} // namespace test
} // namespace http
} // namespace mcp
```

### Template: Adapter Test

```cpp
/**
 * @file httplib_adapter_test.cpp
 * @brief Tests for httplib adapter implementation
 */

#include <gtest/gtest.h>
#include "mcp_http_httplib_adapter.h"
#include "mcp_server.h"  // For test server
#include <thread>

namespace mcp {
namespace http {
namespace test {

class HttplibAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start test server on unique port
        port_ = 9000 + (std::rand() % 1000);
        // ... setup code
    }
    
    void TearDown() override {
        // Cleanup
    }
    
    int port_;
};

// Client tests
TEST_F(HttplibAdapterTest, ClientConstruction) {
    std::string url = "http://localhost:" + std::to_string(port_);
    client_config config;
    auto client = std::make_unique<httplib_client>(url, config);
    
    EXPECT_NE(client, nullptr);
}

TEST_F(HttplibAdapterTest, SimpleGetRequest) {
    // Test implementation
}

// Server tests
TEST_F(HttplibAdapterTest, ServerStartStop) {
    server_config config;
    config.port = port_;
    auto server = std::make_unique<httplib_server>(config);
    
    EXPECT_TRUE(server->start(false));
    EXPECT_TRUE(server->is_running());
    
    server->stop();
    EXPECT_FALSE(server->is_running());
}

// SSE streaming tests
TEST_F(HttplibAdapterTest, SSEStreaming) {
    // Test implementation
}

} // namespace test
} // namespace http
} // namespace mcp
```

---

## Appendix C: Quick Reference

### httplib Classes Used
- `httplib::Server` - HTTP server
- `httplib::SSLServer` - HTTPS server
- `httplib::Client` - HTTP client
- `httplib::Request` - Request structure
- `httplib::Response` - Response structure
- `httplib::DataSink` - Streaming sink
- `httplib::Headers` - Header multimap
- `httplib::Result` - Result wrapper
- `httplib::Error` - Error enum

### Abstraction Equivalents
- `httplib::Server` → `mcp::http::server_interface`
- `httplib::Client` → `mcp::http::client_interface`
- `httplib::Request` → `mcp::http::request_data`
- `httplib::Response` → `mcp::http::response_builder`
- `httplib::DataSink` → `mcp::http::streaming_data_sink`
- `httplib::Headers` → `mcp::http::headers_map`
- `httplib::Result` → `mcp::http::client_result`

### Test Commands
```bash
# Run all tests
cd build && ctest -V

# Run HTTP tests only
cd build && ctest -R http -V

# Run abstraction tests
cd build && ctest -R abstraction -V

# Run adapter tests
cd build && ctest -R adapter -V

# Run with verbose output
cd build && ctest -VV
```

---

**Document End**

**Last Updated:** 2026-02-15  
**Next Review:** After Phase 1 tests complete  
**Maintained By:** cpp-mcp development team
