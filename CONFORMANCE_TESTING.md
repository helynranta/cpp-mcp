# Running Official MCP Conformance Tests

This guide explains how to run the official MCP conformance test framework against the C++ MCP implementation.

## Overview

The official MCP conformance framework is available at:
- Repository: https://github.com/modelcontextprotocol/conformance
- NPM Package: `@modelcontextprotocol/conformance`

The framework provides automated testing of MCP implementations against the official protocol specification.

## Prerequisites

```bash
# Ensure Node.js and npm are installed
node --version  # Should be 18+
npm --version

# Build your C++ MCP server
cd /path/to/cpp-mcp
cmake --preset dev-release
cmake --build --preset dev-release
```

## Running Server Conformance Tests

### Step 1: Start Your C++ Server

Start the C++ MCP server in one terminal:

```bash
# Using the server example
./build/dev-release/examples/server_example

# Server will start on http://localhost:8080/mcp by default
```

Or customize the port:

```bash
# Custom port
./build/dev-release/examples/server_example --port 3001
# Server available at http://localhost:3001/mcp
```

### Step 2: Run Conformance Tests

In another terminal, run the conformance framework:

```bash
# Run all active server scenarios
npx @modelcontextprotocol/conformance server --url http://localhost:8080/mcp

# Run a specific scenario
npx @modelcontextprotocol/conformance server \
  --url http://localhost:8080/mcp \
  --scenario server-initialize

# Run with verbose output
npx @modelcontextprotocol/conformance server \
  --url http://localhost:8080/mcp \
  --verbose
```

### Step 3: Review Results

Results are saved to `results/server-<scenario>-<timestamp>/`:
- `checks.json` - Pass/fail status for each conformance check
- Check exit code: 0 = all tests passed, 1 = failures detected

## Available Server Scenarios

The official conformance framework includes these server scenarios:

### Core Lifecycle
- `server-initialize` - Basic server initialization handshake ✅ **Covered**
- `server-sse-polling` - SSE connection polling behavior ✅ **Covered**
- `server-sse-multiple-streams` - Multiple concurrent SSE streams ✅ **Covered**

### Tools
- `tools-list` - List available tools ✅ **Covered**
- `tools-call-simple-text` - Simple tool invocation with text response ✅ **Covered**
- `tools-call-image` - Tool returning image content ⚠️ **Partial** (images supported)
- `tools-call-mixed-content` - Tool returning mixed content types ✅ **Covered**
- `tools-call-with-logging` - Tool with logging notifications ✅ **Covered**
- `tools-call-error` - Tool error handling ✅ **Covered**
- `tools-call-with-progress` - Tool with progress notifications ⚠️ **Partial**
- `tools-call-sampling` - Tool with LLM sampling ❌ **Not Implemented**
- `tools-call-elicitation` - Tool with user input elicitation ❌ **Not Implemented**
- `tools-call-audio` - Tool returning audio content ⚠️ **Partial** (audio supported)
- `tools-call-embedded-resource` - Tool referencing embedded resources ✅ **Covered**

### Resources
- `resources-list` - List available resources ✅ **Covered**
- `resources-read-text` - Read text resource ✅ **Covered**
- `resources-read-binary` - Read binary resource ✅ **Covered**
- `resources-templates-read` - Read templated resources ✅ **Covered**
- `resources-subscribe` - Subscribe to resource changes ⚠️ **Partial**
- `resources-unsubscribe` - Unsubscribe from resource changes ⚠️ **Partial**

### Prompts
- `prompts-list` - List available prompts ✅ **Covered**
- `prompts-get-simple` - Get simple prompt ✅ **Covered**
- `prompts-get-with-args` - Get prompt with arguments ✅ **Covered**
- `prompts-get-embedded-resource` - Prompt with embedded resources ✅ **Covered**
- `prompts-get-with-image` - Prompt with image content ✅ **Covered**

### Security & Advanced
- `dns-rebinding-protection` - DNS rebinding attack mitigation ✅ **Covered**
- `json-schema-2020-12` - JSON Schema 2020-12 support ✅ **Covered**
- `elicitation-sep1034-defaults` - Elicitation default handling ❌ **Not Implemented**
- `elicitation-sep1330-enums` - Elicitation enum handling ❌ **Not Implemented**

