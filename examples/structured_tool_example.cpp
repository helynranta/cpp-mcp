/**
 * @file structured_tool_example.cpp
 * @brief Example demonstrating MCP 2025-06-18 structured tool output schema
 *
 * This example shows how to:
 * 1. Define tools with optional title and outputSchema fields
 * 2. Return structured content matching the output schema
 * 3. Maintain backward compatibility with text content
 */

#include "mcp_server.h"
#include "mcp_tool.h"

#include <iostream>
#include <thread>

using namespace mcp;

int main() {
    std::cout << "=== MCP 2025-06-18 Structured Tool Output Example ===" << std::endl;
    std::cout << std::endl;

    // Create server configuration
    server::configuration srv_conf;
    srv_conf.port = 8080;
    srv_conf.host = "0.0.0.0";

    // Create an MCP server
    server mcp_server(srv_conf);

    // Example 1: Weather tool with structured output schema
    // This demonstrates the new MCP 2025-06-18 features
    json weather_output_schema = {{"type", "object"},
                                  {"properties",
                                   {{"temperature", {{"type", "number"}, {"description", "Temperature in celsius"}}},
                                    {"conditions", {{"type", "string"}, {"description", "Current weather conditions"}}},
                                    {"humidity", {{"type", "number"}, {"description", "Humidity percentage (0-100)"}}},
                                    {"wind_speed", {{"type", "number"}, {"description", "Wind speed in km/h"}}}}},
                                  {"required", json::array({"temperature", "conditions", "humidity"})}};

    tool weather_tool = tool_builder("get_weather")
                            .with_title("Weather Information Retriever") // NEW in 2025-06-18
                            .with_description("Get current weather data for a specific location")
                            .with_string_param("location", "City name or zip code", true)
                            .with_output_schema(weather_output_schema) // NEW in 2025-06-18
                            .with_read_only(true)                      // Tool doesn't modify state
                            .build();

    // Register the weather tool with a handler
    mcp_server.register_tool(weather_tool, [](const json& params, const std::string& session_id) -> json {
        std::string location = params["location"];

        // Simulate fetching weather data
        json structured_data = {
            {"temperature", 22.5}, {"conditions", "Partly cloudy"}, {"humidity", 65}, {"wind_speed", 15.3}};

        // MCP 2025-06-18: Return both structured content AND text content
        // This maintains backward compatibility while providing structured data
        return {
            {"content",
             json::array({
                 {{"type", "text"},
                  {"text", "Weather in " + location + ":\n" +
                               "Temperature: " + std::to_string(structured_data["temperature"].get<double>()) + "°C\n" +
                               "Conditions: " + structured_data["conditions"].get<std::string>() + "\n" +
                               "Humidity: " + std::to_string(structured_data["humidity"].get<int>()) + "%\n" +
                               "Wind Speed: " + std::to_string(structured_data["wind_speed"].get<double>()) + " km/h"}},
             })},
            {"structuredContent", structured_data}, // NEW in 2025-06-18: Structured output
            {"isError", false}};
    });

    // Example 2: API query tool with complex nested schema
    json api_output_schema = {
        {"type", "object"},
        {"properties",
         {{"status", {{"type", "string"}, {"enum", json::array({"success", "error", "pending"})}}},
          {"data",
           {{"type", "object"},
            {"properties",
             {{"items", {{"type", "array"}, {"items", {{"type", "string"}}}}},
              {"count", {{"type", "integer"}}},
              {"page", {{"type", "integer"}}}}}}},
          {"metadata",
           {{"type", "object"},
            {"properties", {{"timestamp", {{"type", "string"}}}, {"version", {{"type", "string"}}}}}}}}}};

    tool api_tool = tool_builder("query_api")
                        .with_title("External API Query Tool")
                        .with_description("Query external API and return structured results")
                        .with_string_param("endpoint", "API endpoint to query", true)
                        .with_string_param("method", "HTTP method (GET, POST, etc.)", false)
                        .with_output_schema(api_output_schema)
                        .with_read_only(true)
                        .build();

    mcp_server.register_tool(api_tool, [](const json& params, const std::string& session_id) -> json {
        std::string endpoint = params["endpoint"];
        std::string method = params.contains("method") ? params["method"].get<std::string>() : "GET";

        // Simulate API query
        json structured_response = {
            {"status", "success"},
            {"data", {{"items", json::array({"item1", "item2", "item3"})}, {"count", 3}, {"page", 1}}},
            {"metadata", {{"timestamp", "2025-06-18T10:30:00Z"}, {"version", "2.0"}}}};

        return {{"content",
                 json::array({{{"type", "text"},
                               {"text", std::string("API Query Results:\n") + "Status: " +
                                            structured_response["status"].get<std::string>() + "\n" + "Items found: " +
                                            std::to_string(structured_response["data"]["count"].get<int>()) + "\n"}}})},
                {"structuredContent", structured_response},
                {"isError", false}};
    });

    // Example 3: Calculator tool with simple schema
    json calc_output_schema = {{"type", "object"},
                               {"properties",
                                {{"result", {{"type", "number"}, {"description", "Calculation result"}}},
                                 {"expression", {{"type", "string"}, {"description", "Original expression"}}}}},
                               {"required", json::array({"result", "expression"})}};

    tool calc_tool = tool_builder("calculate")
                         .with_title("Simple Calculator")
                         .with_description("Perform basic arithmetic calculations")
                         .with_number_param("a", "First operand", true)
                         .with_number_param("b", "Second operand", true)
                         .with_string_param("operation", "Operation: add, subtract, multiply, divide", true)
                         .with_output_schema(calc_output_schema)
                         .with_read_only(true)
                         .build();

    mcp_server.register_tool(calc_tool, [](const json& params, const std::string& session_id) -> json {
        double a = params["a"];
        double b = params["b"];
        std::string op = params["operation"];

        double result = 0;
        if (op == "add")
            result = a + b;
        else if (op == "subtract")
            result = a - b;
        else if (op == "multiply")
            result = a * b;
        else if (op == "divide")
            result = (b != 0) ? a / b : 0;

        std::string expr = std::to_string(a) + " " + op + " " + std::to_string(b);

        json structured_result = {{"result", result}, {"expression", expr}};

        return {{"content", json::array({
                                {{"type", "text"}, {"text", expr + " = " + std::to_string(result)}},
                            })},
                {"structuredContent", structured_result},
                {"isError", false}};
    });

    // Example 4: Legacy tool without structured output (backward compatibility)
    tool legacy_tool = tool_builder("legacy_echo")
                           .with_description("Echo tool using legacy 2025-03-26 format")
                           .with_string_param("message", "Message to echo", true)
                           .build(); // No title, no output schema

    mcp_server.register_tool(legacy_tool, [](const json& params, const std::string& session_id) -> json {
        std::string message = params["message"];

        // Legacy tool returns only text content (no structuredContent)
        return {{"content", json::array({
                                {{"type", "text"}, {"text", "Echo: " + message}},
                            })},
                {"isError", false}};
    });

    // Set server information
    mcp_server.set_server_info("Structured Tool Output Example", "1.0.0");

    // Display registered tools
    std::cout << "Registered " << 4 << " tools demonstrating MCP 2025-06-18 features:" << std::endl;
    std::cout << std::endl;

    std::cout << "1. get_weather" << std::endl;
    std::cout << "   - Has title: YES (\"Weather Information Retriever\")" << std::endl;
    std::cout << "   - Has output schema: YES (temperature, conditions, humidity, wind_speed)" << std::endl;
    std::cout << "   - Returns structured content: YES" << std::endl;
    std::cout << std::endl;

    std::cout << "2. query_api" << std::endl;
    std::cout << "   - Has title: YES (\"External API Query Tool\")" << std::endl;
    std::cout << "   - Has output schema: YES (complex nested structure)" << std::endl;
    std::cout << "   - Returns structured content: YES" << std::endl;
    std::cout << std::endl;

    std::cout << "3. calculate" << std::endl;
    std::cout << "   - Has title: YES (\"Simple Calculator\")" << std::endl;
    std::cout << "   - Has output schema: YES (result, expression)" << std::endl;
    std::cout << "   - Returns structured content: YES" << std::endl;
    std::cout << std::endl;

    std::cout << "4. legacy_echo (backward compatibility)" << std::endl;
    std::cout << "   - Has title: NO (maintains 2025-03-26 compatibility)" << std::endl;
    std::cout << "   - Has output schema: NO" << std::endl;
    std::cout << "   - Returns structured content: NO" << std::endl;
    std::cout << std::endl;

    // Start the server
    std::cout << "Starting server on http://0.0.0.0:8080/mcp" << std::endl;
    std::cout << "Server supports both MCP 2025-03-26 and 2025-06-18 clients" << std::endl;
    std::cout << std::endl;
    std::cout << "Example requests:" << std::endl;
    std::cout << std::endl;
    std::cout << "List tools (shows title and outputSchema):" << std::endl;
    std::cout << "  curl -X POST http://localhost:8080/mcp \\" << std::endl;
    std::cout << "    -H \"Content-Type: application/json\" \\" << std::endl;
    std::cout << "    -H \"MCP-Protocol-Version: 2025-06-18\" \\" << std::endl;
    std::cout << "    -d '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{}}'" << std::endl;
    std::cout << std::endl;
    std::cout << "Call weather tool (returns structuredContent):" << std::endl;
    std::cout << "  curl -X POST http://localhost:8080/mcp \\" << std::endl;
    std::cout << "    -H \"Content-Type: application/json\" \\" << std::endl;
    std::cout << "    -H \"MCP-Protocol-Version: 2025-06-18\" \\" << std::endl;
    std::cout << "    -d '{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\"," << std::endl;
    std::cout << "         \"params\":{\"name\":\"get_weather\",\"arguments\":{\"location\":\"New York\"}}}'"
              << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    // Run the server (blocking mode)
    mcp_server.start(true);

    return 0;
}
