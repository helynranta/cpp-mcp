# httplib to Boost.Beast Migration Plan

## Status: IN PROGRESS

## Overview

This document outlines the plan for migrating cpp-mcp from cpp-httplib to Boost.Beast for HTTP server and client functionality. This migration is necessary to align with modern C++ networking practices and reduce dependency on single-header libraries.

## Scope

**Files to Migrate:**
- `include/mcp_server.h` - Server interface (9 httplib usages)
- `include/mcp_sse_client.h` - Client interface (5 httplib usages)
- `src/mcp_server.cpp` - Server implementation (22 httplib usages)
- `src/mcp_sse_client.cpp` - Client implementation (6 httplib usages)
- `common/httplib.h` - To be removed

**Estimated Effort:** 24-35 working days (per HTTPLIB_MIGRATION_INVENTORY.md)

## Migration Strategy

### Why This Is Complex

1. **Architectural Difference:**
   - httplib: Synchronous, blocking I/O with simple callbacks
   - Boost.Beast: Async I/O requiring io_context, coroutines, or callbacks

2. **SSE Streaming:**
   - httplib: `set_chunked_content_provider()` with simple callback
   - Boost.Beast: Manual chunked encoding with async write operations

3. **Dual Client Pattern:**
   - httplib: Two `httplib::Client` instances (POST + SSE)
   - Boost.Beast: Requires separate connections with async I/O coordination

4. **Public API Changes:**
   - `event_dispatcher::wait_event(httplib::DataSink*)` - Breaking change required
   - `server::set_mount_point(..., httplib::Headers)` - Breaking change required

### Recommended Approach

**OPTION A: Direct Replacement (NOT RECOMMENDED)**
- Replace httplib with beast directly
- Pros: Clean migration
- Cons: Requires complete rewrite, breaks everything during migration

**OPTION B: Abstraction Layer (RECOMMENDED)**
- Create HTTP abstraction layer
- Implement httplib adapter (wraps existing code)
- Implement beast adapter (new code)
- Migrate incrementally
- Pros: Testable, incremental, doesn't break existing code
- Cons: More initial work, temporary code duplication

**OPTION C: Hybrid Approach**
- Keep httplib for client, migrate server only
- Pros: Reduced risk, faster
- Cons: Mixed dependencies

**DECISION: Following Option B (Abstraction Layer)**

## Implementation Phases

### Phase 1: Abstraction Layer (Week 1-2)

**Goal:** Create HTTP abstractions that hide library-specific details

**Tasks:**
1. Create `include/mcp_http_abstraction.h`:
   - `http_request` interface
   - `http_response` interface
   - `streaming_sink` interface
   - `http_server` interface
   - `http_client` interface

2. Create `src/mcp_http_httplib_adapter.cpp`:
   - Implement abstractions using httplib
   - Wrap existing `httplib::Server`, `httplib::Client`
   - No behavior changes, just wrapping

3. Add tests:
   - `test/http_abstraction_test.cpp`
   - Verify httplib adapter works correctly
   - Test all HTTP operations

**Deliverables:**
- Working abstraction layer
- All tests passing with httplib adapter
- No changes to existing MCP code yet

### Phase 2: Boost.Beast Implementation (Week 3-5)

**Goal:** Implement HTTP abstractions using Boost.Beast

**Tasks:**
1. Create `src/mcp_http_beast_adapter.cpp`:
   - Implement `http_server` using Beast
   - Implement `http_client` using Beast
   - Handle SSE streaming with chunked encoding
   - Manage io_context and threading

2. Implement Beast server:
   - Async accept loop
   - Request routing
   - SSE chunked streaming
   - SSL/TLS support

3. Implement Beast client:
   - Async request/response
   - SSE streaming with callback
   - Dual connection pattern
   - SSL/TLS support

4. Add tests:
   - `test/beast_adapter_test.cpp`
   - Verify Beast adapter works correctly
   - Compare behavior with httplib adapter

**Deliverables:**
- Working Beast adapter
- All HTTP abstraction tests passing with both adapters
- Still no changes to existing MCP code

### Phase 3: MCP Server Migration (Week 6-7)

**Goal:** Migrate `mcp_server` to use HTTP abstractions

**Tasks:**
1. Update `include/mcp_server.h`:
   - Change `httplib::DataSink*` to `http::streaming_sink*`
   - Change `httplib::Headers` to `http::headers_t`
   - Replace `std::unique_ptr<httplib::Server>` with `std::unique_ptr<http::http_server>`

2. Update `src/mcp_server.cpp`:
   - Use HTTP abstractions instead of httplib directly
   - Update all request/response handling
   - Update SSE streaming code

3. Test with httplib adapter first:
   - Ensure all existing tests pass
   - Verify no behavior changes

4. Switch to Beast adapter:
   - Change factory to use Beast
   - Run all tests
   - Fix any issues

