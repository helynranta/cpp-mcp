# MCP Library Update Plan: 2024-11-05 → 2025-03-26

This document outlines the comprehensive plan to update the cpp-mcp library from the 2024-11-05 specification to the 2025-03-26 specification, ensuring compatibility with both GitHub Copilot and Claude Code.

## Overview

**Current Version:** 2024-11-05  
**Target Version:** 2025-03-26  
**Reason for Update:** The 2024-11-05 spec is outdated. The 2025-03-26 version includes critical security improvements, better transport mechanisms, and enhanced features while maintaining broad compatibility with modern AI tools.

## Key Changes Summary

The 2025-03-26 specification introduces several major improvements:

1. **Security:** OAuth 2.1 with PKCE, mandatory HTTPS, token binding
2. **Transport:** Streamable HTTP replaces SSE as primary transport
3. **Features:** Enhanced metadata, progress notifications, multimodal support
4. **Session Management:** Reliable reconnection and state recovery
5. **Multi-user Support:** Formalized roles and authentication

---

## GitHub Issues Structure

Below are the issues to be created in the GitHub repository. Each can be assigned to different agents/developers.

---

### Issue #1: Core Protocol Version Update

**Title:** Update MCP Protocol Version to 2025-03-26

**Description:**
Update the core protocol version constant and all references throughout the codebase.

**Tasks:**
- [ ] Update `MCP_VERSION` constant from "2024-11-05" to "2025-03-26"
- [ ] Update all comments and documentation referencing the old version
- [ ] Update CMakeLists.txt project version
- [ ] Update README.md with new version information
- [ ] Verify all version strings are updated

**Files to Modify:**
- `include/mcp_message.h`
- `CMakeLists.txt`
- `README.md`
- All source files with version comments

**Priority:** High  
**Dependencies:** None  
**Estimated Effort:** 1-2 hours

---

### Issue #2: Implement Streamable HTTP Transport

**Title:** Add Streamable HTTP Transport as Primary Transport Mechanism

**Description:**
The 2025-03-26 spec introduces Streamable HTTP as the primary transport mechanism, replacing SSE. This provides bidirectional communication and better reconnection handling.

**Tasks:**
- [ ] Research Streamable HTTP protocol details
- [ ] Create new transport class `mcp_streamable_http_client.h/cpp`
- [ ] Implement bidirectional communication
- [ ] Add reconnection logic
- [ ] Handle network interruptions gracefully
- [ ] Update server to support Streamable HTTP
- [ ] Keep SSE as optional fallback transport
- [ ] Add configuration options for transport selection

**Files to Create:**
- `include/mcp_streamable_http_client.h`
- `src/mcp_streamable_http_client.cpp`

**Files to Modify:**
- `include/mcp_client.h` (add new transport option)
- `include/mcp_server.h` (support new transport)
- `src/mcp_server.cpp`

**Priority:** High  
**Dependencies:** Issue #1  
**Estimated Effort:** 2-3 days

---

### Issue #3: OAuth 2.1 with PKCE Implementation

**Title:** Upgrade OAuth to 2.1 with PKCE and Remove Implicit Flow

**Description:**
Upgrade authentication from OAuth 2.0 to OAuth 2.1, implementing PKCE (Proof Key for Code Exchange) for enhanced security. Remove deprecated implicit flow.

**Tasks:**
- [ ] Implement PKCE challenge generation
- [ ] Add code verifier and challenge support
- [ ] Update authorization flow to use authorization code + PKCE
- [ ] Remove implicit flow code (deprecated)
- [ ] Add token binding support
- [ ] Implement secure token storage
- [ ] Add short-lived token rotation
- [ ] Enforce HTTPS for all OAuth flows

**Files to Create:**
- `include/mcp_oauth.h`
- `src/mcp_oauth.cpp`

**Files to Modify:**
- `include/mcp_client.h`
- `include/mcp_sse_client.h`
- `src/mcp_sse_client.cpp`
- `include/mcp_server.h`
- `src/mcp_server.cpp`

**Priority:** High  
**Dependencies:** Issue #1  
**Estimated Effort:** 3-4 days

