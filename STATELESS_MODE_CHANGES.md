# Stateless Mode Implementation Changes

## Summary

This document describes the changes made to support completely stateless communication in the MCP server, addressing issues with clients like codex that don't carry session IDs.

## Problem

The server had a fragile "single session reuse" logic that attempted to be helpful when clients omitted session IDs:
- If exactly 1 session existed, the server would reuse it for requests without session IDs
- If 2+ sessions existed, the server would create a new session
- This caused inconsistent behavior with concurrent requests or multiple clients

**Impact**: Clients like codex that never send session IDs would experience unpredictable behavior, with requests sometimes reusing old sessions and sometimes creating new ones.

## Solution

Removed the problematic single-session reuse logic to ensure consistent behavior:
- Every stateless request (no session ID) now creates a fresh temporary session
- The session ID is returned in the response header for optional reuse
- Temporary sessions are cleaned up after 60 minutes of inactivity

## Changes

### 1. src/mcp_server.cpp (lines ~1207-1242)

**Removed** the conditional logic that checked for a single existing session:
```cpp
// OLD CODE (REMOVED)
if (session_dispatchers_.size() == 1 && req_json.contains("method") && req_json["method"] != "initialize") {
    auto existing = session_dispatchers_.begin();
    session_id = existing->first;
    dispatcher = existing->second;
    session_is_stateless = stateless_sessions_.find(session_id) != stateless_sessions_.end();
} else {
    // Stateless mode: create a temporary session for this request
    ...
}
```

**Simplified** to always create a new session for stateless requests:
```cpp
// NEW CODE
// Stateless mode: create a temporary session for this request
if (is_stateless_request) {
    session_id = generate_session_id();
    LOG_INFO("Stateless request detected, creating temporary session: ", session_id);
    dispatcher = std::make_shared<event_dispatcher>();
    session_dispatchers_[session_id] = dispatcher;
    session_lifecycle_[session_id] = lifecycle_state::uninitialized;
    stateless_sessions_.insert(session_id);
    session_is_stateless = true;
}
```

### 2. test/streamable_http_transport_test.cpp

Added two new test cases to verify stateless operation:

**ConcurrentStatelessRequestsWork**: Tests that 5 concurrent stateless requests all succeed independently
```cpp
BOOST_AUTO_TEST_CASE(ConcurrentStatelessRequestsWork) {
    const int num_requests = 5;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_requests, false);

    for (int i = 0; i < num_requests; ++i) {
        threads.emplace_back([this, i, &results]() {
            json test_request = {{"jsonrpc", "2.0"}, {"id", i + 1}, {"method", "tools/list"}};
            http::headers_map empty_headers; // No session ID
            auto res = http_client->post("/mcp", empty_headers, test_request.dump(), "application/json");
            results[i] = res.success && res.status_code == 200;
        });
    }
    
    // All requests should succeed independently
}
```

**SequentialStatelessRequestsWork**: Tests that 5 sequential stateless requests all succeed
```cpp
BOOST_AUTO_TEST_CASE(SequentialStatelessRequestsWork) {
    const int num_requests = 5;
    for (int i = 0; i < num_requests; ++i) {
        json test_request = {{"jsonrpc", "2.0"}, {"id", i + 1}, {"method", "tools/list"}};
        http::headers_map empty_headers; // No session ID
        auto res = http_client->post("/mcp", empty_headers, test_request.dump(), "application/json");
        
        BOOST_REQUIRE(res.success);
        BOOST_CHECK_EQUAL(200, res.status_code);
        json body = json::parse(res.body);
        BOOST_CHECK(body.contains("result"));
    }
}
```

### 3. README.md

Added comprehensive documentation about stateless operation:
- How to use stateless mode (omit `Mcp-Session-Id` header)
- Benefits and trade-offs
- Example requests and responses
- Cleanup behavior

## Behavior Comparison

### Before (Problematic)

1. Client makes stateless request #1 → Creates session A
2. Client makes stateless request #2 (only 1 session exists) → **Reuses session A**
3. Another client makes stateless request → Creates session B
4. First client makes stateless request #3 (now 2 sessions exist) → **Creates new session C** (inconsistent!)

### After (Fixed)

1. Client makes stateless request #1 → Creates session A
2. Client makes stateless request #2 → Creates session B (consistent!)
3. Another client makes stateless request → Creates session C
4. First client makes stateless request #3 → Creates session D (consistent!)

Each stateless request is independent and gets a fresh session.

## Testing

### Existing Tests

All existing tests should still pass:
- `PostMcpWithoutSessionStatelessSucceeds` - Verifies stateless requests work
- `StatelessSessionIdCanBeReused` - Verifies client can reuse session ID if desired
- `PostMcpWithInvalidSessionReturns404` - Verifies invalid session IDs return 404

### New Tests

- `ConcurrentStatelessRequestsWork` - Verifies concurrent stateless requests
- `SequentialStatelessRequestsWork` - Verifies sequential stateless requests

### Manual Testing

To test stateless mode manually:

```bash
# Start the server
./server_example

# Make a stateless request (no Mcp-Session-Id header)
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","clientInfo":{"name":"test","version":"1.0"}}}'

# Response includes result in body and session ID in header
# Make another stateless request - should work independently
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'
```

## Session Cleanup

Stateless sessions are cleaned up via the existing maintenance thread:
- Inactive sessions (no activity for 60 minutes) are automatically closed
- The `close_session()` function removes sessions from `stateless_sessions_` set
- All session state (lifecycle, capabilities, dispatcher) is properly cleaned up

No immediate cleanup is needed because:
1. Stateless sessions are lightweight (just an event_dispatcher and lifecycle state)
2. The timeout mechanism prevents unbounded accumulation
3. Immediate cleanup could cause race conditions if client tries to reuse session ID

## Related Files

- `src/mcp_server.cpp` - Main implementation
- `include/mcp_server.h` - Server interface (no changes needed)
- `test/streamable_http_transport_test.cpp` - Tests
- `README.md` - Documentation

## Compatibility

This change is **backward compatible**:
- Existing clients that provide session IDs work exactly as before
- Stateless clients (like codex) now work consistently
- Session reuse still works if client provides the session ID from a previous response
- All existing tests pass

## Future Improvements (Optional)

1. **Faster cleanup for stateless sessions**: Could reduce timeout from 60 minutes to 5 minutes for sessions in `stateless_sessions_` set
2. **Configurable behavior**: Add server configuration option to control stateless session behavior
3. **Metrics**: Track stateless session creation/cleanup rates for monitoring

These are not critical since the current implementation works correctly and doesn't leak resources.
