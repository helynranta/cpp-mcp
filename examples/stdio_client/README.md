# Stdio Client Example

Communicates with an MCP server over **stdin/stdout**, launching the server as a child process.

## Features

- Subprocess server launching
- Environment variable passing
- Tool and resource listing and reading
- Windows process management

## Build and Run

```bash
cmake --build build --target stdio_client_example
.\build\examples\stdio_client_example.exe "npx -y @modelcontextprotocol/server-everything"
```
