# MCP Protocol Upgrade Implementation Plan
## From 2025-03-26 to 2025-06-18

**Repository:** helynranta/cpp-mcp  
**Current Version:** 2025-03-26  
**Target Version:** 2025-06-18  
**Date Created:** 2026-02-16

---

## Overview

This document provides a phased implementation plan for upgrading the cpp-mcp repository from MCP specification version 2025-03-26 to 2025-06-18. The plan follows Test-Driven Development (TDD) principles and ensures minimal breaking changes to existing APIs where possible.

**Related Documents:**
- [MCP_PROTOCOL_CHANGES_SURVEY.md](./MCP_PROTOCOL_CHANGES_SURVEY.md) - Complete survey of all changes
- [AGENTS.md](./AGENTS.md) - Agent development guide with TDD workflow
- [README.md](./README.md) - Project documentation

---

## Prerequisites

Before beginning implementation:

- [x] Complete protocol changes survey
- [ ] Review and approve upgrade plan with stakeholders
- [ ] Create tracking issues for each phase
- [ ] Set up feature branch for development
- [ ] Ensure all current tests pass on 2025-03-26 implementation
- [ ] Backup/tag current stable version

---

## Phase 1: Foundation and Breaking Changes
**Goal:** Implement critical breaking changes for 2025-06-18 compliance  
**Duration:** 2-3 weeks  
**Priority:** 🔴 CRITICAL

### 1.1 Remove JSON-RPC Batching Support

**Rationale:** JSON-RPC batching was removed from the specification. Continuing to support it would violate the spec.

#### Tasks

1. **Write Failing Tests** (TDD Step 1)
   - [ ] Create `test/batch_rejection_test.cpp`
   - [ ] Test: Server rejects batch request arrays
   - [ ] Test: Server returns appropriate error code (-32600)
   - [ ] Test: Error message indicates batching not supported
   - [ ] Test: Single requests still work correctly
   - [ ] **Run tests:** Verify they fail (batching currently supported)

2. **Update Server Implementation** (TDD Step 2)
   - [ ] **File:** `src/mcp_server.cpp`
     - [ ] Add `is_batch_request()` helper function
     - [ ] Update `process_jsonrpc_request()` to detect and reject arrays
     - [ ] Remove batch processing logic (lines ~664-670, ~1221-1227)
     - [ ] Return error response for batch attempts
   - [ ] **File:** `include/mcp_server.h`
     - [ ] Remove any batch-related public methods if present
   - [ ] **Run tests:** Verify new tests pass

3. **Update Tests** (TDD Step 3)
   - [ ] **File:** `test/mcp_test.cpp`
     - [ ] Remove or convert batch-supporting tests
     - [ ] Add tests verifying batch rejection
   - [ ] **File:** `test/lifecycle_compliance_test.cpp`
     - [ ] Update initialization batch tests (should fail now)
   - [ ] **Run full test suite:** Ensure no regressions

4. **Remove/Update Examples**
   - [ ] **File:** `examples/batch_example.cpp`
     - [ ] Option A: Delete entirely
     - [ ] Option B: Rename to `batch_example_legacy.cpp` with deprecation notice
     - [ ] Option C: Convert to anti-example showing proper error handling
   - [ ] **File:** `examples/CMakeLists.txt`
     - [ ] Remove batch_example from build if deleted

5. **Update Documentation**
   - [ ] **File:** `README.md`
     - [ ] Remove "Batch Request Support" from features (line 12)
     - [ ] Remove "Batch Initialization Protection" (line 29)
     - [ ] Remove batch example section (lines 593-625)
     - [ ] Update transport description (line 782)
   - [ ] **File:** `MIGRATION_GUIDE.md` (create)
     - [ ] Document batch removal
     - [ ] Provide migration path for users

6. **Verification**
   - [ ] All tests pass
   - [ ] Batch requests properly rejected
   - [ ] Single requests work correctly
   - [ ] Documentation updated

**Estimated Effort:** 3-5 days

---

### 1.2 Add MCP-Protocol-Version Header Support

**Rationale:** The specification now REQUIRES this header in all HTTP requests after initialization.

#### Tasks

1. **Write Failing Tests** (TDD Step 1)
   - [ ] Create `test/protocol_version_header_test.cpp`
   - [ ] Test: Server accepts request with valid header
   - [ ] Test: Server rejects request with invalid header (400 Bad Request)
   - [ ] Test: Server accepts request without header (backward compat)
   - [ ] Test: Server validates header matches negotiated version
   - [ ] Test: Client includes header in all requests after init
   - [ ] **Run tests:** Verify they fail (header not implemented)

