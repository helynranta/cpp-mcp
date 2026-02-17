/**
 * @file completion_test.cpp
 * @brief Tests for MCP completion request/response with _meta and context fields
 *
 * Tests the implementation of completion structures as defined in MCP 2025-06-18:
 * - CompleteRequest with context field
 * - CompleteResult with _meta field
 * - Proper JSON serialization/deserialization
 */

#include "mcp_message.h"
#include <boost/test/unit_test.hpp>

using namespace mcp;

BOOST_AUTO_TEST_SUITE(completion_test_suite)

/**
 * Test that CompleteRequest can be created with basic parameters
 */
BOOST_AUTO_TEST_CASE(complete_request_basic) {
    complete_request req;
    req.ref_type = "ref/prompt";
    req.ref_name = "code_review";
    req.argument_name = "language";
    req.argument_value = "py";

    // Verify fields are set correctly
    BOOST_CHECK_EQUAL(req.ref_type, "ref/prompt");
    BOOST_CHECK_EQUAL(req.ref_name, "code_review");
    BOOST_CHECK_EQUAL(req.argument_name, "language");
    BOOST_CHECK_EQUAL(req.argument_value, "py");
}

/**
 * Test that CompleteRequest serializes to proper JSON format
 */
BOOST_AUTO_TEST_CASE(complete_request_to_json) {
    complete_request req;
    req.ref_type = "ref/prompt";
    req.ref_name = "code_review";
    req.argument_name = "language";
    req.argument_value = "py";

    json j = req.to_json();

    // Verify JSON structure
    BOOST_CHECK_EQUAL(j["ref"]["type"].get<std::string>(), "ref/prompt");
    BOOST_CHECK_EQUAL(j["ref"]["name"].get<std::string>(), "code_review");
    BOOST_CHECK_EQUAL(j["argument"]["name"].get<std::string>(), "language");
    BOOST_CHECK_EQUAL(j["argument"]["value"].get<std::string>(), "py");
}

/**
 * Test that CompleteRequest can include context field
 */
BOOST_AUTO_TEST_CASE(complete_request_with_context) {
    complete_request req;
    req.ref_type = "ref/prompt";
    req.ref_name = "code_review";
    req.argument_name = "language";
    req.argument_value = "py";
    
    // Add context with previously-resolved arguments
    req.context = json::object();
    req.context["arguments"] = json::object();
    req.context["arguments"]["repo"] = "cpp-mcp";
    req.context["arguments"]["branch"] = "main";

    json j = req.to_json();

    // Verify context is included in JSON
    BOOST_CHECK(j.contains("context"));
    BOOST_CHECK(j["context"].contains("arguments"));
    BOOST_CHECK_EQUAL(j["context"]["arguments"]["repo"].get<std::string>(), "cpp-mcp");
    BOOST_CHECK_EQUAL(j["context"]["arguments"]["branch"].get<std::string>(), "main");
}

/**
 * Test that CompleteRequest context is optional
 */
BOOST_AUTO_TEST_CASE(complete_request_context_optional) {
    complete_request req;
    req.ref_type = "ref/prompt";
    req.ref_name = "code_review";
    req.argument_name = "language";
    req.argument_value = "py";
    // Don't set context

    json j = req.to_json();

    // Context should not be present in JSON
    BOOST_CHECK(!j.contains("context"));
}

/**
 * Test that CompleteRequest can be created from JSON
 */
BOOST_AUTO_TEST_CASE(complete_request_from_json) {
    json j = {
        {"ref", {{"type", "ref/prompt"}, {"name", "code_review"}}},
        {"argument", {{"name", "language"}, {"value", "py"}}}
    };

    auto req = complete_request::from_json(j);

    BOOST_CHECK_EQUAL(req.ref_type, "ref/prompt");
    BOOST_CHECK_EQUAL(req.ref_name, "code_review");
    BOOST_CHECK_EQUAL(req.argument_name, "language");
    BOOST_CHECK_EQUAL(req.argument_value, "py");
}

