# MCP Protocol Framework

> ⚠️ **WARNING**: This is a purely vibe-coded experimental fork. Use at your own risk!

[Model Context Protocol (MCP)](https://spec.modelcontextprotocol.io/specification/2025-06-18/architecture/) is an open protocol that provides a standardized way for AI models and agents to interact with various resources, tools, and services. This framework implements the core functionality of the MCP protocol, conforming to the 2025-06-18 protocol specification with backward compatibility for 2025-03-26.

For the full specification and protocol details, see the [MCP GitHub repository](https://github.com/modelcontextprotocol/modelcontextprotocol).

## Core Features

- **JSON-RPC 2.0 Communication**: Request/response communication based on JSON-RPC 2.0 standard
- ~~**Batch Request Support**~~: **DEPRECATED** - Batch requests were removed in MCP 2025-06-18 (previously supported in 2025-03-26)
- **Streamable HTTP Transport**: Full support for MCP 2025-06-18 Streamable HTTP transport with unified `/mcp` endpoint
  - Single endpoint for GET, POST, and DELETE methods
  - `Mcp-Session-Id` header-based session management
  - SSE (Server-Sent Events) streaming for real-time responses
  - Backward compatible with legacy `/sse` and `/message` endpoints
- **HTTP Transport Security (MCP 2025-06-18)**:
  - Origin header validation for DNS rebinding mitigation
  - Configurable allowed origins with localhost defaults
  - Secure CORS handling with origin reflection
  - See [SECURITY.md](SECURITY.md) for details
- **Tool Execution Safety (MCP 2025-06-18)**:
  - Optional user confirmation hooks for sensitive tools
  - Configurable tool execution policies
  - Trust model for tool annotations as untrusted metadata
  - See [SECURITY.md](SECURITY.md) for details
- **Lifecycle Management**: Strict initialization lifecycle with state transitions (uninitialized → initializing → ready)
- ~~**Batch Initialization Protection**~~: **DEPRECATED** - No longer applicable as batch support was removed in MCP 2025-06-18
- **Capability Negotiation**: Store and respect client capabilities negotiated during initialization
- **Cancellation Support**: Handle cancellation notifications (notifications/cancelled) with configurable timeout
- **Resource Abstraction**: Standard interfaces for resources such as files, APIs, etc.
- **Tool Registration**: Register and call tools with structured parameters
- **Elicitation (Human-in-the-Loop) (MCP 2025-06-18)**: Request user input during tool execution with structured forms
- **Completion Support (MCP 2025-06-18)**: Argument autocompletion for prompts and resource templates
  - Context-aware completions with previously-resolved variables
  - Extensible metadata via `_meta` field
  - Support for both prompt arguments and resource template variables
- **Extensible Architecture**: Easy to extend with new resource types and tools
- **Multi-Transport Support**: Supports HTTP and standard input/output (stdio) communication methods

## How to Build

### Requirements

**C++23 Compiler Required**: This project requires a C++23-compliant compiler. No backwards compatibility with older C++ standards is provided.

Minimum compiler versions (with experimental C++23 support):
- **GCC** 11 or later (GCC 13+ recommended for production)
- **Clang** 12 or later (Clang 15+ recommended for production)
- **MSVC** 2019 (v142) or later (MSVC 2022+ recommended for production)

### Dependencies

This project uses vcpkg for dependency management. The following dependencies are automatically fetched via vcpkg:

- **Boost.Beast** (`boost-beast`) - HTTP and WebSocket networking library
  - Automatically includes Boost.Asio (asynchronous I/O) and Boost.System (error handling) as transitive dependencies

All Boost components are version 1.90.0 and managed through vcpkg manifest mode (`vcpkg.json`).

### Build Instructions

#### Using CMake Presets (Recommended)

This project provides CMake presets for standardized builds. Presets simplify configuration and ensure consistency across development and CI environments.

**Prerequisites:** Ensure `VCPKG_ROOT` environment variable points to your vcpkg installation.

**Available Presets:**

Development presets:
- `dev-debug` - Debug build with tests enabled
- `dev-release` - Release build with tests enabled
- `sanitizer-address` - Debug with AddressSanitizer (Linux/macOS only)
- `sanitizer-undefined` - Debug with UndefinedBehaviorSanitizer (Linux/macOS only)
- `coverage` - Debug with code coverage instrumentation (Linux/macOS only)

Production presets:
- `release` - Optimized release build without tests
- `ssl` - Release build with SSL support

CI presets:
- `ci-linux` - CI build for Linux
- `ci-windows` - CI build for Windows

**Quick Start:**

```bash
# Configure using a preset
cmake --preset dev-release

# Build
cmake --build --preset dev-release

# Run tests
ctest --preset dev-release
```

**Common Usage Examples:**

```bash
# Development with tests (Debug)
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug

# Development with tests (Release)
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release

# Production release build
cmake --preset release
cmake --build --preset release

# Build with SSL support
cmake --preset ssl
cmake --build --preset ssl

# Run sanitizers (Linux/macOS)
cmake --preset sanitizer-address
cmake --build --preset sanitizer-address
ctest --preset sanitizer-address

# Code coverage (Linux/macOS)
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
# Then generate coverage report with lcov/gcov
```

**List all available presets:**
```bash
cmake --list-presets
```

#### Manual CMake Configuration (Alternative)

If you prefer not to use presets or need custom configuration:

```bash
# Configure with vcpkg toolchain
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release
```

**Build with tests:**
```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DMCP_BUILD_TESTS=ON \
  -DVCPKG_MANIFEST_FEATURES="tests"

cmake --build build --config Release
cd build && ctest -V
```

**Build with SSL support:**
```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DMCP_SSL=ON

cmake --build build --config Release
```

#### Without vcpkg (Manual dependency installation)

If you prefer to install Boost manually, ensure:
- Boost (version 1.70 or later, tested with 1.90.0) is installed on your system
- A C++23-compliant compiler is available (GCC 11+, Clang 12+, MSVC 2019+)

Then build without the vcpkg toolchain file:

```bash
cmake -B build
cmake --build build --config Release
```

### Boost Integration Details

#### vcpkg Manifest Mode

This project uses vcpkg's manifest mode for dependency management. Dependencies are declared in `vcpkg.json`:

```json
{
  "dependencies": [
    "boost-beast"
  ],
  "features": {
    "tests": {
      "dependencies": ["boost-test"]
    }
  }
}
```

When you configure CMake with the vcpkg toolchain file, vcpkg automatically:
- Downloads and builds the specified Boost components
- Resolves and installs all transitive dependencies (Boost.Asio, Boost.System, etc.)
- Makes them available to CMake via `find_package()`

#### CMake Integration

The project uses standard CMake `find_package()` to locate Boost:

```cmake
find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(mcp PUBLIC Boost::system)
```

- **Boost.Beast** is header-only and included through Boost headers
- **Boost.System** provides compiled components for error handling
- **Boost.Asio** is header-only but depends on Boost.System

#### Verifying Boost Integration

Run the Boost integration tests to verify the installation:

```bash
cd build
./test/mcp_tests --run_test=BoostIntegrationTest
```

These tests verify:
- Boost.Beast headers are accessible
- HTTP request/response types work correctly
- Boost.Asio I/O context can be created

#### Platform-Specific Notes

**Linux:**
- vcpkg typically installs to `/usr/local/share/vcpkg`
- Use `VCPKG_ROOT` environment variable or specify the full path

**Windows:**
- vcpkg installs to `C:\vcpkg` by default
- Use PowerShell syntax: `$env:VCPKG_ROOT`

For more information, see:
- [Boost.Beast Documentation](https://www.boost.org/doc/libs/release/libs/beast/doc/html/index.html)
- [vcpkg Documentation](https://vcpkg.io/en/docs/)

## HTTP Transport

This framework implements the MCP 2025-06-18 Streamable HTTP transport specification with a unified `/mcp` endpoint.

### Streamable HTTP Endpoint

The server exposes a single `/mcp` endpoint that supports:

- **GET** - Establishes an SSE (Server-Sent Events) connection for receiving responses
  - Returns session endpoint information
  - Sets `Mcp-Session-Id` header in response
  - Streams responses in real-time via SSE

- **POST** - Sends JSON-RPC requests or notifications
  - Requires `Mcp-Session-Id` header or `session_id` query parameter
  - Requires `Accept` header with `application/json` and/or `text/event-stream`
  - Returns HTTP 202 Accepted for async processing
  - Returns HTTP 406 Not Acceptable if Accept header missing or invalid
  - Responses delivered via SSE connection
  - Notifications (no ID) return 202 immediately
  - Requests (with ID) process asynchronously with SSE response

- **DELETE** - Terminates a session
  - Requires `Mcp-Session-Id` header or `session_id` query parameter
  - Returns HTTP 204 No Content on success
  - Returns HTTP 404 if session not found

### Session Management

Sessions are managed using the `Mcp-Session-Id` header (recommended) or `session_id` query parameter (legacy):

```cpp
// Example: Establish session via GET
GET /mcp HTTP/1.1
Host: localhost:8080

// Response includes session ID
HTTP/1.1 200 OK
Mcp-Session-Id: abc123-session-id
Content-Type: text/event-stream

event: endpoint
data: /mcp?session_id=abc123-session-id

// Subsequent requests use the session ID
POST /mcp HTTP/1.1
Host: localhost:8080
Mcp-Session-Id: abc123-session-id
Content-Type: application/json
Accept: application/json, text/event-stream

{"jsonrpc":"2.0","id":1,"method":"tools/list"}
```

### Protocol Version Header (MCP 2025-06-18+)

Starting with MCP 2025-06-18, the `MCP-Protocol-Version` header is **required** in all HTTP requests after initialization. This header enables version negotiation and ensures protocol compatibility between clients and servers.

#### Server Behavior

The server validates the `MCP-Protocol-Version` header for all requests:

- **Supported Versions**: `2025-03-26`, `2025-06-18`, `2025-11-25`
- **Missing Header**: Accepts request with warning (assumes `2025-03-26` for backward compatibility)
- **Invalid Version**: Returns HTTP 400 Bad Request with error details
- **Version Mismatch**: Returns HTTP 400 if header doesn't match negotiated version

```cpp
// Example: Request with protocol version header
POST /mcp HTTP/1.1
Host: localhost:8080
Mcp-Session-Id: abc123-session-id
MCP-Protocol-Version: 2025-06-18
Content-Type: application/json
Accept: application/json, text/event-stream

{"jsonrpc":"2.0","id":1,"method":"ping"}
```

#### Client Implementation

Both `streamable_http_client` and `sse_client` automatically:

1. **Negotiate version** during initialization (via `protocolVersion` parameter)
2. **Store negotiated version** from server's `InitializeResult`
3. **Include header** in all subsequent HTTP requests

```cpp
// Client automatically handles version negotiation
streamable_http_client client("http://localhost:8080");
client.initialize("MyClient", "1.0.0");  // Negotiates version

// All subsequent requests automatically include MCP-Protocol-Version header
client.ping();  // Header added automatically
auto result = client.call_tool("my_tool", params);  // Header added automatically
```

#### Error Responses

If the protocol version is invalid or mismatched:

```json
// HTTP 400 Bad Request
{
  "error": "Unsupported protocol version",
  "version_received": "invalid-version",
  "supported_versions": ["2025-03-26", "2025-06-18", "2025-11-25"]
}

// Or for version mismatch:
{
  "error": "Protocol version mismatch",
  "version_in_header": "2025-03-26",
  "negotiated_version": "2025-06-18"
}
```

#### Backward Compatibility

For clients that don't send the header:
- Server logs a warning but accepts the request
- Assumes protocol version `2025-03-26`
- This allows legacy clients to continue working

### Backward Compatibility

The framework maintains backward compatibility with legacy endpoints:

- **`/sse`** (deprecated) - Legacy SSE connection endpoint
- **`/message`** (deprecated) - Legacy JSON-RPC POST endpoint

These endpoints use query parameter-based session management (`?session_id=...`) and will continue to work for existing clients.

### Migration Guide

To migrate from legacy endpoints to Streamable HTTP transport:

1. **Update SSE connection**: Change `GET /sse` to `GET /mcp`
2. **Update POST endpoint**: Change `POST /message` to `POST /mcp`
3. **Use header-based sessions**: Add `Mcp-Session-Id` header instead of query parameter
4. **Add Accept header**: Include `Accept: application/json, text/event-stream` in POST requests
5. **Handle DELETE**: Implement session cleanup using `DELETE /mcp` with `Mcp-Session-Id` header

Example migration:

```cpp
// Old (legacy)
GET /sse                                    // Establish SSE
POST /message?session_id=abc123 HTTP/1.1   // Send request
Content-Type: application/json

// New (Streamable HTTP)
GET /mcp HTTP/1.1                           // Establish SSE
  -> Response includes: Mcp-Session-Id: abc123
  
POST /mcp HTTP/1.1                          // Send request
Mcp-Session-Id: abc123
Accept: application/json, text/event-stream

DELETE /mcp HTTP/1.1                        // Terminate session
Mcp-Session-Id: abc123
```

## Components

The MCP C++ library includes the following main components:

### Core Components

#### Client Interface (`mcp_client.h`)
Defines the abstract interface for MCP clients, which all concrete client implementations inherit from.

#### SSE Client (`mcp_sse_client.h`, `mcp_sse_client.cpp`)
**Legacy transport** - Client implementation that communicates with MCP servers using HTTP and Server-Sent Events (SSE).

- Uses `/sse` endpoint for SSE connection
- Uses `/message` endpoint for JSON-RPC requests
- Session ID passed as query parameter
- Backward compatible with older MCP implementations

#### Streamable HTTP Client (`mcp_streamable_http_client.h`, `mcp_streamable_http_client.cpp`)
**Modern transport (MCP 2025-06-18)** - Client implementation using the Streamable HTTP transport specification.

- Uses unified `/mcp` endpoint for all operations
- Session ID in `Mcp-Session-Id` header
- Includes `Accept: application/json, text/event-stream` header
- Supports explicit session termination via DELETE method
- Server returns HTTP 202 Accepted for async processing
- Fully compatible with MCP 2025-06-18 specification

**Recommended for new applications** - Provides better reliability, session management, and aligns with the latest MCP specification.

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

### HTTP Transport Layer

The MCP framework uses **Boost.Beast** for its HTTP transport layer, providing modern, efficient async I/O.

#### HTTP Factory ([`mcp_http_factory.h`](https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_http_factory.h))

Factory functions for creating HTTP clients and servers:

```cpp
#include "mcp_http_factory.h"

// Create HTTP server (uses Boost.Beast by default)
auto server = mcp::http::create_server(
    use_ssl,        // Whether to use SSL/TLS
    cert_path,      // Path to SSL certificate (if SSL enabled)
    key_path        // Path to SSL private key (if SSL enabled)
);

// Create HTTP client (uses Boost.Beast by default)
auto client = mcp::http::create_client("http://localhost:8080");
```

#### HTTP Abstractions ([`mcp_http_abstraction.h`](https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_http_abstraction.h))

Transport-agnostic HTTP interfaces:
- `server_interface` - Abstract HTTP server interface
- `client_interface` - Abstract HTTP client interface
- `response_builder` - For constructing HTTP responses
- `streaming_data_sink` - For SSE streaming

#### Beast Adapter ([`mcp_http_beast_adapter.h`](https://github.com/helynranta/cpp-mcp/blob/main/include/mcp_http_beast_adapter.h))

Boost.Beast implementation of HTTP abstractions:
- `beast_server` - Async HTTP server with SSE support
- `beast_client` - HTTP client with streaming support

**Key features:**
- Built on Boost.Beast 1.90.0
- Asynchronous I/O with Boost.Asio
- Support for SSE (Server-Sent Events) streaming
- Thread-safe operation using `std::jthread` (C++20)
- Automatic session management

**Example client usage:**
```cpp
#include "mcp_http_factory.h"

// Create client
auto client = mcp::http::create_client("http://localhost:8080");

// GET request
auto result = client->get("/api/endpoint");
if (result.success) {
    std::cout << "Status: " << result.status_code << std::endl;
    std::cout << "Body: " << result.body << std::endl;
}

// POST request
mcp::http::headers_map headers;
auto post_result = client->post("/api/data", headers, 
    "{\"key\":\"value\"}", "application/json");

// Streaming GET (for SSE)
client->get_stream("/events", [](const char* data, size_t size) {
    std::cout << "Event: " << std::string(data, size) << std::endl;
    return true; // Continue streaming
});
```

**Example server usage:**
```cpp
#include "mcp_http_factory.h"

// Create server
auto server = mcp::http::create_server(false, "", ""); // No SSL

// Register route handler
server->on("/test", [](const mcp::http::request_data& req, 
                       mcp::http::response_builder& res) {
    res.set_status(200);
    res.set_content("{\"status\":\"ok\"}", "application/json");
});

// Listen and run
server->listen("localhost", 8080);
server->run(); // Blocking call
```

For more details, see:
- [Boost.Beast Documentation](https://www.boost.org/doc/libs/release/libs/beast/doc/html/index.html)
- [Boost.Beast HTTP Examples](https://www.boost.org/doc/libs/release/libs/beast/example/http/)
- [HTTP Client/Server Example](https://github.com/helynranta/cpp-mcp/blob/main/examples/http_example.cpp) - Complete working example in this repository

## Examples

All examples use the Boost.Beast-based HTTP transport for communication. Source code is available in the [`examples/`](https://github.com/helynranta/cpp-mcp/tree/main/examples) directory.

### HTTP Server Example ([`examples/server_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/server_example.cpp))

Example MCP server implementation with custom tools:
- Time tool: Get the current time
- Calculator tool: Perform mathematical operations
- Echo tool: Echo input with optional transformations (to uppercase, reverse)
- Greeting tool: Returns `Hello, `+ input name + `!`, defaults to `Hello, World!`

**Build and run:**
```bash
cmake --build build --target server_example
./build/examples/server_example
```

The server listens on `http://localhost:8888` by default. You can verify it's working by connecting with the SSE client example in another terminal.

**Testing:**
```bash
# Terminal 1: Start the server
./build/examples/server_example

# Terminal 2: Connect with the client
./build/examples/sse_client_example
```

### SSE Client Example ([`examples/sse_client_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/sse_client_example.cpp))

Example MCP client connecting to a server via Server-Sent Events (legacy transport):
- Connect to an MCP server using HTTP/SSE transport
- Get server information and capabilities
- List available tools
- Call tools with parameters
- Handle errors and exceptions

**Build and run:**
```bash
cmake --build build --target sse_client_example
# First start the server in another terminal
./build/examples/sse_client_example
```

### Streamable HTTP Client Example ([`examples/streamable_http_client_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/streamable_http_client_example.cpp))

**New in MCP 2025-06-18** - Demonstrates both SSE (legacy) and Streamable HTTP (modern) transports side-by-side:
- Compare SSE client vs. Streamable HTTP client
- See differences in endpoint usage (`/sse` + `/message` vs. unified `/mcp`)
- Session management (query parameters vs. `Mcp-Session-Id` header)
- Explicit session termination with Streamable HTTP
- Recommended transport for new applications

**Build and run:**
```bash
cmake --build build --target streamable_http_client_example
./build/examples/streamable_http_client_example
```

**Key differences highlighted:**
- SSE Client uses `/sse` and `/message` endpoints with session ID in query parameters
- Streamable HTTP Client uses unified `/mcp` endpoint with `Mcp-Session-Id` header
- Streamable HTTP returns HTTP 202 Accepted for async processing
- Streamable HTTP supports explicit session termination via DELETE method

### Stdio Client Example ([`examples/stdio_client_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/stdio_client_example.cpp))

Demonstrates how to use the stdio client to communicate with a local server:
- Launch a local server process
- Access filesystem resources
- Call server tools
- Communicate via stdin/stdout

**Build and run:**
```bash
cmake --build build --target stdio_client_example
./build/examples/stdio_client_example "npx -y @modelcontextprotocol/server-everything"
```

### HTTP Client/Server Example ([`examples/http_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/http_example.cpp))

Minimal example demonstrating the low-level Boost.Beast HTTP APIs:
- Creating an HTTP server with custom route handlers (GET, POST)
- Creating an HTTP client
- Making HTTP requests and handling responses
- JSON request/response handling
- Error handling and validation

This example showcases the HTTP abstraction layer (`mcp::http::create_server()` and `mcp::http::create_client()`) that powers the higher-level MCP protocol implementations.

**Build and run:**
```bash
cmake --build build --target http_example
./build/examples/http_example
```

The example will:
1. Start an HTTP server on `localhost:8890`
2. Create a client and make several test requests
3. Demonstrate GET with query parameters, POST with JSON, calculator endpoint, and error handling
4. Keep the server running (press Ctrl+C to stop)

**Sample output:**
```
Creating HTTP server using Boost.Beast...
Starting HTTP server on localhost:8890...

Creating HTTP client using Boost.Beast...

--- Test 1: GET /hello ---
Status: 200
Body: {"message":"Hello, World!","timestamp":1771173000}

--- Test 4: POST /calculate (10 + 5) ---
Status: 200
Body: {"operands":[10.0,5.0],"operation":"add","result":15.0}
```

### Agent Example ([`examples/agent_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/agent_example.cpp))

AI agent that integrates an MCP server with an external LLM API. The agent:
- Runs a local MCP server with tools (e.g., calculator)
- Connects to an LLM API (OpenAI, OpenRouter, etc.) using Boost.Beast HTTP client
- Allows the LLM to call MCP tools to answer user queries
- Implements a chat loop with tool execution

**Command-line options:**

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

**Build and run:**
```bash
# Build with SSL support for HTTPS connections
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DMCP_SSL=ON
cmake --build build --target agent_example

# Run with your LLM API
./build/examples/agent_example --base-url <base_url> --endpoint <endpoint> --api-key <api_key> --model <model_name>
```

**Note**: Remember to compile with `-DMCP_SSL=ON` when connecting to an https base URL.

### Progress Notification Example ([`examples/progress_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/progress_example.cpp))

Demonstrates real-time progress notifications:
- Server sends progress updates during long-running operations
- Client receives and displays progress in real-time
- Shows both with and without progress tokens
- Example of proper progress token handling

**Build and run:**
```bash
cmake --build build --target progress_example
./build/examples/progress_example
```

### ~~Batch Request Example~~ **DEPRECATED** ([`examples/batch_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/batch_example.cpp))

**⚠️ This example is deprecated as of MCP 2025-06-18.**

JSON-RPC batching was removed from the MCP specification in version 2025-06-18. The server now rejects batch requests (arrays) with HTTP 400 and an error message.

This example is kept for historical reference only. It previously demonstrated:
- Multiple requests in a single batch
- Mixed batches (requests + notifications)
- Notification-only batches
- Empty batch validation
- Shows expected server behavior for each scenario

**Build and run (will show deprecation warning):**
```bash
cmake --build build --target batch_example
./build/examples/batch_example
```

### Session State Management Example ([`examples/session_state_example.cpp`](https://github.com/helynranta/cpp-mcp/blob/main/examples/session_state_example.cpp))

Demonstrates how to use the session state storage API to maintain state across tool calls within a session:

```bash
cmake --build build --target session_state_example
./build/examples/session_state_example
```

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

    // Content should be a JSON array, see: https://modelcontextprotocol.io/specification/2025-06-18/server/tools#tool-result
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

**Note**: This is the legacy transport. For new applications, consider using the Streamable HTTP client (see below).

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

### Using the Streamable HTTP Client (MCP 2025-06-18)

The Streamable HTTP client implements the modern MCP 2025-06-18 transport specification. **Recommended for new applications**.

```cpp
#include "mcp_streamable_http_client.h"

// Create a client, specifying the server address and port
// Uses unified /mcp endpoint by default
mcp::streamable_http_client client("http://localhost:8080");

// Optional: Customize the endpoint (defaults to "/mcp")
// mcp::streamable_http_client client("http://localhost:8080", "/custom-mcp-endpoint");

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

// Explicitly close the session when done (optional but recommended)
// This is a unique feature of Streamable HTTP transport
client.close_session();
```

**Key advantages of Streamable HTTP:**
- Unified `/mcp` endpoint for all operations
- Session ID in `Mcp-Session-Id` header (cleaner than query parameters)
- Explicit session termination support
- Better alignment with MCP 2025-06-18 specification
- Returns HTTP 202 Accepted for async processing

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

## ~~Batch Request Support~~ - **DEPRECATED in MCP 2025-06-18**

**⚠️ JSON-RPC batching is NO LONGER SUPPORTED as of MCP 2025-06-18.**

### What Changed?

MCP 2025-03-26 required implementations to support JSON-RPC batches, but this requirement was **removed in MCP 2025-06-18**. The server now rejects batch requests with:

- **HTTP 400 Bad Request**
- **Error code**: `-32600` (Invalid Request)
- **Error message**: "JSON-RPC batching is not supported in MCP 2025-06-18+. Please send individual requests instead of arrays."

### Migration Guide

If you were using batch requests:

**Before (MCP 2025-03-26):**
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
  }
]
```

**After (MCP 2025-06-18+):**
```json
// Send as separate requests
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/list"
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "resources/list"
}
```

### Historical Reference (MCP 2025-03-26)

<details>
<summary>Click to see original batch request documentation</summary>

MCP 2025-03-26 required implementations to support receiving JSON-RPC batches. This framework previously implemented batch request processing according to the JSON-RPC 2.0 specification.

#### What was a Batch Request?

A batch request allowed clients to send multiple JSON-RPC requests in a single HTTP call by wrapping them in a JSON array.

#### Server Behavior (Deprecated)

The server previously handled batch requests as follows:

1. **Multiple Requests**: Processed all requests and returned an array of responses in the same order
2. **Mixed Batches**: Supported requests and notifications together; only requests got responses
3. **Notification-Only Batches**: Returned HTTP 202 Accepted with no response body
4. **Empty Batches**: Returned HTTP 400 error (invalid per JSON-RPC 2.0)
5. **Single Item Batches**: Valid and processed normally

This batch contains:
- Two requests (with IDs) that would receive responses
- One notification (no ID) that would be processed without a response

Expected Response:
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

The response array contained only the results for the two requests, not for the notification.

</details>

### Testing (Deprecated)

The batch example (`examples/batch_example.cpp`) still exists for historical reference:

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

## Lifecycle Management and Cancellation

The framework implements strict lifecycle management according to MCP 2025-03-26:

### Lifecycle States

Sessions transition through these states:
- **Uninitialized**: Session created, waiting for initialize request
- **Initializing**: Initialize request received, waiting for notifications/initialized
- **Ready**: Session fully initialized and ready for operations
- **Shutdown**: Session shutting down

### Lifecycle Rules

1. **Initialize must be first**: The `initialize` request must be the first message (except `ping`)
2. **No batch initialization**: Initialize requests cannot be part of JSON-RPC batches
3. **Initialized notification required**: After `initialize` response, client must send `notifications/initialized` before other requests
4. **Ping allowed anytime**: `ping` requests are allowed in any lifecycle state

### Configurable Request Timeout

```cpp
mcp::server::configuration config;
config.request_timeout_seconds = 300;  // 5 minute timeout (0 = no timeout)
mcp::server server(config);
```

### Cancellation Notification Handling

Register a handler for cancellation notifications:

```cpp
server.set_cancellation_handler([](const json& request_id, const std::string& reason, const std::string& session_id) {
    std::cout << "Request " << request_id << " cancelled: " << reason << std::endl;
    // Clean up resources, stop processing, etc.
});
```

Clients can send cancellation notifications:

```json
{
    "jsonrpc": "2.0",
    "method": "notifications/cancelled",
    "params": {
        "requestId": "123",
        "reason": "User requested cancellation"
    }
}
```

## Session Management

The MCP server provides built-in session management for tracking state across multiple tool calls within a session. Each connected client gets a unique session ID, and you can store arbitrary JSON data associated with that session.

### Session ID

Session IDs are automatically generated as UUIDs (format: `8-4-4-4-12` hexadecimal digits) and are passed via the `Mcp-Session-Id` HTTP header. Clients can also provide their own session ID in the header if needed.

### Session State Storage

You can store and retrieve arbitrary JSON state data for each session:

```cpp
// Store session state
json state = {
    {"user", "alice"},
    {"counter", 42},
    {"preferences", {
        {"theme", "dark"},
        {"language", "en"}
    }}
};
server.set_session_state(session_id, state);

// Retrieve session state
json retrieved_state = server.get_session_state(session_id);

// Clear session state manually
server.clear_session_state(session_id);
```

### Example: Stateful Tool

Here's an example of a tool that maintains a counter per session:

```cpp
tool increment_counter = tool_builder("increment_counter")
    .with_description("Increments a counter for the current session")
    .build();

server.register_tool(increment_counter, [&server](const json& params, const std::string& session_id) -> json {
    // Get current session state
    json state = server.get_session_state(session_id);
    
    // Initialize or increment counter
    int counter = state.is_null() ? 0 : state.value("counter", 0);
    counter++;
    
    // Update session state
    state["counter"] = counter;
    server.set_session_state(session_id, state);
    
    return {{"counter", counter}};
});
```

### Automatic Cleanup

Session state is automatically cleaned up when a session is closed (on disconnect or DELETE request). You can also register cleanup handlers:

```cpp
server.register_session_cleanup("my_tool", [&server](const std::string& session_id) {
    // Custom cleanup logic
    json state = server.get_session_state(session_id);
    // ... perform cleanup actions
});
```

For a complete working example, see [`examples/session_state_example.cpp`](examples/session_state_example.cpp).


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

## Code Style and Formatting

This project follows a modern C++ style guide to ensure consistency and maintainability across the codebase.

### Style Guidelines

**Naming Conventions:**
- `snake_case` for variables, functions, and namespaces
- `PascalCase` for class/struct names
- Descriptive, meaningful names for all symbols

**Modern C++ Practices:**
- Use `auto` for local variable type deduction where it improves readability
- Prefer trailing return types for functions (except simple getters/setters)
- Use C++23 features and idioms
- Follow RAII principles for resource management

**File Naming:**
- `.h` for header files (current standard)
- `.cpp` for source files
- `.cppm` for module interfaces (when using C++20 modules)

### Formatting Tools

The project uses automated formatting to maintain consistency:

**EditorConfig** (`.editorconfig`):
- Basic whitespace and indentation rules
- 4 spaces for C++ files
- UTF-8 encoding, LF line endings
- Trim trailing whitespace

**clang-format** (`.clang-format`):
- Based on LLVM coding style
- 120 character line limit
- 4-space indentation
- Enforces consistent spacing, alignment, and bracing

### Running Formatting Locally

Before submitting code, ensure it's properly formatted:

```bash
# Check if files need formatting (dry-run)
clang-format --dry-run --Werror src/**/*.cpp include/**/*.h

# Format all C++ files in-place
find src include examples test -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Or format specific files
clang-format -i src/mcp_server.cpp include/mcp_server.h
```

**Recommended IDE/Editor Integration:**
- **VS Code**: Install "C/C++" and "EditorConfig" extensions
- **CLion**: Built-in support for `.editorconfig` and clang-format
- **Vim/Neovim**: Use `vim-clang-format` plugin
- **Emacs**: Use `clang-format.el`

Most modern editors will automatically apply formatting on save when configured.

### CI Enforcement

All pull requests must pass automated style checks in CI:
- clang-format verifies code formatting
- Builds fail on non-conforming code
- Ensures consistency across all contributions

See `.github/workflows/test.yml` for the complete CI configuration.

### Why These Conventions?

**Readability**: Consistent style makes code easier to read and understand
**Maintainability**: Automated formatting reduces bike-shedding and merge conflicts
**Modern C++**: Following modern practices improves code quality and safety
**Community Standards**: Based on widely-adopted LLVM style guide

For detailed development guidelines, see [AGENTS.md](AGENTS.md).

## Protocol Conformance and Testing

### MCP 2025-06-18 Compliance

This C++ implementation fully conforms to the [MCP 2025-06-18 specification](https://spec.modelcontextprotocol.io/specification/2025-06-18/) with comprehensive test coverage.

**Protocol Versions Supported:**
- ✅ **2025-06-18** (Current - Full compliance)
- ✅ 2025-03-26 (Backward compatibility)
- ✅ 2025-11-25 (Latest - Extensions ready)

**Conformance Status:**
- **Total Tests**: 201+ test cases across 16+ test suites
- **Pass Rate**: 100%
- **Code Coverage**: >80% for core protocol features
- **Breaking Changes**: All MCP 2025-06-18 breaking changes implemented

### Key Compliance Features

#### 1. JSON-RPC Batching Removal ✅
**Status:** Fully Compliant
- Server properly rejects batch requests with error code -32600
- Test suite: `test/batch_rejection_test.cpp` (4 tests)
- Reference: [MCP PR #416](https://github.com/modelcontextprotocol/specification/pull/416)

#### 2. MCP-Protocol-Version Header ✅
**Status:** Fully Compliant
- Clients automatically include MCP-Protocol-Version header after initialization
- Server validates header and supports multiple protocol versions
- Backward compatible (missing header assumes 2025-03-26)
- Test suite: `test/protocol_version_header_test.cpp` (8 tests)
- Reference: [MCP PR #548](https://github.com/modelcontextprotocol/specification/pull/548)

#### 3. Structured Tool Output Schema ✅
**Status:** Fully Compliant
- Tools support optional `title` and `outputSchema` fields
- Tool results can include `structuredContent` with schema validation
- Fully backward compatible with text-only content
- Test suite: `test/structured_tool_output_test.cpp` (15 tests)
- Reference: [MCP PR #371](https://github.com/modelcontextprotocol/specification/pull/371)
- Example: `examples/structured_tool_example.cpp`

#### 4. Elicitation (Human-in-the-Loop) ✅
**Status:** Fully Implemented
- Data structures and API methods complete
- Full async request-response flow working
- Promise/future mechanism for pending requests
- Timeout handling implemented
- Client capability declaration supported
- Test suite: `test/elicitation_test.cpp` (15 tests) + `test/elicitation_integration_test.cpp` (8 tests)
- Reference: [MCP PR #382](https://github.com/modelcontextprotocol/specification/pull/382)
- Example: `examples/elicitation_example.cpp`

**Elicitation Features:**
- Request user input during tool execution
- Structured forms with JSON Schema validation
- Three-action model: accept, decline, cancel
- Support for multiple primitive types (string, number, boolean, enum)
- Automatic timeout handling with configurable duration

**API Usage:**
```cpp
// Check if client supports elicitation
if (server.client_supports_elicitation(session_id)) {
    // Define requested schema
    json schema = {
        {"type", "object"},
        {"properties", {
            {"api_key", {{"type", "string"}, {"description", "Your API key"}}}
        }},
        {"required", json::array({"api_key"})}
    };
    
    // Request user input - this blocks until user responds or timeout
    try {
        elicitation_result result = server.request_elicitation(
            session_id, "Please provide your API key", schema);
        
        if (result.action == elicitation_action::accept) {
            std::string api_key = result.content["api_key"];
            // Use the provided API key
        } else if (result.action == elicitation_action::decline) {
            // User declined - handle appropriately
        } else {
            // User cancelled - handle appropriately
        }
    } catch (const mcp_exception& e) {
        // Handle timeout or other errors
    }
}
```

#### 5. Completion Support ✅
**Status:** Fully Implemented (MCP 2025-06-18)
- Data structures for completion requests and results
- Support for `_meta` field with extensible metadata
- Support for `context` field with previously-resolved variables
- Handles both prompt argument and resource template completions
- Test suite: `test/completion_test.cpp` (17 tests)
- Reference: [MCP PR #598](https://github.com/modelcontextprotocol/specification/pull/598)
- Example: `examples/completion_example.cpp`

**Completion Features:**
- Argument autocompletion for prompts (e.g., suggesting values for prompt parameters)
- Resource template variable completion (e.g., file path suggestions)
- Context-aware suggestions using previously-resolved variables
- Extensible metadata via `_meta` field (caching info, timestamps, sources, etc.)
- Standard JSON serialization/deserialization

**API Usage:**
```cpp
// Register completion handler
server.register_method("completion/complete", 
    [](const json& params, const std::string& session_id) -> json {
        // Parse request with context and metadata
        auto req = complete_request::from_json(params);
        
        // Generate completions based on ref type
        complete_result result;
        
        if (req.ref_type == "ref/prompt") {
            // Handle prompt argument completion
            result.values = get_prompt_completions(
                req.ref_name, req.argument_name, req.argument_value);
        } else if (req.ref_type == "ref/resource") {
            // Handle resource template variable completion
            result.values = get_resource_completions(
                req.ref_uri, req.argument_name, req.argument_value);
        }
        
        // Use context for better suggestions
        if (req.context.contains("arguments")) {
            // Access previously-resolved variables
            auto prev_args = req.context["arguments"];
            // Filter or prioritize completions based on context
        }
        
        // Add metadata
        result.total = static_cast<int>(result.values.size());
        result.has_more = false;
        result.meta["source"] = "builtin";
        result.meta["cached"] = false;
        
        return result.to_json();
    }
);

// Declare completion capability
json capabilities = {
    {"tools", json::object()},
    {"completions", json::object()} // Enable completion support
};
server.set_capabilities(capabilities);
```

**Completion Request with Context:**
```cpp
// Client sends completion request with context
complete_request req;
req.ref_type = "ref/prompt";
req.ref_name = "code_review";
req.argument_name = "language";
req.argument_value = "py";

// Add context with previously-resolved arguments
req.context = json::object();
req.context["arguments"] = {
    {"repo", "cpp-mcp"},
    {"branch", "main"}
};

json request = req.to_json();
// Send to server via completion/complete method
```

**Completion Result with Metadata:**
```cpp
// Server returns completions with metadata
complete_result result;
result.values = {"python", "pytorch", "pyside"};
result.total = 10; // Total available completions
result.has_more = false; // No additional completions

// Add extensible metadata
result.meta["source"] = "builtin";
result.meta["cached"] = false;
result.meta["timestamp"] = "2026-02-17T05:30:00Z";
result.meta["context_used"] = true;

json response = result.to_json();
```

### Running Conformance Tests

```bash
# Build with tests enabled
cmake --preset dev-release
cmake --build --preset dev-release

# Run all conformance tests
ctest --preset dev-release

# Or run test executable directly
./build/dev-release/test/mcp_tests

# Run specific test suites
./build/dev-release/test/mcp_tests --run_test=ProtocolVersionHeaderTestSuite
./build/dev-release/test/mcp_tests --run_test=StructuredToolOutputTestSuite
./build/dev-release/test/mcp_tests --run_test=BatchRejectionTestSuite
```

### Interoperability Testing

The C++ MCP implementation is designed to be interoperable with reference implementations:

**Reference SDKs:**
- **Python SDK**: [modelcontextprotocol/python-sdk](https://github.com/modelcontextprotocol/python-sdk) - 126 test files
- **TypeScript SDK**: [modelcontextprotocol/typescript-sdk](https://github.com/modelcontextprotocol/typescript-sdk)
- **Conformance Tests**: [modelcontextprotocol/conformance](https://github.com/modelcontextprotocol/conformance)

**Test with MCP Inspector:**
```bash
# Start your C++ server
./build/dev-release/examples/server_example

# In another terminal, start the inspector
npx @modelcontextprotocol/inspector

# Connect to: http://localhost:8080/mcp
```

**Interop Matrix:**

| Feature | Python SDK | Node.js SDK | C++ SDK (this) |
|---------|------------|-------------|----------------|
| Protocol 2025-06-18 | ✅ | ✅ | ✅ |
| HTTP/SSE Transport | ✅ | ✅ | ✅ |
| Structured Tools | ✅ | ✅ | ✅ |
| Session Management | ✅ | ✅ | ✅ |
| Batch Rejection | ✅ | ✅ | ✅ |

### Test Coverage by Category

| Category | Test Suite | Tests | Status |
|----------|------------|-------|--------|
| Protocol Version | `protocol_version_header_test.cpp` | 8 | ✅ |
| Structured Tools | `structured_tool_output_test.cpp` | 15 | ✅ |
| Elicitation | `elicitation_test.cpp` + `elicitation_integration_test.cpp` | 23 | ✅ |
| Batch Rejection | `batch_rejection_test.cpp` | 4 | ✅ |
| Lifecycle | `lifecycle_compliance_test.cpp` | 12+ | ✅ |
| Session Management | `session_management_test.cpp` | 10+ | ✅ |
| HTTP Security | `http_security_test.cpp` | 20+ | ✅ |
| JSON-RPC | `jsonrpc_validation_test.cpp` | 15+ | ✅ |
| Tool Safety | `tool_safety_test.cpp` | 8+ | ✅ |
| HTTP Transport | `streamable_http_transport_test.cpp` | 15+ | ✅ |

### Known Limitations

**By Design (Not Implemented):**
- **OAuth/Authentication**: Left to application layer for flexibility
  - See [SECURITY.md](SECURITY.md) for security guidance

**Recently Completed:**
- ✅ **Elicitation Support**: Full implementation complete with async request-response flow
  - All data structures, API methods, and multi-turn workflow working
  - 23/23 tests passing

These omissions are documented in [CONFORMANCE.md](CONFORMANCE.md) with references to equivalent tests in the Python SDK.

### Detailed Conformance Documentation

For comprehensive conformance documentation including:
- Complete test-to-requirement mapping
- Protocol MUST/SHOULD/MAY requirements checklist
- Reference links to official test suites
- Interoperability guidance
- Contributing guidelines for tests

See **[CONFORMANCE.md](CONFORMANCE.md)**

### CI/CD Testing

All tests run automatically on:
- **Linux** (Ubuntu latest) - Release build
- **Windows** (Windows latest) - Release build

**CI Workflows:**
- `.github/workflows/test.yml` - Unit and integration tests (201+ tests)
- `.github/workflows/conformance.yml` - Official MCP conformance tests (29 scenarios)

CI fails on:
- Any test failure
- Code formatting violations
- Compilation errors/warnings
- Conformance test regressions (unexpected failures)

See workflow files for complete CI configuration.

## License

This framework is provided under the MIT license. For details, please see the LICENSE file.

