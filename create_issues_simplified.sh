#!/bin/bash
# Simplified script to create GitHub issues for MCP library update (Local Use)
# This version focuses on essential updates for personal/local development
# Skips authentication, HTTPS enforcement, and enterprise features
#
# Usage: ./create_issues_simplified.sh
# Requires: gh CLI tool authenticated to helynranta/cpp-mcp repository

set -e

REPO="helynranta/cpp-mcp"

echo "Creating GitHub issues for MCP library update (Simplified for Local Use)..."
echo "Repository: $REPO"
echo ""
echo "Note: This plan skips authentication and enterprise features since"
echo "      this is for personal use in a local environment."
echo ""

# Function to create an issue
create_issue() {
    local title="$1"
    local body="$2"
    local labels="$3"
    
    echo "Creating issue: $title"
    gh issue create \
        --repo "$REPO" \
        --title "$title" \
        --body "$body" \
        --label "$labels" || echo "Failed to create issue: $title"
}

# Issue #1: Core Protocol Version Update (HIGH PRIORITY - ESSENTIAL)
create_issue \
    "Update MCP Protocol Version to 2025-03-26" \
    "**Description:**
Update the core protocol version constant and all references throughout the codebase.

**Priority:** ⭐ HIGH - Essential for compatibility

**Tasks:**
- [ ] Update \`MCP_VERSION\` constant from \"2024-11-05\" to \"2025-03-26\"
- [ ] Update all comments and documentation referencing the old version
- [ ] Update CMakeLists.txt project version
- [ ] Update README.md with new version information
- [ ] Verify all version strings are updated

**Files to Modify:**
- \`include/mcp_message.h\`
- \`CMakeLists.txt\`
- \`README.md\`

**Estimated Effort:** 1-2 hours

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` for full details." \
    "priority: high,enhancement,phase-1-core"

# Issue #2: Enhanced Tool Metadata (HIGH PRIORITY - RECOMMENDED)
create_issue \
    "Add Tool Metadata Annotations for Better Developer Experience" \
    "**Description:**
Implement tool metadata to improve the development experience with richer tool information.

**Priority:** ⭐ HIGH - Recommended for better DX

**Tasks:**
- [ ] Extend tool schema to include annotations
- [ ] Add \`readOnly\` flag support (optional metadata)
- [ ] Add \`destructive\` flag support (helpful warning)
- [ ] Add cost estimates metadata (optional)
- [ ] Add latency estimates metadata (optional)
- [ ] Add description enhancements
- [ ] Update tool builder API
- [ ] Update examples to use new annotations

**Note:** Focus on helpful metadata for local development. Skip auth-related flags.

**Files to Modify:**
- \`include/mcp_tool.h\`
- \`src/mcp_tool.cpp\`
- \`examples/server_example.cpp\`

**Estimated Effort:** 1-2 days

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` and \`TECHNICAL_ARCHITECTURE.md\` for details." \
    "priority: high,enhancement,phase-1-core"

# Issue #3: Enhanced Progress Notifications (HIGH PRIORITY - RECOMMENDED)
create_issue \
    "Implement Dynamic Progress Notifications with Text Messages" \
    "**Description:**
Upgrade progress notification system to support dynamic text messages for better feedback during long-running operations.

**Priority:** ⭐ HIGH - Recommended for better user feedback

**Tasks:**
- [ ] Extend progress notification structure
- [ ] Add text/status message field
- [ ] Implement progress update notifications
- [ ] Add progress cancellation support
- [ ] Update client progress handlers
- [ ] Add progress notification examples
- [ ] Document progress notification format

**Files to Create:**
- \`include/mcp_progress.h\`
- \`src/mcp_progress.cpp\`

**Files to Modify:**
- \`include/mcp_client.h\`
- \`include/mcp_server.h\`
- \`src/mcp_server.cpp\`

**Estimated Effort:** 1-2 days

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` and \`TECHNICAL_ARCHITECTURE.md\` for details." \
    "priority: high,enhancement,phase-1-core"

# Issue #4: Simplified Session Management (MEDIUM PRIORITY - OPTIONAL)
create_issue \
    "Add Basic Session Management for State Tracking" \
    "**Description:**
Add lightweight session management for tracking state in local development. No complex authentication or multi-user features needed.

**Priority:** 🔵 MEDIUM - Optional quality of life improvement

**Tasks:**
- [ ] Add simple session ID generation
- [ ] Add optional \`Mcp-Session-Id\` header support
- [ ] Add basic session state storage (in-memory is fine)
- [ ] Add session cleanup on disconnect
- [ ] Keep implementation simple for local use

**Note:** Skip complex features like session recovery, multi-user, timeouts for enterprise use.

**Files to Create:**
- \`include/mcp_session.h\` (simplified version)
- \`src/mcp_session.cpp\` (simplified version)

**Files to Modify:**
- \`include/mcp_server.h\`
- \`src/mcp_server.cpp\`

**Estimated Effort:** 1 day

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` for details." \
    "priority: medium,enhancement,phase-2-features"