---

### Issue #4: Session Management with Mcp-Session-Id

**Title:** Implement Session Management and State Recovery

**Description:**
Add session management with `Mcp-Session-Id` header support for reliable reconnections and state recovery.

**Tasks:**
- [ ] Add session ID generation
- [ ] Implement `Mcp-Session-Id` header in requests/responses
- [ ] Add session state storage
- [ ] Implement session recovery on reconnection
- [ ] Add session timeout handling
- [ ] Add session cleanup on disconnect
- [ ] Update client implementations to track sessions
- [ ] Update server to maintain session state

**Files to Create:**
- `include/mcp_session.h`
- `src/mcp_session.cpp`

**Files to Modify:**
- `include/mcp_client.h`
- `include/mcp_server.h`
- `src/mcp_server.cpp`
- `src/mcp_sse_client.cpp`
- `src/mcp_stdio_client.cpp`

**Priority:** Medium  
**Dependencies:** Issue #2  
**Estimated Effort:** 2-3 days

---

### Issue #5: Enhanced Tool Metadata and Annotations

**Title:** Add Tool Metadata Annotations for Security and UI

**Description:**
Implement rich tool metadata including operational flags (read-only, destructive) and display metadata for better security and UX.

**Tasks:**
- [ ] Extend tool schema to include annotations
- [ ] Add `readOnly` flag support
- [ ] Add `destructive` flag support
- [ ] Add `requiresAuth` flag support
- [ ] Add cost estimates metadata
- [ ] Add latency estimates metadata
- [ ] Add icon/display metadata
- [ ] Update tool builder API
- [ ] Update examples to use new annotations

**Files to Modify:**
- `include/mcp_tool.h`
- `src/mcp_tool.cpp`
- `examples/server_example.cpp`

**Priority:** Medium  
**Dependencies:** Issue #1  
**Estimated Effort:** 1-2 days

---

### Issue #6: Enhanced Progress Notifications

**Title:** Implement Dynamic Progress Notifications with Text Messages

**Description:**
Upgrade progress notification system to support dynamic text messages in addition to numeric progress.

**Tasks:**
- [ ] Extend progress notification structure
- [ ] Add text/status message field
- [ ] Implement progress update notifications
- [ ] Add progress cancellation support
- [ ] Update client progress handlers
- [ ] Add progress notification examples
- [ ] Document progress notification format

**Files to Create:**
- `include/mcp_progress.h`
- `src/mcp_progress.cpp`

**Files to Modify:**
- `include/mcp_client.h`
- `include/mcp_server.h`
- `src/mcp_server.cpp`

**Priority:** Low  
**Dependencies:** Issue #1  
**Estimated Effort:** 1-2 days

---

### Issue #7: Multimodal Support - Audio Streams

**Title:** Add Native Audio Stream Support

**Description:**
Extend the protocol to support audio data streams in addition to text and images.

**Tasks:**
- [ ] Define audio data structure
- [ ] Add audio MIME type support
- [ ] Implement audio streaming in resources
- [ ] Implement audio in tool responses
- [ ] Add audio encoding/decoding utilities
- [ ] Update content type handling
- [ ] Add audio examples
- [ ] Document audio format requirements

**Files to Modify:**
- `include/mcp_resource.h`
- `src/mcp_resource.cpp`
- `include/mcp_tool.h`
- `src/mcp_tool.cpp`
- `include/mcp_message.h`

**Priority:** Low  
**Dependencies:** Issue #1  
**Estimated Effort:** 2-3 days

---

### Issue #8: Parameter Completion Support

**Title:** Implement Parameter Auto-completion Suggestions

**Description:**
Add support for suggesting auto-completion values for tool parameter fields.

**Tasks:**
- [ ] Add completion endpoint to protocol
- [ ] Implement parameter completion handler
- [ ] Add completion suggestions to tool schema
- [ ] Update tool builder to support completions
- [ ] Add client method for completion requests
- [ ] Add examples demonstrating completions
- [ ] Document completion API

**Files to Modify:**
- `include/mcp_tool.h`
- `src/mcp_tool.cpp`
- `include/mcp_client.h`
- `include/mcp_server.h`
- `src/mcp_server.cpp`

