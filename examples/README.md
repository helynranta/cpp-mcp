# Examples

Examples cover HTTP and stdio transports. Build with CMake presets (e.g., `cmake --preset dev-release && cmake --build --preset dev-release`).

| Example | Description |
| :------ | :---------- |
| [server](server/) | MCP server with tools, resources, prompts, and conformance endpoints |
| [sse_client](sse_client/) | SSE (legacy) client connecting to an MCP server |
| [streamable_http_client](streamable_http_client/) | Streamable HTTP (modern) client for MCP 2025-06-18+ |
| [stdio_client](stdio_client/) | Stdio transport client launching a local server process |
| [stdio_server](stdio_server/) | Client-spawned stdio server with clean EOF shutdown |
| [http](http/) | Low-level Boost.Beast HTTP client/server demo |
| [agent](agent/) | AI agent integrating MCP tools with an external LLM API |
| [structured_tool](structured_tool/) | Structured tool output with JSON Schema (MCP 2025-06-18) |
| [progress](progress/) | Real-time progress notifications |
| [session_state](session_state/) | Per-session state management across tool calls |
| [elicitation](elicitation/) | Human-in-the-loop elicitation (MCP 2025-06-18) |
| [completion](completion/) | Argument autocompletion for prompts and templates |
