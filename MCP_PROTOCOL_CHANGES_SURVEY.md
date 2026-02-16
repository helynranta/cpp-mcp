# MCP Protocol Changes Survey: 2025-03-26 to 2025-11-25

**Document Purpose:** Complete survey of all protocol, behavioral, and breaking changes in the Model Context Protocol (MCP) from version 2025-03-26 to 2025-11-25 (latest release).

**Target Repository:** helynranta/cpp-mcp (C++23 MCP implementation)

**Date:** 2026-02-16

---

## Executive Summary

The Model Context Protocol underwent **two major revisions** since 2025-03-26:
- **2025-06-18** - Major breaking changes including removal of JSON-RPC batching, structured tool outputs, OAuth security enhancements
- **2025-11-25** - Minor updates focused on extensions support

The cpp-mcp repository currently implements **MCP 2025-03-26** and requires significant updates to conform to the latest specification.

---

## Version Timeline

| Version | Release Date | Status | Implementation Status in cpp-mcp |
|---------|--------------|--------|----------------------------------|
| 2025-03-26 | March 26, 2025 | Superseded | ✅ **Fully Implemented** (current) |
| 2025-06-18 | June 18, 2025 | Stable | ❌ **Not Implemented** |
| 2025-11-25 | November 25, 2025 | Latest Stable | ❌ **Not Implemented** |
| draft | Ongoing | Draft/RC | ❌ **Not Implemented** |

---

## Breaking Changes Matrix

### 1. JSON-RPC Batching Removal (BREAKING - HIGH PRIORITY)

**Status:** ❌ BREAKING CHANGE - MUST ADDRESS

