# Changelog

All notable changes to the cpp-mcp implementation are documented in this file.

This project adheres to the [Model Context Protocol (MCP) specification](https://modelcontextprotocol.io/specification/latest/).

## [2025.11.25] - 2026-02-17

### Changed
- **Protocol Version**: Updated to claim conformance with MCP 2025-11-25 specification
- **Documentation**: Enhanced conformance documentation with links to latest specification
- **Security**: Added prominent disclaimers about lack of authentication/authorization implementation

### Notes
This version claims conformance with MCP 2025-11-25 while maintaining the core implementation from 2025-06-18. The 2025-11-25 specification primarily adds optional extensions support, which is prepared for but not yet utilized.

## [2025.06.18] - 2026-02-16

### Major Protocol Upgrades from 2025-03-26

This release implements comprehensive upgrades to conform with the MCP 2025-06-18 specification, representing a major protocol evolution.

#### Breaking Changes

1. **JSON-RPC Batch Removal** (BREAKING)
   - **Removed**: JSON-RPC batch request support (arrays of requests)
   - **Rationale**: Batch operations were removed from the MCP specification
   - **Impact**: Servers now reject batch request arrays with error code `-32600`
   - **Migration**: Send individual requests instead of batched arrays
   - **Tests**: 4 comprehensive rejection tests validate proper error handling

2. **MCP-Protocol-Version Header** (REQUIRED)
   - **Added**: Required `MCP-Protocol-Version` header for all HTTP requests after initialization
   - **Validation**: Server validates header and returns 400 Bad Request for invalid versions
   - **Negotiation**: Version is negotiated during initialization and must match in subsequent requests
   - **Backward Compatibility**: Missing headers default to 2025-03-26 for compatibility
   - **Supported Versions**: `2025-03-26`, `2025-06-18`, `2025-11-25`
   - **Tests**: 8 comprehensive tests covering validation, negotiation, and multi-client scenarios

#### Major Features

3. **Structured Tool Output Schema**
   - **Added**: Optional `title` and `outputSchema` fields to tool definitions
   - **Purpose**: Enable typed tool outputs for better LLM integration
   - **Result Format**: Tool results can include `structuredContent` alongside text content
   - **Schema**: JSON Schema validation for structured outputs
   - **Backward Compatible**: Text-only content still fully supported
   - **Tests**: 15 comprehensive tests including nested structures and arrays
   - **Example**: `examples/structured_tool_example.cpp`

4. **Elicitation (Human-in-the-Loop)** (NEW CAPABILITY)
   - **Added**: Server-to-client request mechanism for user input during tool execution
   - **Use Cases**: Confirmation dialogs, form input, interactive workflows
   - **Implementation**: Complete async request-response flow with promise/future mechanism
   - **Actions**: Support for accept, decline, and cancel responses
   - **Timeout**: Configurable timeout handling for elicitation requests
   - **Tests**: 23 tests (15 data structure + 8 integration)
   - **Example**: `examples/elicitation_example.cpp`

5. **Completion Support** (NEW CAPABILITY)
   - **Added**: Argument autocompletion for prompts and resource templates
   - **Context**: Support for previously-resolved variables via `context` field
   - **Metadata**: Extensible metadata via `_meta` field (cache info, timestamps, sources)
   - **Use Cases**: Smart argument completion, template variable suggestions
   - **Tests**: 17 comprehensive tests
   - **Example**: `examples/completion_example.cpp`

6. **Resource Links**
   - **Added**: New `resource_link` content type for tool results
   - **Fields**: Required `uri` and `type`, optional `name`, `description`, `mimeType`, `annotations`
   - **Purpose**: Allow tools to reference external resources (files, URLs, APIs)
   - **Backward Compatible**: Part of content array alongside text and image content

#### Security Enhancements

7. **HTTP Transport Security**
   - **Origin Validation**: Validates `Origin` header for POST/DELETE requests
   - **DNS Rebinding Mitigation**: Prevents DNS rebinding attacks
   - **Default Behavior**: Enabled by default for localhost bindings
   - **Configurable Origins**: Support for custom allowed origins list
   - **CORS Handling**: Secure CORS with origin reflection

8. **Tool Execution Safety**
   - **Confirmation Hooks**: Optional user confirmation for sensitive tools
   - **Trust Model**: Tool annotations treated as untrusted metadata
   - **Execution Policies**: Configurable tool execution policies
   - **Security Documentation**: Comprehensive security guide in SECURITY.md

#### Testing and Conformance

9. **Enhanced Test Coverage**
   - **Total Tests**: 201+ tests with 100% pass rate
   - **Test Categories**:
     - Batch rejection: 4 tests
     - Protocol version header: 8 tests
     - Structured tool output: 15 tests
     - Elicitation: 23 tests (15 data + 8 integration)
     - Completion: 17 tests
     - HTTP security: Multiple security-focused test suites
   - **Test Frameworks**: Migrated to Boost.Test for all testing

10. **Official Conformance Testing**
    - **Framework**: Integrated with official MCP conformance suite (v0.1.11)
    - **Scenarios**: 29 server scenarios tested
    - **Coverage**: 21/29 fully passing (72%), 25/29 implemented (86%)
    - **CI Integration**: GitHub Actions workflow runs conformance tests on every PR
    - **Documentation**: Complete conformance documentation in CONFORMANCE.md and CONFORMANCE_TESTING.md

#### Documentation

11. **Comprehensive Documentation Updates**
    - **New Documents**:
      - `CONFORMANCE.md` - Complete test-to-requirement mapping (18KB)
      - `CONFORMANCE_TESTING.md` - Official conformance framework guide (12KB)
      - `PHASE_1_COMPLETION_SUMMARY.md` - Phase 1 implementation summary (9KB)
      - `PHASE_2_COMPLETION_SUMMARY.md` - Phase 2 conformance summary (13KB)
    - **Updated Documents**:
      - `README.md` - Complete feature and API documentation
      - `SECURITY.md` - Enhanced security documentation
      - `AGENTS.md` - TDD workflow and development guidelines

#### Transport Layer

12. **Streamable HTTP Transport**
    - **Unified Endpoint**: Single `/mcp` endpoint for all operations
    - **Session Management**: `Mcp-Session-Id` header-based sessions
    - **SSE Streaming**: Server-Sent Events for real-time responses
    - **Backward Compatible**: Legacy `/sse` and `/message` endpoints still supported
    - **HTTP Methods**: Support for GET (SSE), POST (requests), DELETE (session cleanup)

#### Build and Infrastructure

13. **CMake Presets**
    - **Development Presets**: `dev-debug`, `dev-release`
    - **Sanitizer Presets**: `sanitizer-address`, `sanitizer-undefined`
    - **Coverage Preset**: `coverage` for code coverage analysis
    - **CI Presets**: `ci-linux`, `ci-windows`

14. **vcpkg Dependency Management**
    - **Manifest Mode**: All dependencies managed via `vcpkg.json`
    - **Binary Caching**: File-based cache with GitHub Actions integration
    - **Primary Dependency**: Boost.Beast 1.90.0 for HTTP/WebSocket
    - **Test Framework**: Boost.Test 1.90.0 for unit and integration testing

### API Changes

#### Server API
- `server.register_tool()` - Enhanced to support `title` and `outputSchema` fields
- `server.request_elicitation()` - New method for human-in-the-loop interactions
- `server.client_supports_elicitation()` - Check if client supports elicitation
- `server.handle_complete()` - New handler for completion requests
- `server.validate_protocol_version_header()` - New validation method

#### Tool Builder API
- `tool_builder.with_title()` - Set optional tool title
- `tool_builder.with_output_schema()` - Define structured output schema

#### Client API
- Protocol version header automatically included after initialization
- Support for receiving elicitation requests from server
- Support for sending completion requests

### Migration Guide from 2025-03-26

#### Required Changes

1. **Remove Batch Requests**
   ```cpp
   // OLD (2025-03-26): Batch request
   json batch = json::array();
   batch.push_back(request1);
   batch.push_back(request2);
   // REMOVE THIS - no longer supported
   
   // NEW (2025-06-18): Individual requests
   auto result1 = client.send_request(request1);
   auto result2 = client.send_request(request2);
   ```

2. **Protocol Version Header**
   - **Automatic**: Clients automatically include `MCP-Protocol-Version` header after initialization
   - **No Action Required**: Header handling is transparent to API users
   - **Server Validation**: Servers automatically validate incoming headers

#### Optional Enhancements

3. **Add Structured Tool Outputs**
   ```cpp
   // Add title and output schema to existing tools
   tool calculator = tool_builder("calculator")
       .with_description("Perform calculations")
       .with_title("Calculator Tool")  // NEW
       .with_output_schema(schema)     // NEW
       .with_string_param("expression", "Math expression", "")
       .build();
   ```

4. **Use Elicitation for User Input**
   ```cpp
   // Request user confirmation during tool execution
   if (server.client_supports_elicitation(session_id)) {
       auto result = server.request_elicitation(
           session_id,
           "Confirm deletion?",
           create_confirmation_schema(),
           30s  // timeout
       );
       if (result.action == "accept") {
           // Proceed with operation
       }
   }
   ```

5. **Implement Completion Support**
   ```cpp
   // Provide argument completions
   server.register_completion_handler([](const complete_request& req) {
       complete_result result;
       result.completion.values = {"option1", "option2", "option3"};
       return result;
   });
   ```

### Removed Features

- **JSON-RPC Batch Requests**: Completely removed as per specification
- **Legacy Batch Initialization Protection**: No longer applicable

### Deprecated Features

- **Legacy Transport Endpoints**: `/sse` and `/message` endpoints deprecated in favor of `/mcp`
  - Still supported for backward compatibility
  - Will be removed in a future version
  - Migrate to Streamable HTTP transport

### Known Limitations

1. **Authentication/Authorization**: Not implemented (see Security section below)
2. **Sampling**: Not implemented (optional feature)
3. **Roots**: Partially implemented (list_roots works, no change notifications)
4. **Logging**: Not implemented (optional feature)

### Security Note

⚠️ **IMPORTANT**: This implementation intentionally does NOT include authentication or authorization mechanisms, even where they may be mentioned in the MCP protocol or upstream SDKs. This is a design decision for personal/local use only.

**Do NOT deploy this server in multi-user, shared, or production environments without implementing proper authentication and authorization controls.**

See [SECURITY.md](SECURITY.md) for more details on security features that ARE implemented (origin validation, tool execution safety, DNS rebinding mitigation).

### Testing

- **Total Tests**: 201+ tests across all components
- **Pass Rate**: 100%
- **Official Conformance**: 72% fully passing, 86% implemented
- **Continuous Integration**: GitHub Actions on Linux and Windows
- **Test Framework**: Boost.Test 1.90.0

### Performance

- **Concurrent Sessions**: Supports multiple simultaneous clients
- **Thread Pool**: Configurable thread pool for request handling
- **Async I/O**: Boost.Asio-based asynchronous I/O
- **SSE Streaming**: Real-time event streaming with heartbeat

### References

- **MCP Specification**: https://modelcontextprotocol.io/specification/2025-06-18/
- **Changelog**: https://modelcontextprotocol.io/specification/2025-06-18/changelog
- **Reference Implementation**: https://github.com/modelcontextprotocol/modelcontextprotocol
- **Official Conformance Suite**: https://github.com/modelcontextprotocol/conformance

## [2025.03.26] - 2026-01-15

### Initial Release

- Initial implementation of MCP 2025-03-26 specification
- Basic JSON-RPC 2.0 communication
- Tool registration and execution
- Resource management
- HTTP and stdio transports
- Session lifecycle management
- JSON-RPC batch support (later removed in 2025-06-18)

---

## Version Format

Version numbers follow the MCP specification date format: `YYYY.MM.DD` (e.g., `2025.06.18`)

This reflects the MCP protocol version implemented, not a traditional semantic version.
