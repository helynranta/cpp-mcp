# MCP Breaking Changes Since 2025-03-26

This document lists all **breaking changes** in the Model Context Protocol (MCP) specification since version 2025-03-26. For each breaking change, we provide a link to the official changelog and a one-line summary of its impact for server implementers.

## Version 2025-06-18 (Released June 18, 2025)

### Breaking Changes

1. **JSON-RPC Batch Removal**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - Servers must reject batch request arrays (previously required) and return error code -32600 for batch attempts.

2. **MCP-Protocol-Version Header Requirement**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - HTTP clients must include `MCP-Protocol-Version` header in all requests after initialization, servers must validate and return 400 Bad Request for invalid/missing versions.

3. **Lifecycle Operation Enforcement**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - Changed lifecycle operation requirements from **SHOULD** to **MUST**, requiring stricter enforcement of state transitions.

4. **Resource Indicators for OAuth**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - MCP clients must implement RFC 8707 Resource Indicators to prevent malicious servers from obtaining access tokens.

### Major Features (Non-Breaking, Backward Compatible)

5. **Structured Tool Output**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - Tools can now declare `outputSchema` and return structured content, but servers must still support text-only content.

6. **Elicitation Support**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - Servers can request user input during tool execution (new capability, optional).

7. **Resource Links**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - Tool results can include resource link content type referencing external resources (additive feature).

8. **OAuth Resource Server Classification**: [changelog](https://modelcontextprotocol.io/specification/2025-06-18/changelog#major-changes) - MCP servers classified as OAuth Resource Servers with protected resource metadata (affects authorization implementations).

## Version 2025-11-25 (Released November 25, 2025)

### Breaking Changes

9. **ElicitResult Schema Update**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - `ElicitResult` and `EnumSchema` changed to standards-based approach supporting titled, untitled, single-select, and multi-select enums (affects elicitation implementations).

10. **Tool Execution Error Classification**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#minor-changes) - Input validation errors must be returned as Tool Execution Errors rather than Protocol Errors to enable model self-correction (changes error handling patterns).

11. **HTTP 403 for Invalid Origin**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#minor-changes) - Servers must respond with HTTP 403 Forbidden (not 400) for invalid Origin headers in Streamable HTTP transport.

12. **OAuth 2.0 Discovery Changes**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#minor-changes) - OAuth Protected Resource Metadata discovery aligned with RFC 9728, making `WWW-Authenticate` header optional with fallback to `.well-known` endpoint.

### Major Features (Non-Breaking, Backward Compatible)

13. **OpenID Connect Discovery**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - Enhanced authorization server discovery with OpenID Connect Discovery 1.0 support (optional enhancement).

14. **Icon Support**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - Servers can expose icons as metadata for tools, resources, and prompts (additive feature).

15. **Incremental Scope Consent**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - Authorization flows enhanced with incremental scope consent via `WWW-Authenticate` (optional OAuth enhancement).

16. **URL Mode Elicitation**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - Added URL mode for elicitation requests (extends existing elicitation feature).

17. **Tool Calling in Sampling**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - Sampling supports tool calling via `tools` and `toolChoice` parameters (optional sampling enhancement).

18. **OAuth Client ID Metadata**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - Support for OAuth Client ID Metadata Documents as recommended client registration (optional OAuth enhancement).

19. **Experimental Tasks Support**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#major-changes) - Experimental support for durable requests with polling and deferred retrieval (experimental, not required).

20. **SSE Polling Support**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#minor-changes) - Servers can disconnect SSE streams at will to support polling patterns (affects streaming implementations).

21. **JSON Schema 2020-12**: [changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog#minor-changes) - Established JSON Schema 2020-12 as the default dialect for MCP schema definitions (affects schema validation).

## Version Draft (Ongoing)

### Breaking Changes

No breaking changes in current draft.

### Minor Changes

22. **Extensions Field**: [changelog](https://modelcontextprotocol.io/specification/draft/changelog#minor-changes) - Added `extensions` field to `ClientCapabilities` and `ServerCapabilities` for optional extensions beyond core protocol (additive, non-breaking).

## Summary: Critical Breaking Changes for Server Implementers

The following are the most critical breaking changes that **must** be addressed to maintain protocol compliance:

### High Priority (Required for 2025-06-18)
- **JSON-RPC Batch Removal** (#1) - Must reject batch requests
- **MCP-Protocol-Version Header** (#2) - Must validate header in all HTTP requests
- **Lifecycle SHOULD → MUST** (#3) - Must enforce strict state transitions

### Medium Priority (Affects Specific Features)
- **Resource Indicators** (#4) - Required for OAuth implementations
- **OAuth Resource Server Classification** (#8) - Affects authorization metadata
- **ElicitResult Schema** (#9) - Required if implementing elicitation
- **Tool Execution Errors** (#10) - Changes error handling patterns
- **HTTP 403 for Invalid Origin** (#11) - Affects HTTP transport security
- **OAuth Discovery Changes** (#12) - Affects OAuth implementations
- **SSE Polling** (#20) - Affects streaming implementations
- **JSON Schema Dialect** (#21) - Affects schema validation

## Resources

- **MCP 2025-06-18 Changelog**: https://modelcontextprotocol.io/specification/2025-06-18/changelog
- **MCP 2025-11-25 Changelog**: https://modelcontextprotocol.io/specification/2025-11-25/changelog
- **MCP Draft Changelog**: https://modelcontextprotocol.io/specification/draft/changelog
- **MCP Specification Repository**: https://github.com/modelcontextprotocol/specification
- **Python SDK Changelog**: https://github.com/modelcontextprotocol/python-mcp/blob/main/CHANGELOG.md

---

**Document Version**: 1.0  
**Last Updated**: 2026-02-17  
**Scope**: Breaking changes from 2025-03-26 to current draft
