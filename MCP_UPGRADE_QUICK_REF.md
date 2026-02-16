# MCP Protocol Changes Quick Reference

**Quick navigation for the MCP protocol upgrade survey**

---

## 📚 Survey Documents

1. **[MCP_PROTOCOL_CHANGES_SURVEY.md](./MCP_PROTOCOL_CHANGES_SURVEY.md)** - Complete detailed survey
   - 30+ pages of comprehensive analysis
   - Breaking changes with before/after code examples
   - File-by-file impact mapping
   - Normative requirements

2. **[MCP_UPGRADE_IMPLEMENTATION_PLAN.md](./MCP_UPGRADE_IMPLEMENTATION_PLAN.md)** - Phased implementation plan
   - TDD-based task breakdown
   - 4 phases, 6-7 weeks timeline
   - Success criteria and testing strategy

---

## ⚡ Critical Breaking Changes (2025-06-18)

### 1. 🔴 JSON-RPC Batching REMOVED
- **Status:** ❌ BREAKING - Must remove support
- **Impact:** 6+ files affected
- **Action:** Remove batch processing, update tests, delete examples
- **Files:** `src/mcp_server.cpp`, `examples/batch_example.cpp`, tests, docs

### 2. 🔴 MCP-Protocol-Version Header REQUIRED
- **Status:** ❌ BREAKING - Must implement
- **Impact:** All HTTP clients and servers
- **Action:** Add header validation, send header in all requests
- **Format:** `MCP-Protocol-Version: 2025-06-18`

### 3. 🟡 Structured Tool Output Schema (New Feature)
- **Status:** ❌ NEW FEATURE - Recommended
- **Impact:** Tool definitions and results
- **Action:** Add `title`, `outputSchema`, `structuredContent` support
- **Files:** `include/mcp_tool.h`, `src/mcp_tool.cpp`, examples

---

## 📊 Priority Matrix

| Priority | Changes |
|----------|---------|
| 🔴 **CRITICAL** | Batching removal, Protocol version header |
| 🟡 **HIGH** | Structured tools, Resource links, Security docs |
| 🟢 **MEDIUM** | Elicitation, Meta fields, Title fields |
| ⚪ **LOW** | Extensions (2025-11-25), Completion context |

---

## 📅 Version Timeline

| Version | Date | Status | Implementation |
|---------|------|--------|----------------|
| 2025-03-26 | Mar 2025 | Superseded | ✅ Current |
| 2025-06-18 | Jun 2025 | Stable | ❌ Target |
| 2025-11-25 | Nov 2025 | Latest | ⏭️ Future |

---

## 🎯 Quick Actions

### For Immediate Upgrade to 2025-06-18:

1. **Phase 1 (Weeks 1-4)** - Breaking Changes
   - Remove JSON-RPC batching
   - Add MCP-Protocol-Version header
   - Implement structured tool output
   - Update version constant to "2025-06-18"

2. **Phase 2 (Week 5)** - High Priority Features
   - Add resource links support
   - Update security documentation

### Files You'll Touch Most:

- `include/mcp_message.h` - Version constant, content types
- `include/mcp_tool.h` - Tool structure with new fields
- `src/mcp_server.cpp` - Remove batching, add header validation
- `src/mcp_streamable_http_client.cpp` - Add version header
- `examples/batch_example.cpp` - Remove or deprecate
- `README.md` - Update all docs
- `test/` - Update and add tests

---

## 📖 Normative Requirements (MUST)

From 2025-06-18 specification:

1. ✅ **Clients MUST** include `MCP-Protocol-Version` header (post-initialization)
2. ✅ **Servers MUST** respond 400 for invalid/unsupported version headers
3. ✅ **Implementations MUST NOT** accept JSON-RPC batch arrays
4. ✅ **Servers MUST** validate structured content against outputSchema (if provided)
5. ✅ **Lifecycle operations** use MUST (no longer SHOULD)

---

## 🔗 Official Resources

- [MCP 2025-06-18 Specification](https://modelcontextprotocol.io/specification/2025-06-18)
- [MCP 2025-06-18 Changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog)
- [MCP GitHub Releases](https://github.com/modelcontextprotocol/modelcontextprotocol/releases)
- [MCP Specification Repo](https://github.com/modelcontextprotocol/specification)

---

## 🧪 Testing Checklist

After implementing each change:

- [ ] Unit tests pass
- [ ] Integration tests pass  
- [ ] No regressions in existing tests
- [ ] New tests for new features
- [ ] Examples work correctly
- [ ] Documentation updated
- [ ] Security scan (codeql_checker) passes

---

## 💡 Key Code Examples

### Rejecting Batch Requests (NEW)
```cpp
// 2025-06-18: Reject batch arrays
if (request.is_array()) {
    return create_error_response(
        json(nullptr),
        error_code::invalid_request,
        "JSON-RPC batching not supported in MCP 2025-06-18+"
    );
}
```

### Adding Protocol Version Header (NEW)
```cpp
// Client must send after initialization
headers.emplace("MCP-Protocol-Version", negotiated_version_);
```

### Tool with Output Schema (NEW)
```cpp
tool weather = tool_builder("get_weather")
    .with_title("Weather Information")  // NEW
    .with_output_schema(json{          // NEW
        {"type", "object"},
        {"properties", {
            {"temperature", {{"type", "number"}}},
            {"conditions", {{"type", "string"}}}
        }}
    })
    .build();
```

---

## 🚀 Getting Started

1. Read **[MCP_PROTOCOL_CHANGES_SURVEY.md](./MCP_PROTOCOL_CHANGES_SURVEY.md)** for complete details
2. Review **[MCP_UPGRADE_IMPLEMENTATION_PLAN.md](./MCP_UPGRADE_IMPLEMENTATION_PLAN.md)** for task breakdown
3. Start with Phase 1.1 (Remove Batching) using TDD workflow
4. Create feature branch: `feature/mcp-2025-06-18`
5. Follow TDD: Write test → Fail → Implement → Pass → Refactor

---

**Last Updated:** 2026-02-16  
**Survey Version:** 1.0  
**Status:** 📋 Ready for Implementation
