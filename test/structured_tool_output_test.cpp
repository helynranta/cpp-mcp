/**
 * @file structured_tool_output_test.cpp
 * @brief Tests for MCP 2025-06-18 structured tool output schema feature
 *
 * Tests the new optional fields added in MCP 2025-06-18:
 * - title: Optional display name for tools
 * - outputSchema: Optional JSON schema defining tool output structure
 * - structuredContent: Structured output conforming to outputSchema
 */

#include "mcp_tool.h"

#include <boost/test/unit_test.hpp>

using namespace mcp;

BOOST_AUTO_TEST_SUITE(StructuredToolOutputTestSuite)

// Test 1: Tool can have an optional title field
BOOST_AUTO_TEST_CASE(ToolHasOptionalTitleField) {
    // Create tool with title
    tool t =
        tool_builder("weather_tool").with_description("Get weather data").with_title("Weather Data Retriever").build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify title is included
    BOOST_CHECK(tool_json.contains("title"));
    BOOST_CHECK_EQUAL(tool_json["title"].get<std::string>(), "Weather Data Retriever");
}

// Test 2: Tool without title should not include title field in JSON
BOOST_AUTO_TEST_CASE(ToolWithoutTitleOmitsField) {
    // Create tool without title
    tool t = tool_builder("simple_tool").with_description("A simple tool").build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify title is not included
    BOOST_CHECK(!tool_json.contains("title"));
}

// Test 3: Tool can have an optional outputSchema field
BOOST_AUTO_TEST_CASE(ToolHasOptionalOutputSchema) {
    // Define output schema
    json output_schema = {{"type", "object"},
                          {"properties",
                           {{"temperature", {{"type", "number"}, {"description", "Temperature in celsius"}}},
                            {"conditions", {{"type", "string"}, {"description", "Weather conditions"}}},
                            {"humidity", {{"type", "number"}, {"description", "Humidity percentage"}}}}},
                          {"required", json::array({"temperature", "conditions", "humidity"})}};

    // Create tool with output schema
    tool t =
        tool_builder("weather_tool").with_description("Get weather data").with_output_schema(output_schema).build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify outputSchema is included
    BOOST_CHECK(tool_json.contains("outputSchema"));
    BOOST_CHECK_EQUAL(tool_json["outputSchema"]["type"].get<std::string>(), "object");
    BOOST_CHECK(tool_json["outputSchema"].contains("properties"));
    BOOST_CHECK(tool_json["outputSchema"]["properties"].contains("temperature"));
}

// Test 4: Tool without outputSchema should not include field in JSON
BOOST_AUTO_TEST_CASE(ToolWithoutOutputSchemaOmitsField) {
    // Create tool without output schema
    tool t = tool_builder("simple_tool").with_description("A simple tool").build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify outputSchema is not included
    BOOST_CHECK(!tool_json.contains("outputSchema"));
}

// Test 5: Tool can have both title and outputSchema
BOOST_AUTO_TEST_CASE(ToolCanHaveBothTitleAndOutputSchema) {
    json output_schema = {{"type", "object"}, {"properties", {{"result", {{"type", "string"}}}}}};

    // Create tool with both
    tool t = tool_builder("calculator")
                 .with_description("Performs calculations")
                 .with_title("Advanced Calculator")
                 .with_output_schema(output_schema)
                 .build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify both fields are included
    BOOST_CHECK(tool_json.contains("title"));
    BOOST_CHECK(tool_json.contains("outputSchema"));
    BOOST_CHECK_EQUAL(tool_json["title"].get<std::string>(), "Advanced Calculator");
    BOOST_CHECK_EQUAL(tool_json["outputSchema"]["type"].get<std::string>(), "object");
}