**Priority:** Low  
**Dependencies:** Issue #1  
**Estimated Effort:** 1-2 days

---

### Issue #9: HTTPS Enforcement and Security Hardening

**Title:** Enforce HTTPS and Implement Security Best Practices

**Description:**
Make HTTPS mandatory for production use and implement security best practices.

**Tasks:**
- [ ] Add HTTPS validation in client connections
- [ ] Update default configurations to use HTTPS
- [ ] Add certificate validation options
- [ ] Implement TLS 1.3 minimum requirement
- [ ] Add security warnings for HTTP connections
- [ ] Update documentation on security requirements
- [ ] Add security best practices guide
- [ ] Review and update all crypto operations

**Files to Modify:**
- `include/mcp_sse_client.h`
- `src/mcp_sse_client.cpp`
- `include/mcp_server.h`
- `src/mcp_server.cpp`
- `README.md`

**Priority:** High  
**Dependencies:** Issue #1  
**Estimated Effort:** 1-2 days

---

### Issue #10: Multi-user Support and Role Management

**Title:** Implement Multi-user Support with Role Definitions

**Description:**
Add support for multiple users and formalize builder vs. end-user roles.

**Tasks:**
- [ ] Add user identification system
- [ ] Implement role-based access control (RBAC)
- [ ] Add builder role support
- [ ] Add end-user role support
- [ ] Implement per-user authorization
- [ ] Add user context to requests
- [ ] Update session management for multi-user
- [ ] Document role-based workflows

**Files to Create:**
- `include/mcp_auth.h`
- `src/mcp_auth.cpp`

**Files to Modify:**
- `include/mcp_server.h`
- `src/mcp_server.cpp`
- `include/mcp_session.h`
- `src/mcp_session.cpp`

**Priority:** Medium  
**Dependencies:** Issue #3, Issue #4  
**Estimated Effort:** 3-4 days

---

### Issue #11: Update and Extend Tests

**Title:** Update Test Suite for 2025-03-26 Specification

**Description:**
Update existing tests and add new tests for all new features.

**Tasks:**
- [ ] Update existing tests for protocol changes
- [ ] Add tests for Streamable HTTP transport
- [ ] Add tests for OAuth 2.1 with PKCE
- [ ] Add tests for session management
- [ ] Add tests for tool annotations
- [ ] Add tests for progress notifications
- [ ] Add tests for audio support
- [ ] Add tests for parameter completion
- [ ] Add integration tests
- [ ] Add security tests
- [ ] Ensure backward compatibility tests pass

**Files to Modify:**
- `test/mcp_test.cpp`
- Create new test files as needed

**Priority:** High  
**Dependencies:** All implementation issues  
**Estimated Effort:** 3-5 days

---

### Issue #12: Documentation Update

**Title:** Update All Documentation for 2025-03-26 Specification

**Description:**
Comprehensive documentation update covering all changes and new features.

**Tasks:**
- [ ] Update README.md with new spec version
- [ ] Document all new features
- [ ] Update API documentation
- [ ] Update code examples
- [ ] Create migration guide from 2024-11-05 to 2025-03-26
- [ ] Document breaking changes
- [ ] Update security documentation
- [ ] Add troubleshooting guide
- [ ] Update build instructions if needed
- [ ] Review and update all code comments

**Files to Modify:**
- `README.md`
- All example files
- All header files (comments)
- Create `MIGRATION_GUIDE.md`
- Create `SECURITY.md`

**Priority:** Medium  
**Dependencies:** All implementation issues  
**Estimated Effort:** 2-3 days

---

### Issue #13: Remove Deprecated Features

**Title:** Clean Up Deprecated Features from Old Specification

**Description:**
Remove features that have been deprecated in the new specification.

**Tasks:**
- [ ] Review all deprecated features
- [ ] Remove SSE as primary transport (keep as optional)
- [ ] Remove OAuth 2.0 implicit flow code
- [ ] Remove old progress notification format
- [ ] Remove JSON-RPC batching requirement (if present)
- [ ] Clean up unused code
- [ ] Update changelog
- [ ] Document removed features in migration guide