**Spec Reference:** [PR #416](https://github.com/modelcontextprotocol/specification/pull/416), 2025-06-18 changelog

#### Change Description
- **Before (2025-03-26):** JSON-RPC batching was **REQUIRED**. Servers MUST support receiving batch requests (arrays of JSON-RPC messages).
- **After (2025-06-18):** JSON-RPC batching support has been **REMOVED** from the specification entirely.

#### Old Format (2025-03-26) - DEPRECATED
```json
// Batch request - NO LONGER VALID in 2025-06-18+
[
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/list",
    "params": {}
  },
  {
    "jsonrpc": "2.0",
    "id": 2,
    "method": "resources/list",
    "params": {}
  }
]
```

#### New Format (2025-06-18+) - CURRENT STANDARD
```json
// Single request only
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/list",
  "params": {}
}
```

#### Impact on cpp-mcp Codebase

**Current Implementation:**
- ✅ **GOOD:** Code actively supports batching (implemented per 2025-03-26)
- ❌ **ACTION REQUIRED:** Must remove batch support for 2025-06-18+ compliance

**Files Affected:**
1. **`src/mcp_server.cpp`**
   - Lines 664-670: Batch initialization protection (can be removed)
   - Lines 1221-1227: Duplicate batch initialization check (can be removed)
   - Batch processing logic in `handle_jsonrpc_request()` and `handle_mcp_post()`

2. **`test/lifecycle_compliance_test.cpp`**
   - Batch-related test cases may need removal or updates

3. **`test/mcp_test.cpp`**
   - Batch request tests

4. **`examples/batch_example.cpp`**
   - ⚠️ **ENTIRE FILE OBSOLETE** - This example demonstrates 2025-03-26 batch support

5. **`README.md`**
   - Line 12: "Batch Request Support" feature claim (REMOVE)
   - Line 29: "Batch Initialization Protection" (REMOVE)
   - Lines 593-625: Batch example documentation (REMOVE or UPDATE)
   - Line 782: "MCP 2025-03-26 requires implementations to support receiving JSON-RPC batches" (FALSE for 2025-06-18+)

**Normative Requirements:**
- **MUST NOT** accept batch requests (arrays of JSON-RPC messages)
- **MUST** return appropriate error if client sends batch
- **SHOULD** return HTTP 400 Bad Request or JSON-RPC error -32600 (Invalid Request)

**Recommended Action:**
1. Remove batch processing code from server
2. Add validation to reject array inputs
3. Remove batch examples and documentation
4. Update tests to verify batch rejection
5. Consider migration guide for users upgrading from 2025-03-26

---

### 2. Structured Tool Output Schema (BREAKING - HIGH PRIORITY)

**Status:** ❌ NEW FEATURE - MUST IMPLEMENT

**Spec Reference:** [PR #371](https://github.com/modelcontextprotocol/specification/pull/371), [Tools Specification](https://modelcontextprotocol.io/specification/2025-06-18/server/tools)

#### Change Description
- **Before (2025-03-26):** Tools returned only unstructured content in `content` array
- **After (2025-06-18):** Tools MAY provide `outputSchema` and return `structuredContent` alongside `content`

#### New Tool Definition (2025-06-18+)
```json
{
  "name": "get_weather_data",
  "title": "Weather Data Retriever",  // NEW: Optional display name
  "description": "Get current weather data for a location",
  "inputSchema": {
    "type": "object",
    "properties": {
      "location": {
        "type": "string",
        "description": "City name or zip code"
      }
    },
    "required": ["location"]
  },
  "outputSchema": {  // NEW: Optional output schema
    "type": "object",
    "properties": {
      "temperature": {
        "type": "number",
        "description": "Temperature in celsius"
      },
      "conditions": {
        "type": "string",
        "description": "Weather conditions description"
      },
      "humidity": {
        "type": "number",
        "description": "Humidity percentage"
      }
    },
    "required": ["temperature", "conditions", "humidity"]
  }
}
```

#### Old Tool Response (2025-03-26)
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Current weather in New York:\nTemperature: 72°F\nConditions: Partly cloudy"
      }
    ],
    "isError": false
  }
}
```

#### New Tool Response (2025-06-18+)
```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "result": {
    "content": [  // Still required for backwards compatibility
      {
        "type": "text",
        "text": "{\"temperature\": 22.5, \"conditions\": \"Partly cloudy\", \"humidity\": 65}"
      }
    ],
    "structuredContent": {  // NEW: Structured data matching outputSchema
      "temperature": 22.5,
      "conditions": "Partly cloudy",
      "humidity": 65
    },
    "isError": false
  }
}
```

#### Impact on cpp-mcp Codebase

**Current Implementation:**
- ❌ **MISSING:** No `outputSchema` support in tool definitions
- ❌ **MISSING:** No `structuredContent` support in tool results
- ❌ **MISSING:** No `title` field support for display names

**Files Affected:**
1. **`include/mcp_tool.h`**
   - `struct tool`: Add `std::string title` (optional)
   - `struct tool`: Add `json output_schema` (optional)
   - `tool::to_json()`: Include `title` and `outputSchema` fields if present

2. **`src/mcp_tool.cpp`**
   - Update `to_json()` implementation

3. **`include/mcp_message.h` or new header**
   - Define `ToolResult` structure with `structuredContent` support

4. **`src/mcp_server.cpp`**
   - Update tool call response handling to support `structuredContent`

5. **`test/mcp_test.cpp`**
   - Add tests for structured output

**Normative Requirements:**
- If `outputSchema` is provided:
  - Servers **MUST** provide structured results conforming to the schema
  - Clients **SHOULD** validate structured results against the schema
- Tools returning structured content **SHOULD** also return serialized JSON in a TextContent block (backwards compatibility)

**Recommended Action:**
1. Extend `tool` struct with `title` and `output_schema` fields
2. Extend tool result to support `structuredContent`
3. Update `tool_builder` to support new fields
4. Add validation for structured content against output schema
5. Update documentation and examples

---

### 3. MCP-Protocol-Version Header (BREAKING - HIGH PRIORITY)

**Status:** ❌ MUST IMPLEMENT

**Spec Reference:** [PR #548](https://github.com/modelcontextprotocol/specification/pull/548), [Transports Specification](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports#protocol-version-header)

#### Change Description
- **Before (2025-03-26):** No protocol version header requirement
- **After (2025-06-18):** **MUST** include `MCP-Protocol-Version` header in all HTTP requests after initialization

#### Old Behavior (2025-03-26)
```http
POST /mcp HTTP/1.1
Host: example.com
Content-Type: application/json
Accept: application/json, text/event-stream