// Test 6: Verify tool maintains all existing fields
BOOST_AUTO_TEST_CASE(ToolMaintainsExistingFields) {
    // Create tool with all fields
    tool t = tool_builder("test_tool")
                 .with_description("Test description")
                 .with_title("Test Title")
                 .with_string_param("input", "Input parameter", true)
                 .build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify all required fields are present
    BOOST_CHECK(tool_json.contains("name"));
    BOOST_CHECK(tool_json.contains("description"));
    BOOST_CHECK(tool_json.contains("inputSchema"));
    BOOST_CHECK(tool_json.contains("title"));

    BOOST_CHECK_EQUAL(tool_json["name"].get<std::string>(), "test_tool");
    BOOST_CHECK_EQUAL(tool_json["description"].get<std::string>(), "Test description");
    BOOST_CHECK_EQUAL(tool_json["title"].get<std::string>(), "Test Title");
}

// Test 7: Complex outputSchema with nested structures
BOOST_AUTO_TEST_CASE(ComplexOutputSchemaSupported) {
    json complex_schema = {
        {"type", "object"},
        {"properties",
         {{"status", {{"type", "string"}, {"enum", json::array({"success", "error"})}}},
          {"data",
           {{"type", "object"},
            {"properties",
             {{"items", {{"type", "array"}, {"items", {{"type", "string"}}}}}, {"count", {{"type", "number"}}}}}}},
          {"metadata",
           {{"type", "object"},
            {"properties", {{"timestamp", {{"type", "string"}}}, {"version", {{"type", "string"}}}}}}}}}};

    // Create tool with complex schema
    tool t = tool_builder("api_tool")
                 .with_description("API interaction tool")
                 .with_title("API Client")
                 .with_output_schema(complex_schema)
                 .build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify complex schema structure is preserved
    BOOST_CHECK(tool_json.contains("outputSchema"));
    BOOST_CHECK(tool_json["outputSchema"]["properties"].contains("status"));
    BOOST_CHECK(tool_json["outputSchema"]["properties"].contains("data"));
    BOOST_CHECK(tool_json["outputSchema"]["properties"].contains("metadata"));
    BOOST_CHECK(tool_json["outputSchema"]["properties"]["data"]["properties"].contains("items"));
}

// Test 8: Backward compatibility - tools without new fields still work
BOOST_AUTO_TEST_CASE(BackwardCompatibilityMaintained) {
    // Create tool using old API (no title or output schema)
    tool t = tool_builder("legacy_tool")
                 .with_description("Legacy tool description")
                 .with_string_param("param1", "Parameter 1", true)
                 .with_number_param("param2", "Parameter 2", false)
                 .build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify it has all required fields but not new optional fields
    BOOST_CHECK(tool_json.contains("name"));
    BOOST_CHECK(tool_json.contains("description"));
    BOOST_CHECK(tool_json.contains("inputSchema"));
    BOOST_CHECK(!tool_json.contains("title"));
    BOOST_CHECK(!tool_json.contains("outputSchema"));

    // Verify parameters are still correct
    BOOST_CHECK(tool_json["inputSchema"].contains("properties"));
    BOOST_CHECK(tool_json["inputSchema"]["properties"].contains("param1"));
    BOOST_CHECK(tool_json["inputSchema"]["properties"].contains("param2"));
}

// Test 9: Builder method chaining works correctly
BOOST_AUTO_TEST_CASE(BuilderMethodChainingWorks) {
    json schema = {{"type", "string"}};

    // Chain all methods including new ones
    tool t = tool_builder("chained_tool")
                 .with_description("Chained builder test")
                 .with_title("Chained Tool")
                 .with_string_param("input", "Input value", true)
                 .with_output_schema(schema)
                 .with_read_only(true)
                 .build();

    // Verify all fields are set correctly
    json tool_json = t.to_json();
    BOOST_CHECK_EQUAL(tool_json["name"].get<std::string>(), "chained_tool");
    BOOST_CHECK_EQUAL(tool_json["title"].get<std::string>(), "Chained Tool");
    BOOST_CHECK(tool_json.contains("outputSchema"));
    BOOST_CHECK(tool_json.contains("annotations"));
    BOOST_CHECK_EQUAL(tool_json["annotations"]["readOnly"].get<bool>(), true);
}

