# httplib Usage Summary - Quick Reference

**For detailed analysis, see:** [HTTPLIB_MIGRATION_INVENTORY.md](HTTPLIB_MIGRATION_INVENTORY.md)

---

## Quick Stats

| Metric | Count |
|--------|-------|
| **Total Files Using httplib** | 10 |
| **Total httplib:: Usages** | 84 |
| **Public API Methods Exposing httplib** | 2 |
| **Critical Code Patterns** | 3 |

---

## Files by Impact Level

### 🔴 High Impact (Major Refactoring Required)

1. **src/mcp_server.cpp** (1,969 LOC)
   - 22 httplib usages
   - Server implementation with SSE streaming
   - Effort: 8-12 days

2. **src/mcp_sse_client.cpp** (624 LOC)
   - 6 httplib usages
   - Dual client pattern for JSON-RPC + SSE
   - Effort: 5-8 days

3. **include/mcp_server.h** (586 LOC)
   - 9 httplib usages
   - Public API exposure (2 methods)
   - Effort: 3-5 days

### 🟡 Medium Impact (Moderate Changes)

4. **test/streamable_http_transport_test.cpp** (~500 LOC)
   - 13 httplib usages
   - Test client updates needed
   - Effort: 2-3 days

5. **test/http_security_test.cpp** (~400 LOC)
   - 17 httplib usages
   - Test client updates needed
   - Effort: 2-3 days

### 🟢 Low Impact (Minor Changes)

6. **include/mcp_sse_client.h** - 5 usages, 1-2 days
7. **test/mcp_test.cpp** - 8 usages, 1-2 days
8. **test/lifecycle_compliance_test.cpp** - 4 usages, 1 day
9. **examples/agent_example.cpp** - 2 usages, <1 day
10. **common/httplib.h** - Library file (to be removed)

---

## Critical Code Patterns

### 1. SSE Chunked Content Provider (Server)

**Location:** `src/mcp_server.cpp:587-618, 1076-1098`

```cpp
res.set_chunked_content_provider(
    "text/event-stream",
    [](size_t offset, httplib::DataSink& sink) -> bool {
        // Write SSE data
        sink.write(data, size);
        return true;  // Continue streaming
    }
);
```

**Why Critical:**
- No direct boost::beast equivalent
- Custom implementation required
- Used for all real-time SSE communication

---

### 2. Dual Client Architecture

**Location:** `src/mcp_sse_client.cpp:26-27`

```cpp
http_client_ = std::make_unique<httplib::Client>(url);  // For POST
sse_client_ = std::make_unique<httplib::Client>(url);   // For GET/SSE
```

**Why Critical:**
- Prevents blocking issues with SSE streaming
- Requires async I/O model in boost::beast
- Architectural pattern must be preserved

---

### 3. SSE Streaming with Callback

**Location:** `src/mcp_sse_client.cpp:266-336`

```cpp
auto res = sse_client_->Get(endpoint, 
    [&](const char* data, size_t len) -> bool {
        process_chunk(data, len);
        return true;  // Continue
    }
);
```

**Why Critical:**
- Synchronous blocking pattern
- boost::beast requires async callbacks
- Significant refactoring needed

---

## Public API Surface

### Methods Exposing httplib Types

1. **`event_dispatcher::wait_event(httplib::DataSink* sink, ...)`**
   - File: `include/mcp_server.h:78`
   - Visibility: Public
   - Impact: **BREAKING CHANGE** required for migration

2. **`server::set_mount_point(..., httplib::Headers headers)`**
   - File: `include/mcp_server.h:411`
   - Visibility: Public
   - Impact: **BREAKING CHANGE** required for migration

---

## httplib Classes Used

| Class | Where Used | Purpose |
|-------|------------|---------|
| `httplib::Server` | mcp_server.cpp | HTTP server |
| `httplib::SSLServer` | mcp_server.cpp | HTTPS server (SSL enabled) |
| `httplib::Client` | mcp_sse_client.cpp, tests | HTTP client (2 instances) |
| `httplib::Request` | Server handlers | HTTP request data |
| `httplib::Response` | Server handlers | HTTP response data |
| `httplib::DataSink` | SSE streaming | Write chunked data |
| `httplib::Headers` | Multiple | HTTP headers (multimap) |
| `httplib::Result` | Client code | Response wrapper (optional-like) |

---

## httplib Methods Used

### Server Methods
- `listen(host, port)` - Start server
- `stop()` - Stop server
- `Options/Get/Post/Delete(route, handler)` - Register HTTP handlers
- `set_mount_point(path, dir, headers)` - Serve static files

