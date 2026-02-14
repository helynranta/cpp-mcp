# MCP Protocol Framework

[Model Context Protocol (MCP)](https://spec.modelcontextprotocol.io/specification/2025-03-26/architecture/) is an open protocol that provides a standardized way for AI models and agents to interact with various resources, tools, and services. This framework implements the core functionality of the MCP protocol, conforming to the 2025-03-26 basic protocol specification.

For the full specification and protocol details, see the [MCP GitHub repository](https://github.com/modelcontextprotocol/modelcontextprotocol).

## Core Features

- **JSON-RPC 2.0 Communication**: Request/response communication based on JSON-RPC 2.0 standard
- **Batch Request Support**: Process multiple JSON-RPC requests in a single HTTP call (MCP 2025-03-26 requirement)
- **Resource Abstraction**: Standard interfaces for resources such as files, APIs, etc.
- **Tool Registration**: Register and call tools with structured parameters
- **Extensible Architecture**: Easy to extend with new resource types and tools
- **Multi-Transport Support**: Supports HTTP and standard input/output (stdio) communication methods

## How to Build

Example of building with CMake:
```bash
cmake -B build
cmake --build build --config Release
```

Build with tests:
```
git submodule update --init --recursive # Get GoogleTest

cmake -B build -DMCP_BUILD_TESTS=ON
cmake --build build --config Release
```

Build with SSL support:
```
git submodule update --init --recursive # Get GoogleTest

cmake -B build -DMCP_SSL=ON
cmake --build build --config Release
```

## Adopters

Here are some open-source projects that are using this repository.  
If you're using it too, feel free to submit a PR to be featured here!

- [humanus.cpp](https://github.com/WHU-MYTH-Lab/humanus.cpp): Lightweight C++ LLM agent framework
- ...waiting for your contribution...



## Components

The MCP C++ library includes the following main components:

### Core Components

#### Client Interface (`mcp_client.h`)
Defines the abstract interface for MCP clients, which all concrete client implementations inherit from.

#### SSE Client (`mcp_sse_client.h`, `mcp_sse_client.cpp`)
Client implementation that communicates with MCP servers using HTTP and Server-Sent Events (SSE).

#### Stdio Client (`mcp_stdio_client.h`, `mcp_stdio_client.cpp`)
Client implementation that communicates with MCP servers using standard input/output, capable of launching subprocesses and communicating with them.

#### Message Processing (`mcp_message.h`, `mcp_message.cpp`)
Handles serialization and deserialization of JSON-RPC messages.

#### Tool Management (`mcp_tool.h`, `mcp_tool.cpp`)
Manages and invokes MCP tools.

#### Resource Management (`mcp_resource.h`, `mcp_resource.cpp`)
Manages MCP resources.

#### Server (`mcp_server.h`, `mcp_server.cpp`)
Implements MCP server functionality.

## Examples

### HTTP Server Example (`examples/server_example.cpp`)

Example MCP server implementation with custom tools:
- Time tool: Get the current time
- Calculator tool: Perform mathematical operations
- Echo tool: Echo input with optional transformations (to uppercase, reverse)
- Greeting tool: Returns `Hello, `+ input name + `!`, defaults to `Hello, World!`

### HTTP Client Example (`examples/client_example.cpp`)

Example MCP client connecting to a server:
- Get server information
- List available tools
- Call tools with parameters
- Access resources

### Stdio Client Example (`examples/stdio_client_example.cpp`)

Demonstrates how to use the stdio client to communicate with a local server:
- Launch a local server process
- Access filesystem resources
- Call server tools

### Agent Example (`examples/agent_example.cpp`)

| Option | Description |
| :- | :- |
| `--base-url` | LLM base URL (e.g. `https://openrouter.ai`) |
| `--endpoint` | LLM endpoint (default to `/v1/chat/completions/`) |
| `--api-key` | LLM API key |
| `--model` | Model name (e.g. `gpt-3.5-turbo`) |
| `--system-prompt` | System prompt |
| `--max-tokens` | Maximum number of tokens to generate (default to 2048) |
| `--temperature` | Temperature (default to 0.0) |
| `--max-steps` | Maximum steps calling tools without user input (default to 3) |

Example usage:
```
./build/examples/agent_example --base-url <base_url> --endpoint <endpoint> --api-key <api_key> --model <model_name>
```

**Note**: Remember to compile with `-DMCP_SSL=ON` when connecting to an https base URL.

### Progress Notification Example (`examples/progress_example.cpp`)

Demonstrates real-time progress notifications:
- Server sends progress updates during long-running operations
- Client receives and displays progress in real-time
- Shows both with and without progress tokens
- Example of proper progress token handling

### Batch Request Example (`examples/batch_example.cpp`)

Demonstrates JSON-RPC batch request support (MCP 2025-03-26):
- Multiple requests in a single batch
- Mixed batches (requests + notifications)
- Notification-only batches
- Empty batch validation
- Shows expected server behavior for each scenario

## How to Use

### Setting up an HTTP Server

```cpp
// Create and configure the server
mcp::server::configuration srv_conf;
srv_conf.host = "localhost";
srv_conf.port = 8888;

mcp::server server(srv_conf);
server.set_server_info("MCP Example Server", "0.1.0"); // Name and version

// Register tools
mcp::json hello_handler(const mcp::json& params, const std::string /* session_id */) {
    std::string name = params.contains("name") ? params["name"].get<std::string>() : "World";

    // Server will catch exceptions and return error contents
    // For example, you can use `throw mcp::mcp_exception(mcp::error_code::invalid_params, "Invalid name");` to report an error

    // Content should be a JSON array, see: https://modelcontextprotocol.io/specification/2025-03-26/server/tools#tool-result
    return {
        {
            {"type", "text"},
            {"text", "Hello, " + name + "!"}
        }
    };
}

mcp::tool hello_tool = mcp::tool_builder("hello")
        .with_description("Say hello")
        .with_string_param("name", "Name to say hello to", "World")
        .build();

server.register_tool(hello_tool, hello_handler);

// Register resources
auto file_resource = std::make_shared<mcp::file_resource>("<file_path>");
server.register_resource("file://<file_path>", file_resource);

// Start the server
server.start(true);  // Blocking mode
```

### Creating an HTTP Client

```cpp
// Connect to the server
mcp::sse_client client("http://localhost:8080");

// Initialize the connection
client.initialize("My Client", "1.0.0");

// Call a tool
mcp::json params = {
    {"name", "Client"}
};

mcp::json result = client.call_tool("hello", params);
```

### Using the SSE Client

The SSE client uses HTTP and Server-Sent Events (SSE) to communicate with MCP servers. This is a communication method based on Web standards, suitable for communicating with servers that support HTTP/SSE.

```cpp
#include "mcp_sse_client.h"

// Create a client, specifying the server address and port
mcp::sse_client client("http://localhost:8080");

// Set an authentication token (if needed)
client.set_auth_token("your_auth_token");

// Set custom request headers (if needed)
client.set_header("X-Custom-Header", "value");

// Initialize the client
if (!client.initialize("My Client", "1.0.0")) {
    // Handle initialization failure
}

// Call a tool
json result = client.call_tool("tool_name", {
    {"param1", "value1"},
    {"param2", 42}
});
```

### Using the Stdio Client

The Stdio client can communicate with any MCP server that supports stdio transport, such as:

- @modelcontextprotocol/server-everything - Example server
- @modelcontextprotocol/server-filesystem - Filesystem server
- Other [MCP servers](https://www.pulsemcp.com/servers) that support stdio transport

```cpp
#include "mcp_stdio_client.h"

// Create a client, specifying the server command
mcp::stdio_client client("npx -y @modelcontextprotocol/server-everything");
// mcp::stdio_client client("npx -y @modelcontextprotocol/server-filesystem /path/to/directory");

// Initialize the client
if (!client.initialize("My Client", "1.0.0")) {
    // Handle initialization failure
}

// Access resources
json resources = client.list_resources();
json content = client.read_resource("resource://uri");

// Call a tool
json result = client.call_tool("tool_name", {
    {"param1", "value1"},
    {"param2", "value2"}
});
```

## Batch Request Support

MCP 2025-03-26 requires implementations to support receiving JSON-RPC batches. This framework fully implements batch request processing according to the JSON-RPC 2.0 specification.

### What is a Batch Request?

A batch request allows clients to send multiple JSON-RPC requests in a single HTTP call by wrapping them in a JSON array. This can improve performance by reducing network round-trips.

### Server Behavior

The server handles batch requests as follows:

1. **Multiple Requests**: Processes all requests and returns an array of responses in the same order
2. **Mixed Batches**: Supports requests and notifications together; only requests get responses
3. **Notification-Only Batches**: Returns HTTP 202 Accepted with no response body
4. **Empty Batches**: Returns HTTP 400 error (invalid per JSON-RPC 2.0)
5. **Single Item Batches**: Valid and processed normally

### Example Batch Request

```json
[
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/list"
  },
  {
    "jsonrpc": "2.0",
    "id": 2,
    "method": "resources/list"
  },
  {
    "jsonrpc": "2.0",
    "method": "notifications/log",
    "params": {"message": "Processing batch"}
  }
]
```

This batch contains:
- Two requests (with IDs) that will receive responses
- One notification (no ID) that will be processed without a response

### Expected Response

```json
[
  {
    "jsonrpc": "2.0",
    "id": 1,
    "result": {"tools": [...]}
  },
  {
    "jsonrpc": "2.0",
    "id": 2,
    "result": {"resources": [...]}
  }
]
```

The response array contains only the results for the two requests, not for the notification.

### Testing Batch Support

Run the batch example to see all scenarios:

```bash
./build/examples/batch_example
```

The test suite includes comprehensive batch tests:

```bash
cd build && ctest -R Batch
```

## Progress Notifications

MCP supports progress notifications for long-running operations. This allows servers to send real-time progress updates to clients during tool execution.

### Server-Side Progress Notifications

```cpp
#include "mcp_server.h"
#include "mcp_progress.h"

// Register a long-running tool
mcp::tool long_tool = mcp::tool_builder("process_data")
    .with_description("Process large dataset with progress updates")
    .with_number_param("count", "Number of items to process", 100.0)
    .build();

server.register_tool(long_tool, [&server](const mcp::json& params, const std::string& session_id) -> mcp::json {
    int count = params.value("count", 100);
    
    // Check if client requested progress updates
    auto progress_token = mcp::progress_tracker::extract_progress_token(params);
    
    for (int i = 1; i <= count; ++i) {
        // Do work...
        process_item(i);
        
        // Send progress notification if token is available
        if (progress_token.has_value()) {
            mcp::progress_notification notif = mcp::progress_notification::create(
                progress_token.value(),
                static_cast<double>(i),        // current progress
                static_cast<double>(count),    // total (optional)
                "Processing item " + std::to_string(i)  // message (optional)
            );
            
            server.send_progress(session_id, notif);
        }
    }
    
    return {{"result", "Completed"}};
});
```

### Client-Side Progress Handling

```cpp
#include "mcp_sse_client.h"
#include "mcp_progress.h"

mcp::sse_client client("http://localhost:8080");

// Set up progress handler to receive notifications
client.set_progress_handler([](const mcp::progress_notification& notif) {
    std::cout << "Progress: " << notif.progress;
    
    if (notif.total.has_value()) {
        double percent = (notif.progress / notif.total.value()) * 100.0;
        std::cout << "/" << notif.total.value() << " (" << percent << "%)";
    }
    
    if (notif.message.has_value()) {
        std::cout << " - " << notif.message.value();
    }
    
    std::cout << std::endl;
});

client.initialize("My Client", "1.0.0");

// Call tool with progress token in metadata
mcp::json params = {
    {"count", 50},
    {"_meta", {
        {"progressToken", "operation-123"}  // Include progress token
    }}
};

mcp::json result = client.call_tool("process_data", params);
```

### Progress Notification Format

Progress notifications follow the MCP specification (2025-03-26):

- **progressToken**: Token from the original request (required)
- **progress**: Current progress value, must always increase (required)
- **total**: Total value if known (optional)
- **message**: Human-readable status message (optional)

Example notification:
```json
{
    "jsonrpc": "2.0",
    "method": "notifications/progress",
    "params": {
        "progressToken": "operation-123",
        "progress": 50,
        "total": 100,
        "message": "Processing item 50"
    }
}
```


## Using TLS clients and servers

### Creating test certificates on Linux
1. Generate Certificate Authority (CA) private key
    ```bash
    openssl genrsa -out ca.key.pem 2048
    ```
1. Generate CA certificate
    ```bash
    openssl req -x509 -new -nodes -key ca.key.pem -sha256 -days 1 -out ca.cert.pem -subj "/CN=Test CA"
    ```
1. Generate server private key
    ```bash
    openssl genrsa -out server.key.pem 2048
    ```
1. Generate Certificate Signing Request (CSR)
    ```
    openssl req -new -key server.key.pem -out server.csr.pem -subj "/O=TestServer/OU=Dev/CN=localhost"
    ```
1. Generate server certificate signed by CA
    ```
    openssl x509 -req -in server.csr.pem -CA ca.cert.pem -CAkey ca.key.pem -CAcreateserial -out server.cert.pem -days 1 -sha256
    ```
### Setting up an HTTPs server

```cpp
mcp::server::configuration srv_conf;
srv_conf.host = "localhost";
srv_conf.port = 8888;
srv_conf.ssl.server_cert_path = "./server.cert.pem";
srv_conf.ssl.server_private_key_path = "./server.key.pem";
```

### Setting up an SSE client with TLS

```cpp
 mcp::sse_client client("https://localhost:8888");
 ```

## License

This framework is provided under the MIT license. For details, please see the LICENSE file.

