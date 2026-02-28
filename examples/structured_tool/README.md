# Structured Tool Output Example

Demonstrates **MCP 2025-06-18 structured tool output** with JSON Schema.

## Features

- Weather, API query, and calculator tools with `outputSchema`
- `structuredContent` responses matching declared schemas
- Human-readable `content` for backward compatibility
- Legacy echo tool without schema (backward compat demo)

## Build and Run

```bash
cmake --build build --target structured_tool_example
.\build\examples\structured_tool_example.exe
```

Server listens on `http://localhost:8080/mcp`.