/**
 * Test that CompleteRequest can be created from JSON with context
 */
BOOST_AUTO_TEST_CASE(complete_request_from_json_with_context) {
    json j = {
        {"ref", {{"type", "ref/prompt"}, {"name", "code_review"}}},
        {"argument", {{"name", "language"}, {"value", "py"}}},
        {"context", {{"arguments", {{"repo", "cpp-mcp"}}}}}
    };

    auto req = complete_request::from_json(j);

    BOOST_CHECK_EQUAL(req.ref_type, "ref/prompt");
    BOOST_CHECK_EQUAL(req.ref_name, "code_review");
    BOOST_CHECK(req.context.contains("arguments"));
    BOOST_CHECK_EQUAL(req.context["arguments"]["repo"].get<std::string>(), "cpp-mcp");
}

/**
 * Test that CompleteResult can be created with basic parameters
 */
BOOST_AUTO_TEST_CASE(complete_result_basic) {
    complete_result result;
    result.values = {"python", "pytorch", "pyside"};
    result.total = 10;
    result.has_more = true;

    BOOST_CHECK_EQUAL(result.values.size(), 3);
    BOOST_CHECK_EQUAL(result.values[0], "python");
    BOOST_CHECK_EQUAL(result.values[1], "pytorch");
    BOOST_CHECK_EQUAL(result.values[2], "pyside");
    BOOST_CHECK_EQUAL(result.total, 10);
    BOOST_CHECK_EQUAL(result.has_more, true);
}

/**
 * Test that CompleteResult serializes to proper JSON format
 */
BOOST_AUTO_TEST_CASE(complete_result_to_json) {
    complete_result result;
    result.values = {"python", "pytorch"};
    result.total = 5;
    result.has_more = false;

    json j = result.to_json();

    // Verify JSON structure
    BOOST_CHECK(j.contains("completion"));
    BOOST_CHECK(j["completion"].contains("values"));
    BOOST_CHECK_EQUAL(j["completion"]["values"].size(), 2);
    BOOST_CHECK_EQUAL(j["completion"]["values"][0].get<std::string>(), "python");
    BOOST_CHECK_EQUAL(j["completion"]["values"][1].get<std::string>(), "pytorch");
    BOOST_CHECK_EQUAL(j["completion"]["total"].get<int>(), 5);
    // hasMore is false (default), so it should not be in JSON
    BOOST_CHECK(!j["completion"].contains("hasMore"));
}

/**
 * Test that CompleteResult includes hasMore when true
 */
BOOST_AUTO_TEST_CASE(complete_result_has_more_true) {
    complete_result result;
    result.values = {"python"};
    result.total = 10;
    result.has_more = true;

    json j = result.to_json();

    // hasMore is true, so it should be in JSON
    BOOST_CHECK(j["completion"].contains("hasMore"));
    BOOST_CHECK_EQUAL(j["completion"]["hasMore"].get<bool>(), true);
}

/**
 * Test that CompleteResult can include _meta field
 */
BOOST_AUTO_TEST_CASE(complete_result_with_meta) {
    complete_result result;
    result.values = {"python"};
    
    // Add _meta field with custom metadata
    result.meta["source"] = "builtin";
    result.meta["cached"] = true;
    result.meta["timestamp"] = "2026-02-17T05:00:00Z";

    json j = result.to_json();

    // Verify _meta is included in JSON
    BOOST_CHECK(j.contains("_meta"));
    BOOST_CHECK_EQUAL(j["_meta"]["source"].get<std::string>(), "builtin");
    BOOST_CHECK_EQUAL(j["_meta"]["cached"].get<bool>(), true);
    BOOST_CHECK_EQUAL(j["_meta"]["timestamp"].get<std::string>(), "2026-02-17T05:00:00Z");
}

/**
 * Test that CompleteResult _meta is optional
 */
BOOST_AUTO_TEST_CASE(complete_result_meta_optional) {
    complete_result result;
    result.values = {"python"};
    // Don't set meta

    json j = result.to_json();

    // _meta should not be present in JSON
    BOOST_CHECK(!j.contains("_meta"));
}

