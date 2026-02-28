# HTTP Example

Minimal low-level demonstration of the **Boost.Beast HTTP abstraction layer** (`mcp::http::create_server()` / `mcp::http::create_client()`).

## Features

- HTTP server with custom GET/POST route handlers
- HTTP client making requests and handling responses
- JSON request/response handling
- Separate from the MCP protocol layer

## Build and Run

```bash
cmake --build build --target http_example
.\build\examples\http_example.exe
```

Starts a server on `localhost:8890`, runs test requests, then keeps the server running.