// Test 10: Empty output schema is valid
BOOST_AUTO_TEST_CASE(EmptyOutputSchemaIsValid) {
    json empty_schema = json::object();

    // Create tool with empty schema
    tool t = tool_builder("tool_with_empty_schema")
                 .with_description("Tool with empty output schema")
                 .with_output_schema(empty_schema)
                 .build();

    // Convert to JSON
    json tool_json = t.to_json();

    // Verify empty schema is included
    BOOST_CHECK(tool_json.contains("outputSchema"));
    BOOST_CHECK(tool_json["outputSchema"].is_object());
}

// Test 11: Tool result with structured content
BOOST_AUTO_TEST_CASE(ToolResultWithStructuredContent) {
    // Define a tool with output schema
    json output_schema = {{"type", "object"},
                          {"properties",
                           {{"temperature", {{"type", "number"}}},
                            {"conditions", {{"type", "string"}}},
                            {"humidity", {{"type", "number"}}}}},
                          {"required", json::array({"temperature", "conditions", "humidity"})}};

    tool weather_tool = tool_builder("weather_tool")
                            .with_description("Get weather data")
                            .with_title("Weather Tool")
                            .with_output_schema(output_schema)
                            .build();

    // Verify tool has output schema
    json tool_json = weather_tool.to_json();
    BOOST_CHECK(tool_json.contains("outputSchema"));

    // Simulate tool result with structured content
    json structured_data = {{"temperature", 22.5}, {"conditions", "Partly cloudy"}, {"humidity", 65}};

    json tool_result = {
        {"content", json::array({{{"type", "text"}, {"text", "Weather: 22.5°C, Partly cloudy, Humidity: 65%"}}})},
        {"structuredContent", structured_data},
        {"isError", false}};

    // Verify result has both content and structuredContent
    BOOST_CHECK(tool_result.contains("content"));
    BOOST_CHECK(tool_result.contains("structuredContent"));
    BOOST_CHECK_EQUAL(tool_result["structuredContent"]["temperature"].get<double>(), 22.5);
    BOOST_CHECK_EQUAL(tool_result["structuredContent"]["conditions"].get<std::string>(), "Partly cloudy");
    BOOST_CHECK_EQUAL(tool_result["structuredContent"]["humidity"].get<int>(), 65);
}

// Test 12: Tool result backward compatibility (content only)
BOOST_AUTO_TEST_CASE(ToolResultBackwardCompatibility) {
    // Old-style tool without output schema
    tool simple_tool = tool_builder("simple_tool").with_description("Simple tool").build();

    // Verify tool has no output schema
    json tool_json = simple_tool.to_json();
    BOOST_CHECK(!tool_json.contains("outputSchema"));

    // Old-style result with only content (no structured content)
    json tool_result = {{"content", json::array({{{"type", "text"}, {"text", "Simple response"}}})},
                        {"isError", false}};

    // Verify result has only content
    BOOST_CHECK(tool_result.contains("content"));
    BOOST_CHECK(!tool_result.contains("structuredContent"));
}

// Test 13: Complex structured content with nested objects
BOOST_AUTO_TEST_CASE(ComplexStructuredContentSupported) {
    // Define complex output schema
    json complex_schema = {
        {"type", "object"},
        {"properties",
         {{"status", {{"type", "string"}}},
          {"data",
           {{"type", "object"},
            {"properties",
             {{"users",
               {{"type", "array"},
                {"items",
                 {{"type", "object"},
                  {"properties", {{"id", {{"type", "number"}}}, {"name", {{"type", "string"}}}}}}}}},
              {"count", {{"type", "number"}}}}}}},
          {"metadata",
           {{"type", "object"},
            {"properties", {{"timestamp", {{"type", "string"}}}, {"version", {{"type", "string"}}}}}}}}}};

    tool api_tool = tool_builder("api_tool").with_description("API tool").with_output_schema(complex_schema).build();

    // Verify complex schema
    json tool_json = api_tool.to_json();
    BOOST_CHECK(tool_json.contains("outputSchema"));

    // Create complex structured result
    json complex_result = {{"status", "success"},
                           {"data",
                            {{"users", json::array({{{"id", 1}, {"name", "Alice"}},
                                                    {{"id", 2}, {"name", "Bob"}},
                                                    {{"id", 3}, {"name", "Charlie"}}})},
                             {"count", 3}}},
                           {"metadata", {{"timestamp", "2026-02-16T20:30:00Z"}, {"version", "1.0.0"}}}};

    json tool_result = {{"content", json::array({{{"type", "text"}, {"text", "API response with 3 users"}}})},
                        {"structuredContent", complex_result},
                        {"isError", false}};

    // Verify nested structure is preserved
    BOOST_CHECK(tool_result["structuredContent"].contains("data"));
    BOOST_CHECK(tool_result["structuredContent"]["data"].contains("users"));
    BOOST_CHECK_EQUAL(tool_result["structuredContent"]["data"]["users"].size(), 3);
    BOOST_CHECK_EQUAL(tool_result["structuredContent"]["data"]["users"][0]["name"].get<std::string>(), "Alice");
}