{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}
```

#### New Behavior (2025-06-18+) - REQUIRED
```http
POST /mcp HTTP/1.1
Host: example.com
Content-Type: application/json
Accept: application/json, text/event-stream
MCP-Protocol-Version: 2025-06-18  ⬅️ NEW: MUST include after initialization

{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}
```

#### Backward Compatibility Rule
```
If server does NOT receive MCP-Protocol-Version header:
  AND has no other way to identify version (e.g., from initialization)
  THEN server SHOULD assume protocol version "2025-03-26"

If server receives INVALID or UNSUPPORTED MCP-Protocol-Version:
  THEN server MUST respond with 400 Bad Request
```

#### Impact on cpp-mcp Codebase

**Current Implementation:**
- ❌ **MISSING:** No `MCP-Protocol-Version` header validation
- ❌ **MISSING:** No version header sent by clients
- ⚠️ **PARTIAL:** Session management exists but doesn't track negotiated version

**Files Affected:**
1. **`src/mcp_server.cpp`**
   - Add header validation in `handle_mcp_post()` and `handle_mcp_get()`
   - Store negotiated version during initialization
   - Validate header matches negotiated version
   - Return 400 Bad Request for missing/invalid header (post-initialization)
   - Default to "2025-03-26" if no header and no session state

2. **`src/mcp_streamable_http_client.cpp`**
   - Add `MCP-Protocol-Version` header to all POST/GET requests after initialization

3. **`src/mcp_sse_client.cpp`**
   - Add `MCP-Protocol-Version` header to all HTTP requests after initialization

4. **`include/mcp_server.h`**
   - Add `std::string negotiated_protocol_version_` to session state

5. **`test/streamable_http_transport_test.cpp`**
   - Add tests for header presence and validation

**Normative Requirements:**
- Clients **MUST** include `MCP-Protocol-Version: <version>` header on all HTTP requests after initialization
- Version **SHOULD** be the one negotiated during initialization
- Servers **MUST** respond with 400 Bad Request if header is invalid/unsupported
- Servers **SHOULD** assume "2025-03-26" if no header present (backward compatibility)

**Recommended Action:**
1. Update server to store negotiated version per session
2. Add header validation middleware/logic
3. Update clients to send header
4. Add tests for header validation
5. Document version negotiation process

---

### 4. Resource Links in Tool Results (NEW FEATURE - MEDIUM PRIORITY)

**Status:** ❌ NEW FEATURE - OPTIONAL BUT RECOMMENDED

**Spec Reference:** [PR #603](https://github.com/modelcontextprotocol/specification/pull/603)

#### Change Description
Tools can now return resource links in their results, allowing references to external resources.

#### New Content Type (2025-06-18+)
```json
{
  "type": "resource_link",  // NEW content type
  "uri": "file:///project/src/main.rs",
  "name": "main.rs",
  "description": "Primary application entry point",
  "mimeType": "text/x-rust",
  "annotations": {
    "audience": ["assistant"],
    "priority": 0.9
  }
}
```

#### Example Tool Result with Resource Links
```json
{
  "jsonrpc": "2.0",
  "id": 10,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Analysis complete. See referenced files for details."
      },
      {
        "type": "resource_link",
        "uri": "file:///project/analysis_report.md",
        "name": "Analysis Report",
        "description": "Detailed analysis results",
        "mimeType": "text/markdown"
      }
    ],
    "isError": false
  }
}
```

#### Impact on cpp-mcp Codebase

**Files Affected:**
1. **`include/mcp_message.h`** or new content type definitions
   - Add `ResourceLinkContent` type
2. **`src/mcp_server.cpp`**
   - Support resource_link content type in responses
3. **Documentation**
   - Add examples of using resource links

**Normative Requirements:**
- Resource links **MAY** be included in tool results
- Resource links support same annotations as regular resources

---

### 5. Elicitation Support (NEW FEATURE - MEDIUM PRIORITY)

**Status:** ❌ NEW FEATURE - OPTIONAL

**Spec Reference:** [PR #382](https://github.com/modelcontextprotocol/specification/pull/382), [Elicitation Specification](https://modelcontextprotocol.io/specification/2025-06-18/client/elicitation)

#### Change Description
Servers can now request additional information from users during interactions via elicitation.

#### New Capability
```json
{
  "capabilities": {
    "elicitation": true  // NEW: Server supports requesting user input
  }
}
```

#### Example Elicitation Request
```json
{
  "jsonrpc": "2.0",
  "method": "elicitation/request",
  "params": {
    "prompt": "Please provide your API key:",
    "field": "api_key",
    "inputType": "password"
  }
}
```

#### Impact on cpp-mcp Codebase
- Optional feature, not critical for basic conformance
- Could be implemented in future versions

---

### 6. OAuth and Security Enhancements (BREAKING - HIGH PRIORITY)

**Status:** ⚠️ PARTIALLY IMPLEMENTED (2025-03-26 security present)

**Spec Reference:** [PR #338](https://github.com/modelcontextprotocol/specification/pull/338), [PR #734](https://github.com/modelcontextprotocol/specification/pull/734)

#### Change Description
- **New:** MCP servers classified as OAuth Resource Servers
- **New:** Clients MUST implement Resource Indicators (RFC 8707)
- **Enhanced:** Security best practices documentation

#### Current Security Implementation (2025-03-26)
cpp-mcp already implements:
- ✅ Origin header validation
- ✅ DNS rebinding mitigation
- ✅ Configurable allowed origins
- ✅ Tool confirmation hooks
- ✅ Documented in SECURITY.md

#### Additional Requirements (2025-06-18+)
- Add OAuth Resource Server metadata
- Implement Resource Indicators for access tokens
- Update security documentation to reference 2025-06-18 spec

**Files Affected:**
1. **`SECURITY.md`**
   - Update title from "MCP 2025-03-26" to "MCP 2025-06-18"
   - Add OAuth Resource Server information
   - Add Resource Indicators (RFC 8707) documentation

---

### 7. Lifecycle Operation Changes (NORMATIVE - HIGH PRIORITY)

**Status:** ⚠️ NEEDS VERIFICATION

**Spec Reference:** 2025-06-18 changelog item #9

#### Change Description
- **Before (2025-03-26):** Lifecycle operations used **SHOULD**
- **After (2025-06-18):** Changed **SHOULD** to **MUST** in Lifecycle Operation

#### Impact on cpp-mcp Codebase

**Current Implementation:**
- `test/lifecycle_compliance_test.cpp` - Already implements strict lifecycle
- `src/mcp_server.cpp` - Lines 1527-1540: Already enforces lifecycle rules

**Action Required:**
- ✅ **VERIFY:** Ensure all lifecycle operations use MUST-level enforcement
- Update documentation to reflect MUST requirements

---

### 8. Meta Field Extensions (NEW FEATURE - LOW PRIORITY)

**Status:** ❌ NEW FEATURE - OPTIONAL

**Spec Reference:** [PR #710](https://github.com/modelcontextprotocol/specification/pull/710)

#### Change Description
Added `_meta` field to additional interface types for extended metadata.

#### Example
```json
{
  "name": "my_tool",
  "description": "Tool description",
  "inputSchema": {...},
  "_meta": {  // NEW: Extended metadata
    "version": "1.2.0",
    "author": "example",
    "custom_field": "value"
  }
}
```

#### Impact on cpp-mcp Codebase
- Low priority
- Can be added as `json meta` field to relevant structures

---

### 9. Completion Request Context (NEW FEATURE - LOW PRIORITY)

**Status:** ❌ NEW FEATURE - OPTIONAL

**Spec Reference:** [PR #598](https://github.com/modelcontextprotocol/specification/pull/598)

#### Change Description
Added `context` field to `CompletionRequest` for including previously-resolved variables.

#### Impact on cpp-mcp Codebase
- Only relevant if implementing completion/autocomplete features
- Not critical for basic MCP server functionality

---

### 10. Extensions Support (2025-11-25 - NEW FEATURE)

**Status:** ❌ NEW FEATURE - FORWARD LOOKING

**Spec Reference:** [2025-11-25 changelog](https://modelcontextprotocol.io/specification/draft/changelog)

#### Change Description
Added `extensions` field to `ClientCapabilities` and `ServerCapabilities` to support optional extensions beyond core protocol.

#### Example
```json
{
  "capabilities": {
    "tools": {...},
    "extensions": {  // NEW in 2025-11-25
      "custom_extension_1": {...},
      "custom_extension_2": {...}
    }
  }
}
```

#### Impact on cpp-mcp Codebase
- Future feature for 2025-11-25 upgrade
- Not critical for 2025-06-18 conformance

---

## Summary: Priority Matrix

### Critical (MUST Address for 2025-06-18 Compliance)

| Change | Priority | Breaking | Effort | Files Affected |
|--------|----------|----------|--------|----------------|
| Remove JSON-RPC Batching | 🔴 CRITICAL | ✅ Yes | Medium | 6+ files |
| Add MCP-Protocol-Version Header | 🔴 CRITICAL | ✅ Yes | Medium | 5+ files |
| Structured Tool Output | 🔴 CRITICAL | ⚠️ Additive | High | 5+ files |
| Lifecycle SHOULD → MUST | 🟡 HIGH | ⚠️ Normative | Low | 2 files |

### High Priority (Recommended for Full Conformance)

| Change | Priority | Breaking | Effort | Files Affected |
|--------|----------|----------|--------|----------------|
| Resource Links Support | 🟡 HIGH | ❌ No | Medium | 3 files |
| Update Security Docs (OAuth) | 🟡 HIGH | ❌ No | Low | 1 file |

### Medium Priority (Optional Enhancements)

| Change | Priority | Breaking | Effort | Files Affected |
|--------|----------|----------|--------|----------------|
| Elicitation Support | 🟢 MEDIUM | ❌ No | High | Multiple |
| Title Field for Tools | 🟢 MEDIUM | ❌ No | Low | 2 files |
| Meta Field Support | 🟢 MEDIUM | ❌ No | Low | Multiple |

### Future Considerations

| Change | Priority | Target Version |
|--------|----------|----------------|
| Extensions Support | ⚪ LOW | 2025-11-25 |
| Completion Context | ⚪ LOW | 2025-06-18 |

---

## Detailed File Impact Analysis

### Files Requiring Major Changes

1. **`include/mcp_message.h`**
   - Update `MCP_VERSION` from "2025-03-26" to "2025-06-18"
   - Add structured content types
   - Add resource link content type

2. **`src/mcp_server.cpp`**
   - **REMOVE:** Batch request processing (lines ~664-670, ~1221-1227)
   - **ADD:** MCP-Protocol-Version header validation
   - **ADD:** Structured content support in tool responses
   - **UPDATE:** Error handling for batch requests (reject with 400)

3. **`include/mcp_tool.h`**
   - **ADD:** `std::string title` field (optional)
   - **ADD:** `json output_schema` field (optional)
   - **UPDATE:** `to_json()` method to include new fields

4. **`src/mcp_streamable_http_client.cpp`**
   - **ADD:** `MCP-Protocol-Version` header to all requests post-initialization

5. **`src/mcp_sse_client.cpp`**
   - **ADD:** `MCP-Protocol-Version` header to all requests post-initialization

6. **`examples/batch_example.cpp`**
   - **REMOVE:** Entire file or mark as deprecated/legacy

7. **`README.md`**
   - **UPDATE:** Version references from 2025-03-26 to 2025-06-18
   - **REMOVE:** Batch request feature claims
   - **ADD:** Structured tool output documentation
   - **ADD:** Protocol version header documentation
   - **UPDATE:** All code examples to reflect 2025-06-18 format

8. **`SECURITY.md`**
   - **UPDATE:** Title and version references
   - **ADD:** OAuth Resource Server information
   - **ADD:** RFC 8707 Resource Indicators documentation

### Files Requiring Minor Updates

9. **`test/mcp_test.cpp`**
   - Remove batch tests or convert to rejection tests
   - Add structured output tests

10. **`test/lifecycle_compliance_test.cpp`**
    - Verify MUST-level enforcement
    - Update test descriptions

11. **`test/streamable_http_transport_test.cpp`**
    - Add MCP-Protocol-Version header tests

### New Files to Create

12. **`MCP_MIGRATION_GUIDE.md`** (Recommended)
    - Guide for users upgrading from 2025-03-26 to 2025-06-18
    - Breaking changes summary
    - Migration steps

13. **`examples/structured_tool_example.cpp`** (Recommended)
    - Demonstrate new structured output feature

---

## Code Examples for Key Changes

### Example 1: Removing Batch Support

**Before (2025-03-26):**
```cpp
// src/mcp_server.cpp - OLD CODE
json process_batch(const json& batch_array) {
    json responses = json::array();
    for (const auto& req : batch_array) {
        responses.push_back(process_single_request(req));
    }
    return responses;
}
```

**After (2025-06-18):**
```cpp
// src/mcp_server.cpp - NEW CODE
json process_request(const json& request) {
    // Reject batch requests (arrays)
    if (request.is_array()) {
        return create_error_response(
            json(nullptr),  // No ID for batch error
            error_code::invalid_request,
            "JSON-RPC batching is not supported in MCP 2025-06-18+",
            {{"note", "Please send individual requests instead of arrays"}}
        );
    }
    
    return process_single_request(request);
}
```

### Example 2: Adding MCP-Protocol-Version Header Validation

**New Code (2025-06-18):**
```cpp
// src/mcp_server.cpp
bool validate_protocol_version_header(const std::string& header_value, 
                                      const std::string& session_id) {
    if (header_value.empty()) {
        // Backward compatibility: assume 2025-03-26 if no header
        if (session_id.empty()) {
            // No session yet, this is initialization - OK
            return true;
        }
        // Post-initialization without header - assume old version
        LOG_WARNING("Missing MCP-Protocol-Version header, assuming 2025-03-26");
        return true;
    }
    
    // Valid supported versions
    const std::vector<std::string> supported = {"2025-03-26", "2025-06-18", "2025-11-25"};
    
    if (std::find(supported.begin(), supported.end(), header_value) == supported.end()) {
        LOG_ERROR("Unsupported MCP-Protocol-Version: ", header_value);
        return false;
    }
    
    // Verify matches negotiated version if we have a session
    if (!session_id.empty()) {
        auto negotiated = get_session_state<std::string>(session_id, "negotiated_version");
        if (negotiated && *negotiated != header_value) {
            LOG_ERROR("Version mismatch: header=", header_value, " negotiated=", *negotiated);
            return false;
        }
    }
    
    return true;
}
```

### Example 3: Tool with Output Schema

**New Code (2025-06-18):**
```cpp
// Creating a tool with output schema
tool weather_tool = tool_builder("get_weather_data")
    .with_title("Weather Data Retriever")  // NEW: Display name
    .with_description("Get current weather data for a location")
    .with_string_param("location", "City name or zip code", "")
    .with_output_schema(json{  // NEW: Output schema
        {"type", "object"},
        {"properties", {
            {"temperature", {
                {"type", "number"},
                {"description", "Temperature in celsius"}
            }},
            {"conditions", {
                {"type", "string"},
                {"description", "Weather conditions"}
            }},
            {"humidity", {
                {"type", "number"},
                {"description", "Humidity percentage"}
            }}
        }},
        {"required", json::array({"temperature", "conditions", "humidity"})}
    })
    .build();