# Issue #5: Parameter Completion Support (MEDIUM PRIORITY - OPTIONAL)
create_issue \
    "Implement Parameter Auto-completion Suggestions" \
    "**Description:**
Add support for suggesting auto-completion values for tool parameters - improves developer experience.

**Priority:** 🔵 MEDIUM - Optional DX improvement

**Tasks:**
- [ ] Add completion endpoint to protocol
- [ ] Implement parameter completion handler
- [ ] Add completion suggestions to tool schema
- [ ] Update tool builder to support completions
- [ ] Add client method for completion requests
- [ ] Add examples demonstrating completions
- [ ] Document completion API

**Files to Modify:**
- \`include/mcp_tool.h\`
- \`src/mcp_tool.cpp\`
- \`include/mcp_client.h\`
- \`include/mcp_server.h\`
- \`src/mcp_server.cpp\`

**Estimated Effort:** 1-2 days

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` for details." \
    "priority: medium,enhancement,phase-2-features"

# Issue #6: Streamable HTTP Transport (MEDIUM PRIORITY - OPTIONAL)
create_issue \
    "Add Streamable HTTP Transport Option" \
    "**Description:**
Add Streamable HTTP as an alternative transport. SSE works fine for local use, but this provides better performance and is the spec's primary transport.

**Priority:** 🔵 MEDIUM - Optional performance improvement

**Tasks:**
- [ ] Research Streamable HTTP protocol details
- [ ] Create new transport class \`mcp_streamable_http_client.h/cpp\`
- [ ] Implement bidirectional communication
- [ ] Keep SSE as default for backward compatibility
- [ ] Add configuration option to choose transport
- [ ] Update examples showing both transports

**Note:** This is optional - SSE works perfectly fine for local development.

**Files to Create:**
- \`include/mcp_streamable_http_client.h\`
- \`src/mcp_streamable_http_client.cpp\`

**Estimated Effort:** 2-3 days

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` and \`TECHNICAL_ARCHITECTURE.md\` for details." \
    "priority: medium,enhancement,phase-3-optional"

# Issue #7: Audio Support (LOW PRIORITY - OPTIONAL)
create_issue \
    "Add Native Audio Stream Support (Optional)" \
    "**Description:**
Extend the protocol to support audio data streams if you plan to use audio features.

**Priority:** ⚪ LOW - Only if you need audio

**Tasks:**
- [ ] Define audio data structure
- [ ] Add audio MIME type support
- [ ] Implement audio streaming in resources
- [ ] Implement audio in tool responses
- [ ] Add audio examples
- [ ] Document audio format requirements

**Note:** Only implement if you need audio features in your local projects.

**Files to Modify:**
- \`include/mcp_resource.h\`
- \`src/mcp_resource.cpp\`
- \`include/mcp_tool.h\`
- \`src/mcp_tool.cpp\`

**Estimated Effort:** 2-3 days

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` for details." \
    "priority: low,enhancement,phase-3-optional"

# Issue #8: Update Test Suite (HIGH PRIORITY)
create_issue \
    "Update Test Suite for 2025-03-26 Specification" \
    "**Description:**
Update existing tests and add new tests for implemented features.

**Priority:** ⭐ HIGH - Essential for quality

**Tasks:**
- [ ] Update existing tests for protocol version change
- [ ] Add tests for tool metadata
- [ ] Add tests for progress notifications
- [ ] Add tests for session management (if implemented)
- [ ] Add tests for new transport (if implemented)
- [ ] Add tests for audio support (if implemented)
- [ ] Add tests for parameter completion (if implemented)
- [ ] Ensure backward compatibility

**Files to Modify:**
- \`test/mcp_test.cpp\`

**Dependencies:** All implemented feature issues

**Estimated Effort:** 2-3 days

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` for details." \
    "priority: high,testing,phase-4-testing"

