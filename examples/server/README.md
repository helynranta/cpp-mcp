# Server Example

MCP server implementation with custom tools following the **MCP 2025-06-18** specification.

## Features

- **Time tool** – current time with structured timestamp data
- **Calculator** – math operations with structured results
- **Echo** – text transformations (uppercase, reverse) with metadata
- **Greeting** – personalized greeting with structured output
- Resources, prompts, and conformance endpoints for official MCP conformance testing

All tools include `outputSchema`, `structuredContent`, and human-readable `content`.

## Build and Run

```bash
cmake --build build --target server_example
.\build\examples\server_example.exe          # default: http://localhost:8888
.\build\examples\server_example.exe --port 3001
```
