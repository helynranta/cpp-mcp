# SSE Client Example

Connects to an MCP server using the legacy **Server-Sent Events** transport.

## Features

- SSE real-time communication via Boost.Beast
- Server capability negotiation
- Tool listing and execution
- MCP exception handling

## Build and Run

```bash
cmake --build build --target sse_client_example
# Start the server first, then:
.\build\examples\sse_client_example.exe
```
