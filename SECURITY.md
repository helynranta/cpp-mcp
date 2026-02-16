# HTTP Transport and Tool Safety Hardening (MCP 2025-06-18)

This document describes the security features implemented for HTTP transport and tool execution safety in accordance with MCP 2025-06-18 specification.

## HTTP Transport Security

### Origin Header Validation

The server now validates the `Origin` header for POST and DELETE requests to mitigate DNS rebinding attacks. This is a critical security measure for servers that accept cross-origin requests.

#### Configuration

```cpp
mcp::server::configuration config;
config.host = "localhost";
config.port = 8080;

// Enable Origin validation (enabled by default for localhost)
config.security.validate_origin = true;

// Configure allowed origins (default includes localhost variants)
config.security.allowed_origins = {
    "http://localhost",
    "https://localhost",
    "http://127.0.0.1",
    "https://127.0.0.1"
};

mcp::server server(config);
```

#### Default Behavior

- **Origin validation is ENABLED by default** when binding to localhost
- Default allowed origins include:
  - `http://localhost` (with any port)
  - `https://localhost` (with any port)
  - `http://127.0.0.1` (with any port)
  - `https://127.0.0.1` (with any port)

#### Custom Origins

For production deployments, configure specific allowed origins:

```cpp
config.security.allowed_origins = {
    "https://myapp.example.com",
    "https://dashboard.example.com"
};
```

#### Disabling Validation (NOT RECOMMENDED)

```cpp
config.security.validate_origin = false;
```

**Warning:** Disabling Origin validation makes your server vulnerable to DNS rebinding attacks. Only disable for testing in controlled environments.

### CORS Headers

The server automatically sets appropriate CORS headers based on Origin validation:

- When validation is **enabled** and Origin is **allowed**:
  - `Access-Control-Allow-Origin: <matching-origin>`
  - `Access-Control-Allow-Credentials: true`

- When validation is **disabled**:
  - `Access-Control-Allow-Origin: *`

### Security Best Practices

1. **Always enable Origin validation** for production servers
2. **Whitelist only trusted origins** - be specific
3. **Use HTTPS** in production with proper TLS configuration
4. **Bind to localhost** for local-only services
5. **Use authentication** for sensitive operations

## Tool Execution Safety

### Tool Confirmation Framework

Tools can be marked to require user confirmation before execution. This is particularly important for:
- Destructive operations (file deletion, data modification)
- Expensive operations (API calls with costs)
- Sensitive operations (system commands, network access)

#### Marking Tools for Confirmation

```cpp
// Create a tool that requires confirmation
auto delete_tool = mcp::tool_builder("delete_file")
    .with_description("Deletes a file from the filesystem")
    .with_string_param("path", "Path to the file to delete", true)
    .with_destructive(true)  // Metadata annotation
    .with_confirmation_required(true)  // Requires user confirmation
    .build();

server.register_tool(delete_tool, [](const mcp::json& params, const std::string&) {
    std::string path = params["path"];
    // ... delete file logic ...
    return mcp::json::array({{{"type", "text"}, {"text", "File deleted"}}});
});
```

#### Setting Up Confirmation Handler

```cpp
mcp::server::configuration config;
// Enable tool confirmation checking
config.security.enable_tool_confirmation = true;

mcp::server server(config);

// Set the confirmation handler
server.set_tool_confirmation_handler(
    [](const std::string& tool_name, const mcp::json& arguments, const std::string& session_id) -> bool {
        // Implement your confirmation logic here
        // For example: prompt user, check permissions, log the request, etc.
        
        if (tool_name == "delete_file") {
            std::string path = arguments["path"];
            
            // Deny deletion of system files
            if (path.find("/etc/") == 0 || path.find("/sys/") == 0) {
                return false;
            }
            
            // Confirm with user (implementation specific)
            return ask_user_confirmation("Delete file: " + path + "?");
        }
        
        return true;  // Allow by default
    }
);
```

#### Behavior

- When `enable_tool_confirmation` is **enabled**:
  - Tools marked with `requires_confirmation = true` will invoke the handler
  - If handler returns `false`, tool execution is denied with error
  - If no handler is set, execution is denied with error
  
- When `enable_tool_confirmation` is **disabled**:
  - Tools execute without confirmation checks
  - Confirmation handlers are not called

### Trust Model for Tool Annotations

**Important:** Tool metadata annotations (`readOnly`, `destructive`, `cost`, `latency`) are considered **UNTRUSTED** metadata provided by the tool author.

#### Security Implications

1. **Do not use annotations for security decisions** - they are hints, not guarantees
2. **Validate tool behavior independently** - don't trust the `readOnly` flag alone
3. **Use confirmation handlers** for actual access control
4. **Log and audit** tool executions regardless of annotations

#### Example: Don't Trust Annotations

```cpp
// WRONG - Don't rely on readOnly annotation for security
if (tool.has_read_only && tool.read_only_value) {
    // This is NOT safe - the annotation could be incorrect
    execute_without_checks(tool);
}

// RIGHT - Use confirmation handler for security
server.set_tool_confirmation_handler([](const std::string& tool_name, ...) {
    // Implement real access controls here
    return check_user_permissions(tool_name);
});
```

## Testing

### HTTP Security Tests

Tests are provided in `test/http_security_test.cpp`:

- Origin validation with valid/invalid origins
- Localhost and 127.0.0.1 origins
- Custom allowed origins
- CORS header reflection
- DELETE endpoint protection

### Tool Safety Tests

Tests are provided in `test/tool_safety_test.cpp`:

- Tool confirmation requirements
- Confirmation handler execution
- Confirmation denial
- Tools without confirmation
- Builder API correctness

## Migration Guide

### For Existing Applications

1. **Review your server configuration**:
   - Check if you're binding to `0.0.0.0` or localhost
   - Determine which origins should be allowed

2. **Update configuration**:
   ```cpp
   config.security.validate_origin = true;
   config.security.allowed_origins = {/* your origins */};
   ```

3. **Test CORS behavior**:
   - Verify your client applications can still connect
   - Check browser console for CORS errors

4. **Implement tool confirmation** (if needed):
   - Identify destructive/sensitive tools
   - Mark them with `requires_confirmation`
   - Implement confirmation handler

### For New Applications

- Origin validation is **enabled by default**
- Default allowed origins work for localhost development
- Add custom origins for production deployments
- Use tool confirmation for sensitive operations

## References

- [MCP 2025-06-18 Specification](https://spec.modelcontextprotocol.io/)
- [OWASP DNS Rebinding Prevention](https://cheatsheetseries.owasp.org/cheatsheets/DNS_Rebinding_Prevention_Cheat_Sheet.html)
- [MDN CORS Documentation](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS)