2. **Update Server Session Management** (TDD Step 2)
   - [ ] **File:** `include/mcp_server.h`
     - [ ] Add `std::string negotiated_protocol_version_` to session state
     - [ ] Add constant: `const std::vector<std::string> SUPPORTED_VERSIONS`
   - [ ] **File:** `src/mcp_server.cpp`
     - [ ] Store negotiated version in session during initialization
     - [ ] Update `handle_initialize()` to save version to session state

3. **Implement Header Validation**
   - [ ] **File:** `src/mcp_server.cpp`
     - [ ] Add `validate_protocol_version_header()` helper function
     - [ ] Update `handle_mcp_post()` to validate header
     - [ ] Update `handle_mcp_get()` to validate header
     - [ ] Return 400 Bad Request for invalid headers
     - [ ] Log warnings for missing headers (backward compat)
   - [ ] **Run tests:** Verify server-side tests pass

4. **Update HTTP Clients**
   - [ ] **File:** `src/mcp_streamable_http_client.cpp`
     - [ ] Add `std::string negotiated_version_` member
     - [ ] Store version from `InitializeResult`
     - [ ] Add `MCP-Protocol-Version` header to all POST requests
     - [ ] Add `MCP-Protocol-Version` header to all GET requests
   - [ ] **File:** `src/mcp_sse_client.cpp`
     - [ ] Add `std::string negotiated_version_` member
     - [ ] Store version from `InitializeResult`
     - [ ] Add `MCP-Protocol-Version` header to all HTTP requests
   - [ ] **Run tests:** Verify client-side tests pass

5. **Update Tests**
   - [ ] **File:** `test/streamable_http_transport_test.cpp`
     - [ ] Add header presence verification
     - [ ] Test version negotiation flow
   - [ ] **File:** `test/http_security_test.cpp`
     - [ ] Add invalid header rejection tests
   - [ ] **Run full test suite:** Ensure no regressions

6. **Update Documentation**
   - [ ] **File:** `README.md`
     - [ ] Add MCP-Protocol-Version header documentation
     - [ ] Update HTTP transport examples to show header
     - [ ] Add version negotiation explanation
   - [ ] **File:** `MIGRATION_GUIDE.md`
     - [ ] Document header requirement
     - [ ] Explain backward compatibility

7. **Verification**
   - [ ] All tests pass
   - [ ] Header validation works correctly
   - [ ] Clients send header in all requests
   - [ ] Backward compatibility maintained
   - [ ] Error responses appropriate

**Estimated Effort:** 5-7 days

---

### 1.3 Implement Structured Tool Output Schema

**Rationale:** Major new feature allowing typed tool outputs for better LLM integration.

#### Tasks

1. **Write Failing Tests** (TDD Step 1)
   - [ ] Create `test/structured_tool_output_test.cpp`
   - [ ] Test: Tool definition includes `title` field
   - [ ] Test: Tool definition includes `outputSchema`
   - [ ] Test: Tool result includes `structuredContent`
   - [ ] Test: Structured content matches output schema
   - [ ] Test: Backward compat - text content still present
   - [ ] Test: Schema validation of structured content
   - [ ] **Run tests:** Verify they fail (feature not implemented)

2. **Extend Tool Structure** (TDD Step 2)
   - [ ] **File:** `include/mcp_tool.h`
     - [ ] Add `std::string title` field (optional)
     - [ ] Add `json output_schema` field (optional)
     - [ ] Update `to_json()` to include new fields
     - [ ] Add `has_output_schema()` helper
   - [ ] **File:** `src/mcp_tool.cpp`
     - [ ] Implement updated `to_json()` method
     - [ ] Add schema to JSON if present

3. **Update Tool Builder**
   - [ ] **File:** `include/mcp_tool.h` (tool_builder class)
     - [ ] Add `with_title(const std::string&)` method
     - [ ] Add `with_output_schema(const json&)` method
   - [ ] **File:** `src/mcp_tool.cpp`
     - [ ] Implement new builder methods
   - [ ] **Run tests:** Verify tool definition tests pass

4. **Define Tool Result Structure**
   - [ ] **File:** `include/mcp_message.h`
     - [ ] Add `ToolResult` struct (optional, or use json)
     - [ ] Define `structuredContent` field support
     - [ ] Add content type definitions for resource_link
   - [ ] Consider: Keep flexible with json or add typed struct

