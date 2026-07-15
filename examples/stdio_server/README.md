# stdio server example

`stdio_server_example` exposes an `echo` tool over newline-delimited JSON-RPC on standard input/output. MCP clients
spawn the process, complete the normal initialization lifecycle, and close stdin to stop it cleanly.

Only protocol messages are written to stdout. Transport and protocol diagnostics use stderr.