// Tool handler returning structured content
server.register_tool(weather_tool, [](const json& params, const std::string& session_id) -> json {
    // Fetch weather data...
    json structured_data = {
        {"temperature", 22.5},
        {"conditions", "Partly cloudy"},
        {"humidity", 65}
    };
    
    return {
        {"content", json::array({
            {
                {"type", "text"},
                {"text", structured_data.dump()}  // Backward compat
            }
        })},
        {"structuredContent", structured_data},  // NEW: Structured data
        {"isError", false}
    };
});
```

---

## Normative MUST Requirements Summary

### From 2025-03-26 to 2025-06-18

#### Protocol Version Header
- Clients **MUST** include `MCP-Protocol-Version` header in all HTTP requests after initialization
- Servers **MUST** respond with 400 Bad Request if header value is invalid/unsupported
- Servers **SHOULD** assume "2025-03-26" if no header present (backward compatibility)

#### Batching
- Implementations **MUST NOT** send batch requests (JSON-RPC arrays)
- Servers **SHOULD** reject batch requests with appropriate error

#### Structured Tool Output
- If `outputSchema` is provided:
  - Servers **MUST** provide structured results conforming to the schema
  - Clients **SHOULD** validate structured results against the schema

#### Lifecycle
- Lifecycle operations now use **MUST** instead of **SHOULD**
- Stricter enforcement of state transitions

#### Security
- Servers **MUST** validate all tool inputs
- Servers **MUST** implement proper access controls
- Servers **MUST** rate limit tool invocations
- Servers **MUST** sanitize tool outputs

---

## Implementation Roadmap

### Phase 1: Critical Breaking Changes (Target: 2025-06-18 Compliance)
**Estimated Effort:** 2-3 weeks

1. **Week 1: Remove Batching Support**
   - [ ] Remove batch processing from `mcp_server.cpp`
   - [ ] Add batch rejection logic
   - [ ] Remove/update batch examples
   - [ ] Update tests
   - [ ] Update documentation

2. **Week 2: Add Protocol Version Header**
   - [ ] Implement header validation in server
   - [ ] Add header to clients
   - [ ] Store negotiated version in session
   - [ ] Add backward compatibility logic
   - [ ] Add tests

3. **Week 2-3: Structured Tool Output**
   - [ ] Extend `tool` struct with `title` and `output_schema`
   - [ ] Add `structuredContent` support to results
   - [ ] Update `tool_builder`
   - [ ] Add validation
   - [ ] Create examples
   - [ ] Update tests

4. **Week 3: Version Bump and Documentation**
   - [ ] Update `MCP_VERSION` to "2025-06-18"
   - [ ] Update all documentation
   - [ ] Update examples
   - [ ] Update SECURITY.md
   - [ ] Create migration guide

### Phase 2: High Priority Features (Recommended)
**Estimated Effort:** 1-2 weeks

1. **Resource Links Support**
   - [ ] Add `resource_link` content type
   - [ ] Update tool result handling
   - [ ] Add examples

2. **Security Updates**
   - [ ] Add OAuth Resource Server documentation
   - [ ] Document RFC 8707 Resource Indicators

### Phase 3: Optional Enhancements (Future)
**Estimated Effort:** 2-4 weeks

1. **Elicitation Support**
2. **Meta Fields**
3. **Completion Context**

### Phase 4: 2025-11-25 Upgrade (Long-term)
**Estimated Effort:** 1 week

1. **Extensions Support**
   - [ ] Add `extensions` field to capabilities
   - [ ] Update version to "2025-11-25"

---

## Testing Strategy

### Critical Tests to Add/Update

1. **Batch Rejection Tests**
   ```cpp
   BOOST_AUTO_TEST_CASE(RejectsBatchRequests) {
       json batch = json::array({
           request::create("tools/list", {}).to_json(),
           request::create("resources/list", {}).to_json()
       });
       
       auto response = server.process_request(batch);
       BOOST_CHECK(response.contains("error"));
       BOOST_CHECK_EQUAL(response["error"]["code"], -32600);
   }
   ```

2. **Protocol Version Header Tests**
   ```cpp
   BOOST_AUTO_TEST_CASE(ValidatesProtocolVersionHeader) {
       // Test missing header (should accept with warning)
       // Test valid header
       // Test invalid header (should return 400)
       // Test version mismatch
   }
   ```

3. **Structured Output Tests**
   ```cpp
   BOOST_AUTO_TEST_CASE(ToolWithStructuredOutput) {
       // Test tool with outputSchema
       // Test structuredContent in result
       // Test schema validation
   }
   ```

---

## References

### Official MCP Specification
- [MCP 2025-03-26 Specification](https://modelcontextprotocol.io/specification/2025-03-26)
- [MCP 2025-06-18 Specification](https://modelcontextprotocol.io/specification/2025-06-18)
- [MCP 2025-06-18 Changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog)
- [MCP 2025-11-25 Specification](https://modelcontextprotocol.io/specification/2025-11-25)
- [MCP Draft Specification](https://modelcontextprotocol.io/specification/draft)
- [MCP Draft Changelog](https://modelcontextprotocol.io/specification/draft/changelog)

### GitHub Resources
- [MCP Specification Repository](https://github.com/modelcontextprotocol/specification)
- [MCP Protocol Releases](https://github.com/modelcontextprotocol/modelcontextprotocol/releases)
- [Python SDK](https://github.com/modelcontextprotocol/python-mcp)
- [Node.js SDK](https://github.com/modelcontextprotocol/node-mcp)

### Key Pull Requests
- [PR #416 - Remove JSON-RPC Batching](https://github.com/modelcontextprotocol/specification/pull/416)
- [PR #371 - Structured Tool Output](https://github.com/modelcontextprotocol/modelcontextprotocol/pull/371)
- [PR #548 - Protocol Version Header](https://github.com/modelcontextprotocol/specification/pull/548)
- [PR #603 - Resource Links](https://github.com/modelcontextprotocol/modelcontextprotocol/pull/603)
- [PR #338 - OAuth Resource Servers](https://github.com/modelcontextprotocol/modelcontextprotocol/pull/338)
- [PR #734 - Resource Indicators](https://github.com/modelcontextprotocol/modelcontextprotocol/pull/734)

---

## Appendix A: Version Negotiation Process

The MCP specification defines version negotiation during initialization:

1. Client sends `InitializeRequest` with `protocolVersion` parameter
2. Server responds with its supported version in `InitializeResult`
3. Both parties use the negotiated version for all subsequent communication
4. (2025-06-18+) Client includes `MCP-Protocol-Version` header in all HTTP requests

---

## Appendix B: Backward Compatibility Strategy

To maintain backward compatibility with 2025-03-26 clients while supporting 2025-06-18:

1. **Detect client version** during initialization
2. **Store negotiated version** in session state
3. **Apply version-specific behavior**:
   - 2025-03-26: Support batching, no header required
   - 2025-06-18+: Reject batching, require header
4. **Provide migration path** for users

---

**END OF SURVEY**

*Document Version: 1.0*  
*Last Updated: 2026-02-16*  
*Author: GitHub Copilot Agent*