## Coverage Mapping

### What We Cover (✅)

Our C++ implementation and test suite cover:

**Core Protocol (100%)**
- JSON-RPC 2.0 messaging
- Protocol version negotiation (2025-03-26, 2025-06-18, 2025-11-25)
- MCP-Protocol-Version header validation
- Batch request rejection
- Lifecycle management (initialize, initialized, shutdown)
- Session management
- SSE streaming

**Tools (80%)**
- Tool registration and listing
- Tool invocation with structured parameters
- Structured tool output (title, outputSchema, structuredContent)
- Error handling
- Multiple content types (text, images, audio, resources)
- Tool safety and validation

**Resources (90%)**
- Resource registration and listing
- Text and binary resource reading
- Resource templates
- Resource URIs and schemas

**Prompts (100%)**
- Prompt registration and listing
- Prompt templates with arguments
- Embedded resources in prompts

**Security (100%)**
- Origin header validation
- DNS rebinding protection
- CORS handling
- Tool input validation
- Output sanitization

### What We Don't Cover (❌)

**By Design (Optional Features):**
- **LLM Sampling** - Optional feature, application-specific
- **Elicitation** - Optional user input feature, not in core protocol
- **OAuth/Authentication** - Left to application layer for flexibility

**Partial Coverage (⚠️):**
- **Progress Notifications** - Basic support, advanced scenarios not fully tested
- **Resource Subscriptions** - Basic support, live update scenarios not fully tested

## Expected Failures Baseline

Since some features are intentionally not implemented, create a baseline file:

```yaml
# conformance-baseline.yml
server:
  - tools-call-sampling          # Optional feature
  - tools-call-elicitation       # Optional feature
  - elicitation-sep1034-defaults # Optional feature
  - elicitation-sep1330-enums    # Optional feature
  - tools-call-with-progress     # Partial implementation
  - resources-subscribe          # Partial implementation
  - resources-unsubscribe        # Partial implementation
```

Run with baseline:

```bash
npx @modelcontextprotocol/conformance server \
  --url http://localhost:8080/mcp \
  --expected-failures ./conformance-baseline.yml
```

This will:
- Exit 0 for expected failures (listed in baseline)
- Exit 1 for unexpected failures (regressions)
- Exit 1 if baseline has stale entries (test now passes)

## Integration with CI/CD

### GitHub Actions Workflow

The repository includes a conformance testing workflow at `.github/workflows/conformance.yml` that automatically runs on every push and pull request:

**Key Features:**
- Builds the C++ MCP server
- Starts server on port 3001
- Runs official conformance tests against it
- Uses expected failures baseline (`conformance-baseline.yml`)
- Uploads test results as artifacts

**Workflow Configuration:**

```yaml
name: MCP Conformance Tests

on:
  pull_request:
    branches: [ main ]
  push:
    branches: [ main ]

jobs:
  conformance:
    runs-on: ubuntu-latest
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgJsonGlob: 'vcpkg.json'
      
      - name: Build C++ server
        run: |
          cmake --preset ci-linux
          cmake --build --preset ci-linux
      
      - name: Start MCP server
        run: |
          ./build/ci-linux/examples/server_example --port 3001 &
          # Wait for server to be ready
          timeout 30 bash -c 'until curl -s http://localhost:3001/mcp; do sleep 0.5; done'
      
      - name: Run conformance tests
        uses: modelcontextprotocol/conformance@v0.1.11
        with:
          mode: server
          url: http://localhost:3001/mcp
          suite: active
          expected-failures: ./conformance-baseline.yml
          verbose: true
      
      - name: Upload results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: conformance-results
          path: results/
          retention-days: 30
```

**To view conformance results:**
1. Go to the GitHub Actions tab
2. Click on a workflow run
3. Download the `conformance-results` artifact
4. Extract and view `checks.json` for detailed results

See `.github/workflows/conformance.yml` for the complete implementation.

## Test Results Interpretation

### Success Output Example

```
✓ server-initialize: ServerInitialize
  Server responds to initialize request with valid structure
  
✓ tools-list: ToolsList
  Server provides list of available tools
  
✓ resources-read-text: ResourcesReadText
  Server reads text resources successfully

All checks passed! (42/42)
```