### Client Methods
- `Get(endpoint)` - HTTP GET request
- `Get(endpoint, callback)` - HTTP GET with streaming callback
- `Post(endpoint, headers, body, type)` - HTTP POST request
- `set_connection_timeout(sec, usec)` - Configure timeout
- `set_read_timeout(sec, usec)` - Configure timeout
- `set_write_timeout(sec, usec)` - Configure timeout
- `set_default_headers(headers)` - Set headers for all requests
- `enable_server_certificate_verification(bool)` - SSL verification
- `set_ca_cert_path(path)` - CA certificate for SSL
- `stop()` - Stop client (abort active requests)

### Response Methods
- `set_header(key, value)` - Set response header
- `set_content(body, type)` - Set response body
- `set_chunked_content_provider(type, callback)` - SSE streaming

### DataSink Methods
- `write(data, size)` - Write data chunk

### Utility Functions
- `httplib::to_string(Error)` - Convert error enum to string

---

## Migration Complexity

### Effort Estimate (boost::beast)

| Phase | Tasks | Duration |
|-------|-------|----------|
| **Phase 1: Preparation** | Abstraction layer, expand tests | 5-7 days |
| **Phase 2: Implementation** | boost::beast server + client | 10-14 days |
| **Phase 3: Integration** | Replace, test, fix | 7-10 days |
| **Phase 4: Cleanup** | Remove httplib, optimize, docs | 2-4 days |
| **TOTAL** | | **24-35 days** |

### Risk Level: 🔴 HIGH

**Key Risks:**
- SSE streaming incompatibility
- Thread safety issues
- API breaking changes required

---

## Test Coverage Gaps

### Missing Tests for HTTP Layer

1. **HTTP Compliance**
   - ❌ Chunked transfer encoding
   - ❌ Keep-alive connections
   - ❌ Header size limits

2. **SSE Streaming**
   - ❌ Long-running connections (>1 hour)
   - ❌ Multiple concurrent clients (>100)
   - ❌ Client disconnection during stream
   - ❌ Heartbeat timing verification

3. **Error Handling**
   - ❌ Network disconnection
   - ❌ Timeout behavior
   - ❌ Malformed requests
   - ❌ Resource exhaustion

4. **SSL/TLS**
   - ❌ Certificate validation
   - ❌ SSL handshake errors
   - ❌ Mixed HTTP/HTTPS

---

## Recommendations

### ✅ If Migrating to boost::beast

1. **Create abstraction layer first** - Don't directly replace httplib
2. **Prototype SSE streaming early** - Most complex pattern
3. **Expand test coverage** - Add HTTP layer tests
4. **Plan for API breaks** - Version bump required
5. **Budget 6-9 weeks** - Based on complexity analysis

### ✅ If Staying with httplib

1. **Add abstraction layer anyway** - Future-proof
2. **Expand test coverage** - Fill gaps identified
3. **Monitor httplib project** - Track maintenance status
4. **Document vendor decision** - In case of future questions

---

## Key Findings

### Strengths of Current httplib Implementation

- ✅ Simple, easy-to-use API
- ✅ Single-header library (no build complexity)
- ✅ Works well for current scale (~10-20 concurrent clients)
- ✅ Good error handling and diagnostics
- ✅ Well-tested and stable

### Limitations Discovered

- ⚠️ Synchronous blocking I/O (limited scalability)
- ⚠️ Thread-per-connection model (high memory at scale)
- ⚠️ Public API tightly coupled to httplib types
- ⚠️ No async/await patterns
- ⚠️ Limited control over connection lifecycle

### Migration Challenges

- 🔴 SSE chunked content provider has no boost::beast equivalent
- 🔴 Dual client pattern requires careful async I/O design
- 🟡 Public API breaks required (2 methods)
- 🟡 Threading model changes (sync to async)
- 🟢 Header handling mostly straightforward

---

## Next Steps

### Option A: Migrate to boost::beast

1. **Week 1-2:** Proof of concept for SSE streaming
2. **Week 3-4:** Create abstraction layer
3. **Week 5-6:** Implement boost::beast backend
4. **Week 7-8:** Integration testing and bug fixes
5. **Week 9:** Documentation and release

### Option B: Stay with httplib

1. **Immediate:** Add abstraction layer for future-proofing
2. **Month 1:** Expand HTTP layer test coverage
3. **Ongoing:** Monitor httplib project health
4. **As needed:** Prepare migration plan if httplib abandoned

---

## References

- **Detailed Analysis:** [HTTPLIB_MIGRATION_INVENTORY.md](HTTPLIB_MIGRATION_INVENTORY.md)
- **httplib GitHub:** https://github.com/yhirose/cpp-httplib
- **boost::beast Docs:** https://www.boost.org/doc/libs/release/libs/beast/
- **MCP Specification:** https://spec.modelcontextprotocol.io/

---

**Last Updated:** 2026-02-15  
**Document Version:** 1.0