# Issue #9: Documentation Update (HIGH PRIORITY)
create_issue \
    "Update Documentation for 2025-03-26 Specification" \
    "**Description:**
Update all documentation to reflect the new spec version and implemented features.

**Priority:** ⭐ HIGH - Essential for usability

**Tasks:**
- [ ] Update README.md with new spec version
- [ ] Document all implemented features
- [ ] Update API documentation
- [ ] Update code examples
- [ ] Create simple migration guide
- [ ] Update build instructions if needed
- [ ] Add note about skipped features (auth, HTTPS, multi-user)
- [ ] Document local development setup
- [ ] Note that HTTP localhost is fine for local use

**Files to Modify:**
- \`README.md\`
- All example files
- Create \`MIGRATION_GUIDE.md\`

**Dependencies:** All implemented feature issues

**Estimated Effort:** 1-2 days

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` for details." \
    "priority: high,documentation,phase-4-testing"

# Issue #10: Clean Up Deprecated Features (LOW PRIORITY - OPTIONAL)
create_issue \
    "Clean Up Deprecated Features (Optional)" \
    "**Description:**
Clean up features that were deprecated in the new spec, if desired.

**Priority:** ⚪ LOW - Optional cleanup

**Tasks:**
- [ ] Review deprecated features
- [ ] Decide which to keep for backward compatibility
- [ ] Mark SSE as \"legacy but supported\" if keeping it
- [ ] Remove any unused code
- [ ] Update changelog

**Note:** This is low priority for local use. Keeping old features for compatibility is fine.

**Estimated Effort:** 1 day

See \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` for details." \
    "priority: low,cleanup,phase-4-testing"

# Create umbrella issue
create_issue \
    "MCP Library Update: 2024-11-05 → 2025-03-26 (Simplified for Local Use)" \
    "# MCP Library Specification Update (Local Development Focus)

This is the umbrella issue tracking the update of cpp-mcp from MCP spec 2024-11-05 to 2025-03-26, **optimized for personal use in a local environment**.

## Context
- **Current Version:** 2024-11-05
- **Target Version:** 2025-03-26
- **Use Case:** Personal use in local environment
- **Authentication:** Not needed (local use only)
- **Goal:** Ensure compatibility with GitHub Copilot and Claude Code locally

## What We're Implementing

### Essential (High Priority)
- [ ] #[1] Update MCP Protocol Version
- [ ] #[2] Enhanced Tool Metadata
- [ ] #[3] Enhanced Progress Notifications
- [ ] #[8] Update Test Suite
- [ ] #[9] Documentation Update

### Optional Quality of Life (Medium Priority)
- [ ] #[4] Simplified Session Management
- [ ] #[5] Parameter Completion
- [ ] #[6] Streamable HTTP Transport

### Optional Features (Low Priority)
- [ ] #[7] Audio Support (only if needed)
- [ ] #[10] Clean Up Deprecated Features

## What We're Skipping

Since this is for local personal use, we're **skipping** these enterprise features:
- ❌ OAuth 2.1 with PKCE (no auth needed)
- ❌ HTTPS enforcement (HTTP localhost is fine)
- ❌ Multi-user support (single user)
- ❌ Token binding & secure storage (no auth)
- ❌ RBAC & user roles (not needed)
- ❌ Enterprise session recovery
- ❌ Security audit logging

## Planning Documents
- \`MCP_UPDATE_PLAN_SIMPLIFIED.md\` - Simplified plan for local use
- \`MCP_UPDATE_PLAN.md\` - Full plan (includes optional enterprise features)
- \`TECHNICAL_ARCHITECTURE.md\` - Technical implementation guide

## Timeline
**Minimum Viable Update:** 1 week (essential items only)
**Full Update with Optional Features:** 2-4 weeks

## Resources
- [MCP Specification 2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26)
- Repository planning documents in root directory" \
    "epic,enhancement"

echo ""
echo "✅ All issues created successfully!"
echo ""
echo "Created 11 issues total:"
echo "  - 5 High Priority (Essential)"
echo "  - 3 Medium Priority (Optional QoL)"
echo "  - 3 Low Priority (Optional)"
echo ""
echo "Minimum viable update: Issues #1, #2, #3, #8, #9 (1 week)"
echo "Full update with optional features: All issues (2-4 weeks)"
echo ""
echo "View issues at: https://github.com/$REPO/issues"