### Failure Output Example

```
✗ tools-call-simple-text: ToolCallSimpleText
  Tool invocation failed
  Error: Connection timeout after 5000ms
  Spec: https://modelcontextprotocol.io/specification/2025-06-18/server/tools
  
✓ tools-list: ToolsList
  Server provides list of available tools

1 check failed (41/42 passed)
```

### Reviewing Results Files

```bash
# View latest results
cat results/server-<scenario>-<timestamp>/checks.json | jq

# Count passed/failed
cat results/server-tools-list-*/checks.json | jq '[.[] | select(.status=="SUCCESS")] | length'

# View only failures
cat results/server-*/checks.json | jq '.[] | select(.status=="FAILURE")'
```

## Debugging Failures

### Enable Verbose Logging

```bash
# C++ server with debug logging
./build/dev-release/examples/server_example --log-level debug

# Conformance framework with verbose output
npx @modelcontextprotocol/conformance server \
  --url http://localhost:8080/mcp \
  --scenario server-initialize \
  --verbose
```

### Check Server Logs

The C++ server logs all requests:

```
2026-02-16 20:40:40 [INFO] :0 - "POST /mcp HTTP/1.1"
2026-02-16 20:40:40 [INFO] Processing method call: initialize
2026-02-16 20:40:40 [INFO] Client requested protocol version: 2025-06-18
2026-02-16 20:40:40 [INFO] Client connected: TestClient 1.0.0
```

### Compare with Reference Implementation

```bash
# Run against Python SDK server for comparison
# (after installing Python SDK)
python -m mcp.server.examples.everything --port 3002 &

npx @modelcontextprotocol/conformance server \
  --url http://localhost:3002/mcp \
  --scenario server-initialize \
  --verbose
```

## Manual Testing with MCP Inspector

For interactive testing:

```bash
# Start your server
./build/dev-release/examples/server_example

# Start inspector
npx @modelcontextprotocol/inspector

# Open browser to displayed URL
# Connect to: http://localhost:8080/mcp
```

The inspector provides a visual UI to:
- Test tool invocations
- Browse resources
- Execute prompts
- View protocol messages
- Inspect server capabilities

## Updating Conformance Tests

The conformance framework is actively developed. Update regularly:

```bash
# Check current version
npx @modelcontextprotocol/conformance --version

# Update to latest
npm install -g @modelcontextprotocol/conformance@latest

# Or use latest via npx (no install needed)
npx @modelcontextprotocol/conformance@latest server --url http://localhost:8080/mcp
```

## Troubleshooting

### Server Won't Start

```bash
# Check if port is in use
lsof -i :8080

# Kill existing process
kill $(lsof -t -i :8080)

# Or use different port
./build/dev-release/examples/server_example --port 8081
```

### Conformance Tool Connection Fails

```bash
# Verify server is responding
curl -v http://localhost:8080/mcp

# Check server logs for errors
# Enable debug logging in server

# Test with simple HTTP client first
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"ping","params":{}}'
```

### Timeout Errors

```bash
# Increase timeout (default 30000ms)
npx @modelcontextprotocol/conformance server \
  --url http://localhost:8080/mcp \
  --timeout 60000
```

## Resources

- **Conformance Framework**: https://github.com/modelcontextprotocol/conformance
- **SDK Integration Guide**: https://github.com/modelcontextprotocol/conformance/blob/main/SDK_INTEGRATION.md
- **MCP Specification**: https://spec.modelcontextprotocol.io/specification/2025-06-18/
- **MCP Inspector**: https://github.com/modelcontextprotocol/inspector
- **Python SDK (Reference)**: https://github.com/modelcontextprotocol/python-sdk
- **TypeScript SDK (Reference)**: https://github.com/modelcontextprotocol/typescript-sdk

## See Also

- [CONFORMANCE.md](CONFORMANCE.md) - Internal test coverage documentation
- [README.md](README.md) - Project overview and building instructions
- [SECURITY.md](SECURITY.md) - Security considerations
- [AGENTS.md](AGENTS.md) - Development guidelines

---

**Last Updated:** 2026-02-16  
**Conformance Framework Version:** 0.1.11  
**MCP Protocol Version:** 2025-06-18