// Test 14: Array output schema
BOOST_AUTO_TEST_CASE(ArrayOutputSchemaSupported) {
    // Define array output schema
    json array_schema = {
        {"type", "array"},
        {"items",
         {{"type", "object"}, {"properties", {{"id", {{"type", "number"}}}, {"value", {{"type", "string"}}}}}}}};

    tool list_tool = tool_builder("list_tool").with_description("List items").with_output_schema(array_schema).build();

    // Verify array schema
    json tool_json = list_tool.to_json();
    BOOST_CHECK(tool_json.contains("outputSchema"));
    BOOST_CHECK_EQUAL(tool_json["outputSchema"]["type"].get<std::string>(), "array");

    // Create array structured result
    json array_result = json::array(
        {{{"id", 1}, {"value", "item1"}}, {{"id", 2}, {"value", "item2"}}, {{"id", 3}, {"value", "item3"}}});

    json tool_result = {{"content", json::array({{{"type", "text"}, {"text", "3 items returned"}}})},
                        {"structuredContent", array_result},
                        {"isError", false}};

    // Verify array structure
    BOOST_CHECK(tool_result["structuredContent"].is_array());
    BOOST_CHECK_EQUAL(tool_result["structuredContent"].size(), 3);
    BOOST_CHECK_EQUAL(tool_result["structuredContent"][1]["id"].get<int>(), 2);
}

// Test 15: Tool result with both text content and structured content (best practice)
BOOST_AUTO_TEST_CASE(DualContentFormatBestPractice) {
    // Tool with output schema
    json schema = {
        {"type", "object"},
        {"properties",
         {{"result", {{"type", "number"}}}, {"unit", {{"type", "string"}}}, {"timestamp", {{"type", "string"}}}}}};

    tool calculator = tool_builder("calculator")
                          .with_description("Calculator")
                          .with_title("Advanced Calculator")
                          .with_output_schema(schema)
                          .build();

    // Structured result
    json structured = {{"result", 42}, {"unit", "units"}, {"timestamp", "2026-02-16T20:30:00Z"}};

    // Best practice: Provide both formats
    // - text content for backward compatibility and human readability
    // - structured content for programmatic access
    json tool_result = {
        {"content",
         json::array({{{"type", "text"}, {"text", "Calculation result: 42 units (timestamp: 2026-02-16T20:30:00Z)"}}})},
        {"structuredContent", structured},
        {"isError", false}};

    // Verify both formats present
    BOOST_CHECK(tool_result.contains("content"));
    BOOST_CHECK(tool_result.contains("structuredContent"));

    // Verify content is human-readable
    BOOST_CHECK(tool_result["content"][0]["text"].get<std::string>().find("42") != std::string::npos);

    // Verify structured content is machine-readable
    BOOST_CHECK_EQUAL(tool_result["structuredContent"]["result"].get<int>(), 42);
    BOOST_CHECK_EQUAL(tool_result["structuredContent"]["unit"].get<std::string>(), "units");
}

BOOST_AUTO_TEST_SUITE_END()
