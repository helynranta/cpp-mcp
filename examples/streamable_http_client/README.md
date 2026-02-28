# Streamable HTTP Client Example

Demonstrates the **Streamable HTTP** transport (MCP 2025-06-18+), the recommended modern transport.

## Features

- Unified `/mcp` endpoint
- Session management via `Mcp-Session-Id` header
- Tool and resource operations
- Explicit session termination via DELETE
- Side-by-side comparison with SSE client

## Build and Run

```bash
cmake --build build --target streamable_http_client_example
# Start the server first, then:
.\build\examples\streamable_http_client_example.exe
```