**Deliverables:**
- MCP server using HTTP abstractions
- All server tests passing with Beast adapter
- Breaking API changes documented

### Phase 4: MCP Client Migration (Week 8-9)

**Goal:** Migrate `mcp_sse_client` to use HTTP abstractions

**Tasks:**
1. Update `include/mcp_sse_client.h`:
   - Replace `httplib::Client` with `http::http_client`
   - Update method signatures

2. Update `src/mcp_sse_client.cpp`:
   - Use HTTP abstractions
   - Update request/response handling
   - Update SSE streaming

3. Test with httplib adapter first
4. Switch to Beast adapter

**Deliverables:**
- MCP client using HTTP abstractions
- All client tests passing with Beast adapter

### Phase 5: Final Migration (Week 10)

**Goal:** Remove httplib completely

**Tasks:**
1. Remove httplib adapter code
2. Remove `common/httplib.h`
3. Update `CMakeLists.txt`:
   - Remove httplib include directory
   - Remove httplib-specific flags
4. Update tests
5. Update examples
6. Update documentation

**Deliverables:**
- No httplib dependencies
- All tests passing
- Updated documentation

## Testing Strategy

### Test Coverage

**Existing Tests to Maintain:**
- All current MCP protocol tests
- Server/client integration tests
- SSE streaming tests
- Security tests

**New Tests to Add:**
- HTTP abstraction layer tests
- httplib adapter tests
- Beast adapter tests
- Migration compatibility tests

### Test Approach

1. **Phase 1:** Test abstraction layer with httplib adapter
2. **Phase 2:** Test abstraction layer with Beast adapter
3. **Phase 3:** Test MCP server with both adapters
4. **Phase 4:** Test MCP client with both adapters
5. **Phase 5:** Final integration testing

## Risks and Mitigations

**Risk:** Beast async I/O complexity
- Mitigation: Start with simple synchronous wrapper, migrate to async later

**Risk:** SSE streaming incompatibility
- Mitigation: Prototype SSE early, validate with real clients

**Risk:** Performance degradation
- Mitigation: Benchmark before/after, optimize as needed

**Risk:** Breaking API changes
- Mitigation: Version bump, migration guide, deprecation period

**Risk:** Threading model changes
- Mitigation: Careful design of io_context lifecycle, thread safety

## API Breaking Changes

### Required Changes

**1. event_dispatcher::wait_event()**
```cpp
// Before:
bool wait_event(httplib::DataSink* sink, ...);

// After:
bool wait_event(http::streaming_sink* sink, ...);
```

**2. server::set_mount_point()**
```cpp
// Before:
bool set_mount_point(const std::string& path, const std::string& dir, 
                    httplib::Headers headers = httplib::Headers());

// After:
bool set_mount_point(const std::string& path, const std::string& dir,
                    http::headers_t headers = http::headers_t());
```

### Deprecation Strategy

1. Add new methods with `http::` types
2. Mark old methods as `[[deprecated]]`
3. Maintain both for 1-2 releases
4. Remove old methods in major version bump

## Current Status

### Completed

- ✅ Dependency analysis (HTTPLIB_MIGRATION_INVENTORY.md)
- ✅ Boost.Beast integration via vcpkg
- ✅ Migration plan created

### In Progress

- 🔄 Phase 1: HTTP abstraction layer design

### Not Started

- ⬜ Phase 1: Implementation
- ⬜ Phase 2: Beast adapter
- ⬜ Phase 3: Server migration
- ⬜ Phase 4: Client migration
- ⬜ Phase 5: Final cleanup

## Next Steps

1. Create HTTP abstraction layer interfaces
2. Implement httplib adapter
3. Add tests for abstraction layer
4. Prototype SSE streaming with Beast
5. Continue with Phase 2

## Timeline

**Optimistic:** 24 working days (~ 5 weeks)
**Realistic:** 35 working days (~ 7 weeks)
**Pessimistic:** 45 working days (~ 9 weeks)

**Assumptions:**
- 1 experienced C++ developer
- Familiarity with Boost.Beast and Boost.Asio
- Existing test infrastructure
- No major scope changes

## References

- [HTTPLIB_MIGRATION_INVENTORY.md](./HTTPLIB_MIGRATION_INVENTORY.md) - Detailed usage analysis
- [HTTPLIB_USAGE_SUMMARY.md](./HTTPLIB_USAGE_SUMMARY.md) - Quick reference
- [Boost.Beast Documentation](https://www.boost.org/doc/libs/release/libs/beast/)
- [MCP Specification](https://spec.modelcontextprotocol.io/)

## Notes

This is a significant migration that will take several weeks. The key to success is:
1. Incremental approach via abstraction layer
2. Comprehensive testing at each phase
3. Careful handling of SSE streaming
4. Clear communication of API changes

---

**Last Updated:** 2026-02-15
**Status:** Planning / Phase 1 Design
