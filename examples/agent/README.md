# Agent Example

AI agent that integrates an MCP server with an **external LLM API** (OpenAI, OpenRouter, etc.).

## Features

- Runs a local MCP server with tools (e.g., calculator)
- Connects to an LLM API using Boost.Beast HTTP client
- Allows the LLM to call MCP tools to answer user queries
- Interactive chat loop with tool execution

## Build and Run

```bash
# Build with SSL for HTTPS connections
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DMCP_SSL=ON
cmake --build build --target agent_example

.\build\examples\agent_example.exe --base-url <url> --api-key <key> --model <model>
```

| Option | Description |
| :-- | :-- |
| `--base-url` | LLM base URL |
| `--endpoint` | LLM endpoint (default `/v1/chat/completions/`) |
| `--api-key` | API key |
| `--model` | Model name |
| `--system-prompt` | System prompt |
| `--max-tokens` | Max tokens (default 2048) |
| `--temperature` | Temperature (default 0.0) |
| `--max-steps` | Max tool-call steps (default 3) |

> **Note**: Compile with `-DMCP_SSL=ON` when connecting to HTTPS endpoints.
