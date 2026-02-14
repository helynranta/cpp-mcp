# MCP Library Update: Technical Architecture Document

## Executive Summary

This document provides technical details for updating the cpp-mcp library from the 2024-11-05 MCP specification to the 2025-03-26 specification. It serves as a reference for developers implementing the changes.

## Table of Contents

1. [Current Architecture Analysis](#current-architecture-analysis)
2. [Target Architecture](#target-architecture)
3. [Core Protocol Changes](#core-protocol-changes)
4. [Transport Layer Updates](#transport-layer-updates)
5. [Authentication & Security](#authentication--security)
6. [Data Structures](#data-structures)
7. [API Changes](#api-changes)
8. [Migration Strategy](#migration-strategy)

---

## Current Architecture Analysis

### Current Components (2024-11-05)

```
cpp-mcp (2024-11-05)
├── Core Protocol
│   ├── mcp_message.h/cpp      - JSON-RPC 2.0 messages
│   ├── mcp_client.h           - Abstract client interface
│   └── mcp_server.h/cpp       - Server implementation
├── Transports
│   ├── mcp_sse_client.h/cpp   - HTTP + SSE transport (PRIMARY)
│   └── mcp_stdio_client.h/cpp - Stdio transport
├── Features
│   ├── mcp_tool.h/cpp         - Tool management
│   ├── mcp_resource.h/cpp     - Resource management
│   └── mcp_logger.h           - Logging utilities
└── Infrastructure
    └── mcp_thread_pool.h      - Thread pool
```

### Current Limitations

1. **Transport:** SSE is unidirectional, requires dual endpoints
2. **Security:** Basic OAuth 2.0, no PKCE, HTTP allowed
3. **Session:** No formal session management
4. **Metadata:** Limited tool/resource metadata
5. **Progress:** Numeric-only progress tracking
6. **Content:** Text and images only

---

## Target Architecture

### New Components (2025-03-26)

```
cpp-mcp (2025-03-26)
├── Core Protocol
│   ├── mcp_message.h/cpp      - Enhanced JSON-RPC 2.0
│   ├── mcp_client.h           - Abstract client interface
│   ├── mcp_server.h/cpp       - Enhanced server
│   └── mcp_session.h/cpp      - NEW: Session management
├── Transports
│   ├── mcp_streamable_http_client.h/cpp - NEW: Primary transport
│   ├── mcp_sse_client.h/cpp   - Optional fallback
│   └── mcp_stdio_client.h/cpp - Stdio transport
├── Features
│   ├── mcp_tool.h/cpp         - Enhanced with annotations
│   ├── mcp_resource.h/cpp     - Enhanced with audio
│   ├── mcp_progress.h/cpp     - NEW: Rich progress tracking
│   └── mcp_logger.h           - Logging utilities
├── Security
│   ├── mcp_oauth.h/cpp        - NEW: OAuth 2.1 + PKCE
│   └── mcp_auth.h/cpp         - NEW: RBAC & multi-user
└── Infrastructure
    └── mcp_thread_pool.h      - Thread pool
```

---

## Core Protocol Changes

### 1. Version Constant

**Before (2024-11-05):**
```cpp
constexpr const char* MCP_VERSION = "2024-11-05";
```

**After (2025-03-26):**
```cpp
constexpr const char* MCP_VERSION = "2025-03-26";
```

### 2. Message Structure Enhancements

**Session ID Support:**
```cpp
// Add to request/response structures
struct request {
    std::string jsonrpc = "2.0";
    json id;
    std::string method;
    json params;
    std::optional<std::string> session_id;  // NEW
    
    // Header conversion for HTTP
    std::map<std::string, std::string> to_headers() const {
        std::map<std::string, std::string> headers;
        if (session_id) {
            headers["Mcp-Session-Id"] = *session_id;
        }
        return headers;
    }
};
```

### 3. Enhanced Error Codes

```cpp
enum class error_code {
    // Existing codes
    parse_error = -32700,
    invalid_request = -32600,
    method_not_found = -32601,
    invalid_params = -32602,
    internal_error = -32603,
    
    // NEW: Extended error codes
    unauthorized = -32001,        // Authentication required
    forbidden = -32002,           // Insufficient permissions
    resource_not_found = -32003,  // Resource doesn't exist
    rate_limited = -32004,        // Rate limit exceeded
    session_expired = -32005,     // Session no longer valid
    
    server_error_start = -32000,
    server_error_end = -32099
};
```

---

## Transport Layer Updates

### Streamable HTTP Transport

**Key Features:**
- Bidirectional communication
- Connection recovery
- Stream multiplexing
- Lower overhead than SSE

**Implementation Outline:**

```cpp
// include/mcp_streamable_http_client.h

class streamable_http_client : public client {
public:
    streamable_http_client(const std::string& base_url);
    
    // Bidirectional streaming
    bool open_stream();
    void close_stream();
    
    // Send/receive on stream
    void send_message(const json& message);
    json receive_message();
    
    // Connection recovery
    bool reconnect();
    void set_reconnect_handler(std::function<void()> handler);
    
    // Override client interface
    bool initialize(const std::string& name, const std::string& version) override;
    response send_request(const std::string& method, const json& params) override;
    
private:
    struct stream_state {
        bool connected;
        std::string stream_id;
        std::chrono::steady_clock::time_point last_activity;
    };
    
    stream_state state_;
    std::unique_ptr<httplib::Client> http_client_;
    
    // Stream management
    void maintain_connection();
    void handle_disconnect();
};
```

**Protocol Flow:**

```
Client                              Server
  |                                    |
  |-- HTTP POST /stream (init) ------>|
  |<-------- 200 OK (stream-id) ------|
  |                                    |
  |-- bidirectional messages -------->|
  |<----------------------------------|
  |                                    |
  |-- (disconnect) ------------------->|
  |                                    |
  |-- HTTP POST /stream (reconnect) ->|
  |    (with stream-id)                |
  |<-------- 200 OK (restored) -------|
```

### SSE Transport Deprecation

- Keep SSE client as optional fallback
- Mark as deprecated in documentation
- Add warning when using SSE in new projects
- Maintain for backward compatibility

```cpp
// Mark deprecated
[[deprecated("SSE transport is deprecated. Use streamable_http_client instead.")]]
class sse_client : public client {
    // Existing implementation
};
```

---

## Authentication & Security

### OAuth 2.1 with PKCE

**Implementation Components:**

```cpp
// include/mcp_oauth.h

class oauth_client {
public:
    // PKCE flow
    struct pkce_params {
        std::string code_verifier;
        std::string code_challenge;
        std::string code_challenge_method;  // "S256"
    };
    
    // Generate PKCE parameters
    static pkce_params generate_pkce();
    
    // Authorization URL with PKCE
    std::string get_authorization_url(
        const std::string& client_id,
        const std::string& redirect_uri,
        const std::vector<std::string>& scopes,
        const pkce_params& pkce
    );
    
    // Exchange code for token (with verifier)
    struct token_response {
        std::string access_token;
        std::string refresh_token;
        std::string token_type;
        int expires_in;
        std::vector<std::string> scope;
    };
    
    token_response exchange_code(
        const std::string& code,
        const std::string& redirect_uri,
        const pkce_params& pkce
    );
    
    // Token refresh
    token_response refresh_token(const std::string& refresh_token);
    
    // Token binding
    void bind_token(const std::string& token, const std::string& server_id);
    
private:
    std::string auth_endpoint_;
    std::string token_endpoint_;
    std::string client_id_;
    
    // Secure token storage
    class token_store;
    std::unique_ptr<token_store> token_store_;
};
```

**PKCE Implementation:**

```cpp
// src/mcp_oauth.cpp

oauth_client::pkce_params oauth_client::generate_pkce() {
    pkce_params params;
    
    // Generate code verifier (43-128 chars, URL-safe)
    std::vector<unsigned char> random_bytes(32);
    // Use secure random generator (e.g., OpenSSL RAND_bytes)
    RAND_bytes(random_bytes.data(), random_bytes.size());
    
    params.code_verifier = base64_url_encode(random_bytes);
    
    // Generate code challenge: SHA256(code_verifier)
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(
        reinterpret_cast<const unsigned char*>(params.code_verifier.data()),
        params.code_verifier.size(),
        hash
    );
    
    params.code_challenge = base64_url_encode(
        std::vector<unsigned char>(hash, hash + SHA256_DIGEST_LENGTH)
    );
    params.code_challenge_method = "S256";
    
    return params;
}
```

### HTTPS Enforcement

```cpp
// Update client constructors to validate HTTPS

class streamable_http_client : public client {
public:
    streamable_http_client(const std::string& base_url) {
        // Parse URL
        if (base_url.substr(0, 5) != "https") {
            // In production mode, reject HTTP
            #ifdef MCP_PRODUCTION
            throw mcp_exception(
                error_code::invalid_params,
                "HTTPS is required in production mode"
            );
            #else
            // In development, warn
            mcp::logger::warn(
                "Using HTTP is insecure and deprecated. "
                "Use HTTPS in production."
            );
            #endif
        }
        // Continue initialization...
    }
};
```

---

## Data Structures

### Enhanced Tool Schema

**Before (2024-11-05):**
```cpp
struct tool {
    std::string name;
    std::string description;
    json input_schema;
};
```

**After (2025-03-26):**
```cpp
struct tool {
    std::string name;
    std::string description;
    json input_schema;
    
    // NEW: Annotations
    struct annotations {
        bool read_only = false;       // Tool only reads data
        bool destructive = false;     // Tool modifies/deletes data
        bool requires_auth = false;   // Requires authentication
        std::optional<double> cost_estimate;  // Estimated cost
        std::optional<int> latency_ms;        // Estimated latency
        std::string icon_url;                  // Display icon
    } annotations;
    
    // NEW: Output schema
    std::optional<json> output_schema;
    
    // NEW: Parameter completion
    std::map<std::string, std::function<std::vector<std::string>()>> completions;
};
```

### Tool Builder Updates

```cpp
class tool_builder {
public:
    tool_builder(const std::string& name);
    
    tool_builder& with_description(const std::string& desc);
    tool_builder& with_input_schema(const json& schema);
    
    // NEW: Annotation methods
    tool_builder& as_read_only();
    tool_builder& as_destructive();
    tool_builder& requires_authentication();
    tool_builder& with_cost_estimate(double cost);
    tool_builder& with_latency_estimate(int ms);
    tool_builder& with_icon(const std::string& url);
    
    // NEW: Output schema
    tool_builder& with_output_schema(const json& schema);
    
    // NEW: Parameter completion
    tool_builder& with_completion(
        const std::string& param_name,
        std::function<std::vector<std::string>()> completion_fn
    );
    
    tool build();
};
```

### Session Management

```cpp
// include/mcp_session.h

class session {
public:
    session(const std::string& session_id);
    
    std::string id() const;
    
    // Session state
    void set_state(const std::string& key, const json& value);
    json get_state(const std::string& key) const;
    
    // Session lifecycle
    bool is_expired() const;
    void extend();
    void invalidate();
    
    // User association
    void set_user(const std::string& user_id);
    std::string get_user() const;
    
private:
    std::string id_;
    std::map<std::string, json> state_;
    std::chrono::steady_clock::time_point created_at_;
    std::chrono::steady_clock::time_point last_activity_;
    std::chrono::seconds timeout_{3600};  // 1 hour default
    std::optional<std::string> user_id_;
};

class session_manager {
public:
    // Session creation
    std::shared_ptr<session> create_session();
    std::shared_ptr<session> get_session(const std::string& id);
    
    // Session recovery
    bool can_recover(const std::string& id) const;
    std::shared_ptr<session> recover_session(const std::string& id);
    
    // Cleanup
    void remove_expired_sessions();
    
private:
    std::map<std::string, std::shared_ptr<session>> sessions_;
    std::mutex mutex_;
};
```

### Progress Notifications

```cpp
// include/mcp_progress.h

struct progress_notification {
    std::string operation_id;
    
    // Progress indicators
    std::optional<double> percent;  // 0.0 - 100.0
    std::optional<int> current;     // Current item
    std::optional<int> total;       // Total items
    
    // NEW: Text status
    std::string status;             // Human-readable status
    std::string phase;              // Current phase
    
    // Metadata
    std::chrono::steady_clock::time_point timestamp;
    bool cancellable = false;
    
    // Conversion to JSON-RPC notification
    json to_notification() const;
};

class progress_tracker {
public:
    progress_tracker(const std::string& operation_id);
    
    // Update progress
    void update(double percent, const std::string& status);
    void update_phase(const std::string& phase);
    void update_items(int current, int total);
    
    // Completion
    void complete(const std::string& message);
    void fail(const std::string& error);
    
    // Cancellation
    void set_cancellable(bool cancellable);
    bool is_cancelled() const;
    
private:
    std::string operation_id_;
    std::function<void(const progress_notification&)> callback_;
    std::atomic<bool> cancelled_{false};
};
```

### Audio Support

```cpp
// Update resource and tool content types

struct content {
    std::string type;  // "text", "image", "audio"
    
    // Existing
    std::optional<std::string> text;
    std::optional<std::string> image_data;  // base64
    
    // NEW: Audio support
    struct audio {
        std::string format;        // "mp3", "wav", "ogg", etc.
        std::string encoding;      // "base64", "url"
        std::string data;          // Actual data or URL
        int sample_rate = 44100;
        int channels = 2;
        std::optional<int> duration_ms;
    };
    std::optional<audio> audio_data;
    
    // Metadata
    std::optional<std::string> mime_type;
};
```

---

## API Changes

### Client Interface Updates

**New Methods:**
```cpp
class client {
public:
    // Existing methods...
    
    // NEW: Session management
    virtual std::string get_session_id() const = 0;
    virtual void set_session_id(const std::string& id) = 0;
    
    // NEW: Progress subscription
    virtual void subscribe_to_progress(
        const std::string& operation_id,
        std::function<void(const progress_notification&)> callback
    ) = 0;
    
    // NEW: Parameter completion
    virtual std::vector<std::string> get_completions(
        const std::string& tool_name,
        const std::string& param_name,
        const json& partial_params
    ) = 0;
    
    // NEW: Multi-user
    virtual void set_user_context(const std::string& user_id) = 0;
};
```

### Server Interface Updates

**New Methods:**
```cpp
class server {
public:
    // Existing methods...
    
    // NEW: Session management
    void enable_session_management(bool enabled = true);
    void set_session_timeout(std::chrono::seconds timeout);
    
    // NEW: Progress notifications
    void send_progress(
        const std::string& session_id,
        const progress_notification& progress
    );
    
    // NEW: Multi-user support
    void set_user_auth_handler(
        std::function<bool(const std::string& user_id, const std::string& token)> handler
    );
    
    void set_permission_handler(
        std::function<bool(const std::string& user_id, const std::string& resource)> handler
    );
};
```

---

## Migration Strategy

### Phase 1: Backward Compatible Additions

1. Add new components without breaking existing code
2. Use feature flags for new functionality
3. Keep old APIs working alongside new ones

```cpp
// Example: Feature flags
#define MCP_FEATURE_STREAMABLE_HTTP
#define MCP_FEATURE_SESSION_MANAGEMENT
#define MCP_FEATURE_OAUTH21
```

### Phase 2: Deprecation Warnings

1. Mark old APIs as deprecated
2. Update documentation with migration guides
3. Add runtime warnings for deprecated usage

```cpp
[[deprecated("Use streamable_http_client instead")]]
class sse_client : public client { /* ... */ };
```

### Phase 3: Migration Period

1. Support both old and new APIs for at least 6 months
2. Provide migration tools/scripts where possible
3. Offer detailed upgrade guides

### Phase 4: Removal

1. Remove deprecated features in major version bump
2. Clean up feature flags
3. Simplify codebase

---

## Testing Strategy

### Unit Tests

```cpp
// Test OAuth 2.1 PKCE
TEST(OAuth, PKCEGeneration) {
    auto pkce = oauth_client::generate_pkce();
    
    ASSERT_FALSE(pkce.code_verifier.empty());
    ASSERT_FALSE(pkce.code_challenge.empty());
    ASSERT_EQ(pkce.code_challenge_method, "S256");
    
    // Verify code challenge is SHA256(verifier)
    // ...
}

// Test session management
TEST(Session, CreateAndRecover) {
    session_manager mgr;
    
    auto session1 = mgr.create_session();
    std::string id = session1->id();
    
    session1->set_state("key", "value");
    
    // Recover session
    auto session2 = mgr.recover_session(id);
    ASSERT_NE(session2, nullptr);
    ASSERT_EQ(session2->get_state("key"), "value");
}
```

### Integration Tests

```cpp
// Test full client-server flow with new protocol
TEST(Integration, StreamableHTTPFlow) {
    // Start server with new transport
    server srv(config);
    srv.enable_session_management();
    srv.start();
    
    // Connect client
    streamable_http_client client("https://localhost:8888");
    ASSERT_TRUE(client.initialize("test", "1.0"));
    
    // Verify session
    ASSERT_FALSE(client.get_session_id().empty());
    
    // Call tool
    auto result = client.call_tool("echo", {{"text", "hello"}});
    ASSERT_TRUE(result.is_object());
}
```

### Compatibility Tests

```cpp
// Test with real Copilot/Claude Code instances
TEST(Compatibility, GitHubCopilot) {
    // Set up server compatible with Copilot
    // Connect Copilot client
    // Verify all features work
}

TEST(Compatibility, ClaudeCode) {
    // Set up server compatible with Claude
    // Connect Claude client
    // Verify all features work
}
```

---

## Performance Considerations

### Streamable HTTP vs SSE

**Expected Improvements:**
- 30-50% reduction in latency for bidirectional operations
- Better connection reuse
- Lower memory overhead
- Faster reconnection

### Session Management Overhead

**Optimization Strategies:**
- Use memory-efficient session storage
- Implement LRU cache for active sessions
- Periodic cleanup of expired sessions
- Consider Redis for distributed deployments

### OAuth Token Handling

**Security vs Performance:**
- Cache tokens securely in memory
- Implement token refresh in background
- Use short-lived tokens (15 min) with refresh tokens
- Encrypt tokens at rest

---

## Security Checklist

- [ ] HTTPS enforced for production
- [ ] OAuth 2.1 with PKCE implemented
- [ ] Implicit flow removed
- [ ] Token binding implemented
- [ ] Secure token storage
- [ ] Session timeout enforced
- [ ] RBAC for multi-user scenarios
- [ ] Input validation on all parameters
- [ ] Output sanitization
- [ ] Rate limiting implemented
- [ ] Audit logging for sensitive operations

---

## References

1. [MCP Specification 2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26)
2. [OAuth 2.1 Draft](https://datatracker.ietf.org/doc/html/draft-ietf-oauth-v2-1-07)
3. [PKCE RFC 7636](https://datatracker.ietf.org/doc/html/rfc7636)
4. [Streamable HTTP](https://modelcontextprotocol.io/specification/2025-03-26/transport/http)

---

## Appendix A: Code Examples

See `examples/` directory for:
- OAuth 2.1 with PKCE example
- Streamable HTTP client example
- Session management example
- Progress tracking example
- Multi-user server example

## Appendix B: API Reference

Full API reference will be generated from code documentation after implementation.

## Appendix C: Change Log

Track all changes during implementation:

```markdown
### [2025-03-26] - TBD
#### Added
- Streamable HTTP transport
- OAuth 2.1 with PKCE
- Session management
- Tool annotations
- Progress notifications
- Audio support
- Parameter completion
- Multi-user support

#### Changed
- Protocol version to 2025-03-26
- SSE marked as deprecated
- Enhanced error codes

#### Removed
- OAuth implicit flow
- Old progress format
```