/**
 * Test that CompleteResult can be created from JSON
 */
BOOST_AUTO_TEST_CASE(complete_result_from_json) {
    json j = {
        {"completion", {
            {"values", {"python", "pytorch"}},
            {"total", 5},
            {"hasMore", false}
        }}
    };

    auto result = complete_result::from_json(j);

    BOOST_CHECK_EQUAL(result.values.size(), 2);
    BOOST_CHECK_EQUAL(result.values[0], "python");
    BOOST_CHECK_EQUAL(result.values[1], "pytorch");
    BOOST_CHECK_EQUAL(result.total, 5);
    BOOST_CHECK_EQUAL(result.has_more, false);
}

/**
 * Test that CompleteResult can be created from JSON with _meta
 */
BOOST_AUTO_TEST_CASE(complete_result_from_json_with_meta) {
    json j = {
        {"completion", {
            {"values", {"python"}},
            {"total", 1},
            {"hasMore", false}
        }},
        {"_meta", {
            {"source", "builtin"},
            {"cached", true}
        }}
    };

    auto result = complete_result::from_json(j);

    BOOST_CHECK_EQUAL(result.values.size(), 1);
    BOOST_CHECK_EQUAL(result.values[0], "python");
    BOOST_CHECK(result.meta.contains("source"));
    BOOST_CHECK_EQUAL(result.meta["source"].get<std::string>(), "builtin");
    BOOST_CHECK_EQUAL(result.meta["cached"].get<bool>(), true);
}

/**
 * Test that CompleteResult handles optional total field
 */
BOOST_AUTO_TEST_CASE(complete_result_optional_total) {
    json j = {
        {"completion", {
            {"values", {"python"}},
            {"hasMore", false}
        }}
    };

    auto result = complete_result::from_json(j);

    BOOST_CHECK_EQUAL(result.values.size(), 1);
    BOOST_CHECK_EQUAL(result.total, 0); // Default value when not provided
}

/**
 * Test that CompleteResult handles optional hasMore field
 */
BOOST_AUTO_TEST_CASE(complete_result_optional_has_more) {
    json j = {
        {"completion", {
            {"values", {"python"}},
            {"total", 1}
        }}
    };

    auto result = complete_result::from_json(j);

    BOOST_CHECK_EQUAL(result.values.size(), 1);
    BOOST_CHECK_EQUAL(result.has_more, false); // Default value when not provided
}

/**
 * Test resource template reference type
 */
BOOST_AUTO_TEST_CASE(complete_request_resource_template_reference) {
    complete_request req;
    req.ref_type = "ref/resource";
    req.ref_uri = "file:///{repo}/src/{file}";
    req.argument_name = "file";
    req.argument_value = "main";

    json j = req.to_json();

    BOOST_CHECK_EQUAL(j["ref"]["type"].get<std::string>(), "ref/resource");
    BOOST_CHECK_EQUAL(j["ref"]["uri"].get<std::string>(), "file:///{repo}/src/{file}");
}

/**
 * Test that CompleteRequest validates it has either name or uri
 */
BOOST_AUTO_TEST_CASE(complete_request_validation) {
    // Create request with ref_name (prompt reference)
    complete_request req1;
    req1.ref_type = "ref/prompt";
    req1.ref_name = "test";
    req1.argument_name = "arg";
    req1.argument_value = "val";
    
    json j1 = req1.to_json();
    BOOST_CHECK(j1["ref"].contains("name"));
    BOOST_CHECK(!j1["ref"].contains("uri"));

    // Create request with ref_uri (resource reference)
    complete_request req2;
    req2.ref_type = "ref/resource";
    req2.ref_uri = "file:///test";
    req2.argument_name = "arg";
    req2.argument_value = "val";
    
    json j2 = req2.to_json();
    BOOST_CHECK(!j2["ref"].contains("name"));
    BOOST_CHECK(j2["ref"].contains("uri"));
}

BOOST_AUTO_TEST_SUITE_END()
