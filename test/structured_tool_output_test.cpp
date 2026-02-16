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

BOOST_AUTO_TEST_SUITE_END()