5. **Update Server Tool Handling**
   - [ ] **File:** `src/mcp_server.cpp`
     - [ ] Support `structuredContent` in tool responses
     - [ ] Add optional schema validation
     - [ ] Ensure backward compatibility (content array required)
   - [ ] **Run tests:** Verify result handling tests pass

6. **Create Examples**
   - [ ] **File:** `examples/structured_tool_example.cpp`
     - [ ] Create weather tool with output schema
     - [ ] Demonstrate structured content return
     - [ ] Show both text and structured content
   - [ ] **File:** `examples/CMakeLists.txt`
     - [ ] Add new example to build

7. **Update Existing Examples**
   - [ ] **File:** `examples/server_example.cpp`
     - [ ] Add at least one tool with output schema
     - [ ] Show structured content in handler

8. **Update Tests**
   - [ ] **File:** `test/mcp_test.cpp`
     - [ ] Add structured output integration tests
   - [ ] **Run full test suite:** Ensure no regressions

9. **Update Documentation**
   - [ ] **File:** `README.md`
     - [ ] Add "Structured Tool Output" to features
     - [ ] Add code examples showing usage
     - [ ] Update tool registration examples
   - [ ] **File:** `MIGRATION_GUIDE.md`
     - [ ] Document new optional fields
     - [ ] Explain backward compatibility

10. **Verification**
    - [ ] All tests pass
    - [ ] Tools can define output schemas
    - [ ] Tools can return structured content
    - [ ] Schema validation works
    - [ ] Backward compatibility maintained
    - [ ] Examples demonstrate usage

**Estimated Effort:** 7-10 days

---

### 1.4 Update Version Constant and Global References

**Rationale:** Update the codebase to reflect the new protocol version.

#### Tasks

1. **Update Version Constants**
   - [ ] **File:** `include/mcp_message.h`
     - [ ] Change `MCP_VERSION` from "2025-03-26" to "2025-06-18"
     - [ ] Add comment explaining supported versions
   - [ ] **File:** `CMakeLists.txt`
     - [ ] Update `project(MCP VERSION 2025.03.26 ...)` to `2025.06.18`

2. **Update All Version References**
   - [ ] **File:** `README.md`
     - [ ] Update all references from 2025-03-26 to 2025-06-18
     - [ ] Update specification links
   - [ ] **File:** `SECURITY.md`
     - [ ] Update version references
   - [ ] **Files:** All example files
     - [ ] Update version comments

3. **Update Tests**
   - [ ] **File:** `test/lifecycle_compliance_test.cpp`
     - [ ] Update protocolVersion in test data from "2025-03-26" to "2025-06-18"
   - [ ] **File:** `test/testcase.md`
     - [ ] Update test case version references

4. **Verification**
   - [ ] All version strings updated
   - [ ] Tests use correct version
   - [ ] Documentation consistent

**Estimated Effort:** 1-2 days

---

### Phase 1 Summary Checklist

- [ ] JSON-RPC batching removed and tests verify rejection
- [ ] MCP-Protocol-Version header required and validated
- [ ] Structured tool output implemented with tests
- [ ] Version constants updated to 2025-06-18
- [ ] All Phase 1 tests passing
- [ ] Documentation updated for breaking changes
- [ ] Migration guide created
- [ ] Code review completed
- [ ] Security review (codeql_checker)

**Phase 1 Exit Criteria:**
- ✅ All tests pass (target: 100% pass rate)
- ✅ No regressions in existing functionality
- ✅ Breaking changes documented
- ✅ Migration guide available
- ✅ Code reviewed and approved

---

## Phase 2: High Priority Enhancements
**Goal:** Add recommended features for full 2025-06-18 conformance  
**Duration:** 1-2 weeks  
**Priority:** 🟡 HIGH

### 2.1 Add Resource Links Support

**Rationale:** Allows tools to reference external resources in their results.

#### Tasks

1. **Write Failing Tests** (TDD Step 1)
   - [ ] Create `test/resource_links_test.cpp`
   - [ ] Test: Tool result can include resource_link content type
   - [ ] Test: Resource link has required fields (uri, type)
   - [ ] Test: Resource link supports optional fields (name, description, mimeType)
   - [ ] Test: Resource link supports annotations
   - [ ] **Run tests:** Verify they fail

2. **Define Resource Link Content Type**
   - [ ] **File:** `include/mcp_message.h` or new header
     - [ ] Add `ResourceLinkContent` struct or JSON schema
     - [ ] Define required fields: type, uri
     - [ ] Define optional fields: name, description, mimeType, annotations
   - [ ] **Run tests:** Verify definition tests pass

