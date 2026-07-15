#include "mcp_stdio_server.h"
#include "mcp_tool.h"

#include <iostream>

auto main() -> int {
    mcp::stdio_server server({.input = &std::cin, .output = &std::cout, .error = &std::cerr});
    server.set_server_info("cpp-mcp stdio example", "1.0");
    server.set_capabilities({{"tools", mcp::json::object()}});
    server.register_tool(
        mcp::tool_builder("echo")
            .with_description("Echoes the provided text")
            .with_string_param("text", "Text to echo", true)
            .build(),
        [](const mcp::json& arguments, const std::string&) -> mcp::json {
            return {
                {"content", mcp::json::array({{{"type", "text"}, {"text", arguments.at("text").get<std::string>()}}})}};
        });
    return server.start() ? 0 : 1;
}