**Files to Modify:**
- Various files based on deprecated feature locations

**Priority:** Low  
**Dependencies:** Issue #2, Issue #3, Issue #6  
**Estimated Effort:** 1-2 days

---

### Issue #14: Performance Testing and Optimization

**Title:** Performance Testing and Optimization for New Features

**Description:**
Ensure the updated library maintains or improves performance.

**Tasks:**
- [ ] Create performance benchmarks
- [ ] Test Streamable HTTP vs SSE performance
- [ ] Profile authentication overhead
- [ ] Test session management performance
- [ ] Optimize memory usage
- [ ] Optimize network operations
- [ ] Test with high concurrency
- [ ] Test with large payloads
- [ ] Document performance characteristics
- [ ] Create performance comparison report

**Files to Create:**
- `test/performance_test.cpp`
- `PERFORMANCE.md`

**Priority:** Medium  
**Dependencies:** All implementation issues  
**Estimated Effort:** 2-3 days

---

### Issue #15: GitHub Copilot and Claude Code Compatibility Testing

**Title:** Validate Compatibility with GitHub Copilot and Claude Code

**Description:**
Test the updated library with both GitHub Copilot and Claude Code to ensure full compatibility.

**Tasks:**
- [ ] Set up test environment with GitHub Copilot
- [ ] Set up test environment with Claude Code
- [ ] Test basic connectivity
- [ ] Test tool invocation
- [ ] Test resource access
- [ ] Test session management
- [ ] Test authentication flows
- [ ] Test error handling
- [ ] Document any compatibility issues
- [ ] Create compatibility matrix
- [ ] Add examples for both platforms

**Files to Create:**
- `COMPATIBILITY.md`
- Example configurations for both platforms

**Priority:** High  
**Dependencies:** All implementation issues  
**Estimated Effort:** 2-3 days

---

## Implementation Phases

### Phase 1: Core Updates (Weeks 1-2)
- Issue #1: Protocol Version Update
- Issue #9: HTTPS Enforcement
- Issue #2: Streamable HTTP Transport
- Issue #3: OAuth 2.1 with PKCE

### Phase 2: Feature Enhancements (Weeks 3-4)
- Issue #4: Session Management
- Issue #5: Tool Metadata
- Issue #6: Progress Notifications
- Issue #8: Parameter Completion

### Phase 3: Advanced Features (Week 5)
- Issue #7: Audio Support
- Issue #10: Multi-user Support

### Phase 4: Quality Assurance (Weeks 6-7)
- Issue #11: Test Suite Update
- Issue #14: Performance Testing
- Issue #15: Compatibility Testing
- Issue #13: Remove Deprecated Features

### Phase 5: Documentation (Week 8)
- Issue #12: Documentation Update

---

## Success Criteria

- [ ] All tests passing
- [ ] Compatible with GitHub Copilot
- [ ] Compatible with Claude Code
- [ ] All deprecated features removed
- [ ] Performance maintained or improved
- [ ] Complete documentation
- [ ] Migration guide available
- [ ] Security requirements met

---

## Risk Assessment

**High Risk:**
- OAuth 2.1 implementation complexity
- Streamable HTTP transport reliability
- Backward compatibility concerns

**Medium Risk:**
- Session management state consistency
- Performance regression
- Multi-user implementation complexity

**Low Risk:**
- Documentation updates
- Tool metadata additions
- Progress notification enhancements

---

## Resources

- [MCP Specification 2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26)
- [MCP GitHub Repository](https://github.com/modelcontextprotocol/modelcontextprotocol)
- [OAuth 2.1 Specification](https://datatracker.ietf.org/doc/html/draft-ietf-oauth-v2-1-07)
- [Streamable HTTP Documentation](https://modelcontextprotocol.io/specification/2025-03-26/transport/http)

---

## Notes

- Each issue can be assigned to a different agent/developer
- Issues can be worked on in parallel where there are no dependencies
- Some issues may need to be broken down further during implementation
- Regular integration testing should occur throughout the process
- Maintain backward compatibility where possible during transition