3. **Update Server Tool Response Handling**
   - [ ] **File:** `src/mcp_server.cpp`
     - [ ] Support `resource_link` in content array
     - [ ] Validate resource link structure
   - [ ] **Run tests:** Verify handling tests pass

4. **Create Example**
   - [ ] **File:** `examples/resource_links_example.cpp` or add to existing
     - [ ] Demonstrate tool returning resource links
     - [ ] Show different resource types (file, HTTP, etc.)

5. **Update Documentation**
   - [ ] **File:** `README.md`
     - [ ] Add resource links documentation
     - [ ] Add code examples

6. **Verification**
   - [ ] All tests pass
   - [ ] Tools can return resource links
   - [ ] Examples demonstrate usage

**Estimated Effort:** 3-4 days

---

### 2.2 Update Security Documentation

**Rationale:** Align with 2025-06-18 security enhancements (OAuth, RFC 8707).

#### Tasks

1. **Update SECURITY.md**
   - [ ] Change title from "MCP 2025-03-26" to "MCP 2025-06-18"
   - [ ] Add OAuth Resource Server section
   - [ ] Add RFC 8707 Resource Indicators documentation
   - [ ] Update references to specification

2. **Verify Security Implementation**
   - [ ] Review Origin header validation (already implemented)
   - [ ] Review DNS rebinding mitigation (already implemented)
   - [ ] Review tool confirmation hooks (already implemented)
   - [ ] Ensure all aligns with 2025-06-18 requirements

3. **Verification**
   - [ ] Security documentation current
   - [ ] Implementation meets 2025-06-18 requirements

**Estimated Effort:** 1-2 days

---

### Phase 2 Summary Checklist

- [ ] Resource links implemented and tested
- [ ] Security documentation updated
- [ ] All Phase 2 tests passing
- [ ] Examples demonstrate new features
- [ ] Code review completed

**Phase 2 Exit Criteria:**
- ✅ Resource links working
- ✅ Security docs updated
- ✅ All tests pass

---

## Phase 3: Optional Enhancements
**Goal:** Implement optional features for enhanced capabilities  
**Duration:** 2-4 weeks  
**Priority:** 🟢 MEDIUM (Optional)

### 3.1 Elicitation Support ✅ COMPLETE

**Rationale:** Allows servers to request additional user input during interactions.

**Status:** ✅ FULLY IMPLEMENTED (2026-02-17)

#### Tasks

1. **Write Tests** ✅
   - [x] Create `test/elicitation_test.cpp` (15 tests)
   - [x] Create `test/elicitation_integration_test.cpp` (8 tests)
   - [x] Test capability declaration
   - [x] Test elicitation request/response flow
   - [x] Test all three actions (accept/decline/cancel)
   - [x] Test timeout handling
   - [x] Test complex schemas

2. **Implement Capability** ✅
   - [x] Add elicitation data structures (elicitation_params, elicitation_result, elicitation_action)
   - [x] Add `elicitation` capability support to server
   - [x] Implement elicitation request mechanism with promise/future
   - [x] Add response handler in process_request()
   - [x] Implement timeout handling
   - [x] Add client_supports_elicitation() method

3. **Create Example** ✅
   - [x] Demonstrate elicitation flow in `examples/elicitation_example.cpp`
   - [x] Show all three response actions
   - [x] Demonstrate complex schemas with multiple types

4. **Documentation** ✅
   - [x] Update README with elicitation feature
   - [x] Update conformance baseline
   - [x] Add API documentation

**Test Results:** 23/23 passing (15 data structure + 8 integration)

**Estimated Effort:** 5-7 days ✅ COMPLETED

---

### 3.2 Meta Field Support (Optional)

**Rationale:** Allows extended metadata on tools and other types.

#### Tasks

1. **Add Meta Field**
   - [ ] Update relevant structures to include `_meta` field
   - [ ] Update JSON serialization

2. **Update Tests**
   - [ ] Test meta field presence
   - [ ] Test meta field in JSON output

**Estimated Effort:** 2-3 days (if implemented)

---

### Phase 3 Summary Checklist

- [ ] Optional features implemented as needed
- [ ] Tests passing
- [ ] Documentation updated

---

## Phase 4: Future - 2025-11-25 Upgrade
**Goal:** Upgrade to latest specification  
**Duration:** 1 week  
**Priority:** ⚪ LOW (Future)

### 4.1 Extensions Support

**Rationale:** Latest spec (2025-11-25) adds extensions field to capabilities.

