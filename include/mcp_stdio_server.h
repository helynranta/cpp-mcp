/**
 * @file mcp_stdio_server.h
 * @brief MCP server transport over standard input and standard output.
 */

#ifndef MCP_STDIO_SERVER_H
#define MCP_STDIO_SERVER_H

#include "mcp_server.h"

#include <atomic>
#include <istream>
#include <mutex>
#include <ostream>
#include <string>

namespace mcp {

class stdio_server {
public:
    struct configuration {
        std::istream* input;
        std::ostream* output;
        std::ostream* error;
    };

    /** Construct a stdio server. Example streams are `&std::cin`, `&std::cout`, and `&std::cerr`. */
    explicit stdio_server(configuration config);

    /** Read messages until EOF. Example: an MCP client closes stdin to stop the server. */
    bool start();

    /** Request loop termination. Example: application shutdown calls `stop()`. */
    void stop();

    void set_server_info(const std::string& name, const std::string& version);
    void set_capabilities(const json& capabilities);
    void register_method(const std::string& method, method_handler handler);
    void register_notification(const std::string& method, notification_handler handler);
    void register_tool(const tool& value, tool_handler handler);
    bool unregister_tool(const std::string& name);
    /** Atomically replace the catalog. Example: `replace_tools({{paint_tool, paint_handler}})`. */
    bool replace_tools(const std::vector<tool_registration>& catalog);
    std::vector<tool> get_tools() const;
    void set_cancellation_handler(cancellation_handler handler);
    void set_tool_confirmation_handler(tool_confirmation_handler handler);
    void set_session_state(const json& state);
    json get_session_state() const;
    void clear_session_state();
    /** Return the request ID currently executing on this thread. Example: JSON-RPC ID `42` inside a tool handler. */
    [[nodiscard]] std::optional<json> current_request_id() const;

private:
    static constexpr auto session_id = "stdio";

    configuration config_;
    server protocol_;
    std::atomic_bool running_{false};
    mutable std::mutex output_mutex_;

    void write_message(const json& message);
};

} // namespace mcp

#endif // MCP_STDIO_SERVER_H