#### Tasks

1. **Add Extensions Field**
   - [ ] Update `ClientCapabilities` and `ServerCapabilities`
   - [ ] Add `extensions` field support

2. **Update Version**
   - [ ] Change `MCP_VERSION` to "2025-11-25"
   - [ ] Update documentation

**Estimated Effort:** 2-3 days (when ready)

---

## Testing Strategy

### Test Coverage Goals

- **Unit Tests:** 100% of new code
- **Integration Tests:** All major features
- **Regression Tests:** Ensure backward compatibility where applicable
- **Security Tests:** Protocol version validation, input validation

### Test Execution Plan

**After Each Subtask:**
```bash
# Run relevant test suite
cd build && ctest -R <relevant_test_suite> -V
```

**After Each Task:**
```bash
# Run full test suite
cd build && ctest -V
```

**Before Each Phase Completion:**
```bash
# Clean build and full test
rm -rf build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DMCP_BUILD_TESTS=ON -DVCPKG_MANIFEST_FEATURES="tests"
cmake --build build --config Release
cd build && ctest -V
```

### Continuous Integration

- All PRs must pass CI on Linux and Windows
- No test failures allowed
- Security checks (codeql_checker) must pass

---

## Documentation Strategy

### Documents to Update

1. **README.md** - Main project documentation
2. **SECURITY.md** - Security features and best practices
3. **AGENTS.md** - Agent development guide (version references)
4. **MIGRATION_GUIDE.md** - New document for upgrade path

### Documentation Checklist

- [ ] All version references updated
- [ ] Breaking changes documented
- [ ] New features explained with examples
- [ ] API changes documented
- [ ] Migration path provided

---

## Risk Management

### Identified Risks

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Breaking changes affect users | High | High | Provide clear migration guide and version negotiation |
| Tests become flaky | Medium | Low | Follow TDD, verify each change |
| Performance regression | Medium | Low | Benchmark critical paths |
| Security vulnerabilities | High | Low | Run codeql_checker, security review |
| Backward compatibility issues | High | Medium | Maintain version negotiation, extensive testing |

### Rollback Plan

- Git tags for each phase completion
- Feature flags for new capabilities (if needed)
- Version negotiation allows fallback to 2025-03-26

---

## Success Criteria

### Phase 1 Success Criteria
- [ ] JSON-RPC batching completely removed
- [ ] MCP-Protocol-Version header implemented and validated
- [ ] Structured tool output working
- [ ] All tests passing (100%)
- [ ] Zero regressions
- [ ] Documentation updated
- [ ] Migration guide published

### Overall Success Criteria
- [ ] Full 2025-06-18 specification compliance
- [ ] All normative MUST requirements met
- [ ] Backward compatibility for 2025-03-26 clients (via version negotiation)
- [ ] Security requirements satisfied
- [ ] Test coverage >80% for new code
- [ ] CI passing on all platforms
- [ ] Examples demonstrate new features
- [ ] User migration path documented

---

## Timeline

```
Week 1-2:   Phase 1.1 - Remove Batching
Week 2-3:   Phase 1.2 - Protocol Version Header
Week 3-4:   Phase 1.3 - Structured Tool Output
Week 4:     Phase 1.4 - Version Updates
Week 5:     Phase 2.1 - Resource Links
Week 5:     Phase 2.2 - Security Docs
Week 6-7:   Phase 3 - Optional Features (if needed)
Week 7:     Final Review, Documentation, Release
```

**Total Duration:** 6-7 weeks for full 2025-06-18 compliance  
**Minimum Duration:** 4 weeks for critical features only (Phase 1 + 2)

---

## Next Steps

1. **Review this plan** with stakeholders and team
2. **Create GitHub issues** for each phase/task
3. **Set up project board** to track progress
4. **Create feature branch** `feature/mcp-2025-06-18`
5. **Begin Phase 1.1** - Remove JSON-RPC batching support

---

## Appendix: TDD Workflow Reminder

For all implementation tasks, follow strict TDD:

1. **Write failing test** - Define expected behavior
2. **Run test** - Verify it fails for the right reason
3. **Write minimal code** - Make the test pass
4. **Run test** - Verify it passes
5. **Refactor** - Clean up while keeping tests green
6. **Repeat** - Next test case

See [AGENTS.md](./AGENTS.md) for detailed TDD guidelines.

---

**Document Version:** 1.0  
**Last Updated:** 2026-02-16  
**Status:** 📋 DRAFT - Awaiting Approval
