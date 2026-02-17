/**
 * @file elicitation_test.cpp
 * @brief Tests for MCP 2025-06-18 elicitation (human-in-the-loop) feature
 *
 * Tests the elicitation feature that allows servers to request additional
 * information from users during interactions. This implements the multi-turn
 * workflow defined in the MCP 2025-06-18 specification.
 *
 * Key test areas:
 * - Elicitation data structures (params, result, actions)
 * - Client capability declaration
 * - Elicitation request creation and formatting
 * - Response handling (accept, decline, cancel)
 * - JSON Schema validation for requestedSchema
 */

#include "mcp_message.h"
#include "mcp_server.h"

#include <boost/test/unit_test.hpp>

using namespace mcp;

BOOST_AUTO_TEST_SUITE(ElicitationTestSuite)

// ============================================================================
// Test 1: Elicitation params can be created and serialized to JSON
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationParamsCreation) {
    // Create elicitation params
    elicitation_params params;
    params.message = "Please provide your API key";
    params.requested_schema = {{"type", "object"},
                               {"properties", {{"api_key", {{"type", "string"}, {"description", "Your API key"}}}}},
                               {"required", json::array({"api_key"})}};

    // Convert to JSON
    json params_json = params.to_json();

    // Verify JSON structure
    BOOST_CHECK(params_json.contains("message"));
    BOOST_CHECK(params_json.contains("requestedSchema"));
    BOOST_CHECK_EQUAL(params_json["message"].get<std::string>(), "Please provide your API key");
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["type"].get<std::string>(), "object");
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("api_key"));
}

// ============================================================================
// Test 2: Elicitation params can be deserialized from JSON
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationParamsDeserialization) {
    // Create JSON
    json params_json = {{"message", "Enter your username"},
                        {"requestedSchema",
                         {{"type", "object"},
                          {"properties", {{"username", {{"type", "string"}}}}},
                          {"required", json::array({"username"})}}}};

    // Deserialize
    elicitation_params params = elicitation_params::from_json(params_json);

    // Verify
    BOOST_CHECK_EQUAL(params.message, "Enter your username");
    BOOST_CHECK_EQUAL(params.requested_schema["type"].get<std::string>(), "object");
}

// ============================================================================
// Test 3: Elicitation result with accept action
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResultAccept) {
    // Create result with accept action
    elicitation_result result;
    result.action = elicitation_action::accept;
    result.content = {{"username", "testuser"}};

    // Convert to JSON
    json result_json = result.to_json();

    // Verify JSON structure
    BOOST_CHECK(result_json.contains("action"));
    BOOST_CHECK(result_json.contains("content"));
    BOOST_CHECK_EQUAL(result_json["action"].get<std::string>(), "accept");
    BOOST_CHECK_EQUAL(result_json["content"]["username"].get<std::string>(), "testuser");
}

// ============================================================================
// Test 4: Elicitation result with decline action
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResultDecline) {
    // Create result with decline action
    elicitation_result result;
    result.action = elicitation_action::decline;

    // Convert to JSON
    json result_json = result.to_json();

    // Verify JSON structure
    BOOST_CHECK(result_json.contains("action"));
    BOOST_CHECK_EQUAL(result_json["action"].get<std::string>(), "decline");
    // Content should not be present for decline
    BOOST_CHECK(!result_json.contains("content"));
}

// ============================================================================
// Test 5: Elicitation result with cancel action
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResultCancel) {
    // Create result with cancel action
    elicitation_result result;
    result.action = elicitation_action::cancel;

    // Convert to JSON
    json result_json = result.to_json();

    // Verify JSON structure
    BOOST_CHECK(result_json.contains("action"));
    BOOST_CHECK_EQUAL(result_json["action"].get<std::string>(), "cancel");
    // Content should not be present for cancel
    BOOST_CHECK(!result_json.contains("content"));
}

// ============================================================================
// Test 6: Elicitation result deserialization with accept
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResultDeserializeAccept) {
    // Create JSON
    json result_json = {{"action", "accept"}, {"content", {{"email", "user@example.com"}}}};

    // Deserialize
    elicitation_result result = elicitation_result::from_json(result_json);

    // Verify
    BOOST_CHECK(result.action == elicitation_action::accept);
    BOOST_CHECK_EQUAL(result.content["email"].get<std::string>(), "user@example.com");
}

// ============================================================================
// Test 7: Elicitation result deserialization with decline
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResultDeserializeDecline) {
    // Create JSON
    json result_json = {{"action", "decline"}};

    // Deserialize
    elicitation_result result = elicitation_result::from_json(result_json);

    // Verify
    BOOST_CHECK(result.action == elicitation_action::decline);
    BOOST_CHECK(result.content.empty());
}

// ============================================================================
// Test 8: Elicitation result deserialization with invalid action throws
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResultInvalidActionThrows) {
    // Create JSON with invalid action
    json result_json = {{"action", "invalid"}};

    // Verify that from_json throws
    BOOST_CHECK_THROW(elicitation_result::from_json(result_json), mcp_exception);
}

// ============================================================================
// Test 9: Elicitation request message creation
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationRequestCreation) {
    // Create elicitation params
    elicitation_params params;
    params.message = "Please provide your contact information";
    params.requested_schema = {
        {"type", "object"},
        {"properties",
         {{"name", {{"type", "string"}, {"description", "Your full name"}}},
          {"email", {{"type", "string"}, {"format", "email"}, {"description", "Your email address"}}}}},
        {"required", json::array({"name", "email"})}};

    // Create request
    request req = request::create("elicitation/create", params.to_json());

    // Verify request structure
    BOOST_CHECK_EQUAL(req.method, "elicitation/create");
    BOOST_CHECK(!req.is_notification());
    BOOST_CHECK(req.params.contains("message"));
    BOOST_CHECK(req.params.contains("requestedSchema"));
}

// ============================================================================
// Test 10: Server can check if client supports elicitation
// ============================================================================
BOOST_AUTO_TEST_CASE(ServerChecksElicitationSupport) {
    // Create a minimal server configuration
    server::configuration config;
    config.host = "localhost";
    config.port = 8999;
    config.name = "TestServer";
    config.version = "1.0";

    // Create server
    server srv(config);

    // Create a fake session ID
    std::string session_id = "test-session-123";

    // Initially, client doesn't support elicitation
    BOOST_CHECK(!srv.client_supports_elicitation(session_id));
}

// ============================================================================
// Test 11: Complex elicitation schema with multiple types
// ============================================================================
BOOST_AUTO_TEST_CASE(ComplexElicitationSchema) {
    // Create complex schema with multiple primitive types
    json schema = {{"type", "object"},
                   {"properties",
                    {{"name", {{"type", "string"}, {"minLength", 3}, {"maxLength", 50}}},
                     {"age", {{"type", "integer"}, {"minimum", 18}, {"maximum", 120}}},
                     {"email", {{"type", "string"}, {"format", "email"}}},
                     {"subscribe", {{"type", "boolean"}, {"default", false}}},
                     {"role",
                      {{"type", "string"},
                       {"enum", json::array({"user", "admin", "moderator"})},
                       {"enumNames", json::array({"Regular User", "Administrator", "Moderator"})}}}}},
                   {"required", json::array({"name", "email"})}};

    // Create params
    elicitation_params params;
    params.message = "Please fill in your profile";
    params.requested_schema = schema;

    // Convert to JSON
    json params_json = params.to_json();

    // Verify all types are preserved
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("name"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("age"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("email"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("subscribe"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("role"));

    // Verify constraints are preserved
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["name"]["minLength"].get<int>(), 3);
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["age"]["minimum"].get<int>(), 18);
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["email"]["format"].get<std::string>(), "email");
}

// ============================================================================
// Test 12: Elicitation result round-trip (serialize and deserialize)
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResultRoundTrip) {
    // Create original result
    elicitation_result original;
    original.action = elicitation_action::accept;
    original.content = {{"field1", "value1"}, {"field2", 42}, {"field3", true}};

    // Serialize to JSON
    json result_json = original.to_json();

    // Deserialize back
    elicitation_result deserialized = elicitation_result::from_json(result_json);

    // Verify round-trip preserved data
    BOOST_CHECK(deserialized.action == elicitation_action::accept);
    BOOST_CHECK_EQUAL(deserialized.content["field1"].get<std::string>(), "value1");
    BOOST_CHECK_EQUAL(deserialized.content["field2"].get<int>(), 42);
    BOOST_CHECK_EQUAL(deserialized.content["field3"].get<bool>(), true);
}

// ============================================================================
// Test 13: Elicitation params round-trip (serialize and deserialize)
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationParamsRoundTrip) {
    // Create original params
    elicitation_params original;
    original.message = "Test message with unicode: 你好世界 🌍";
    original.requested_schema = {
        {"type", "object"}, {"properties", {{"field", {{"type", "string"}}}}}, {"required", json::array({"field"})}};

    // Serialize to JSON
    json params_json = original.to_json();

    // Deserialize back
    elicitation_params deserialized = elicitation_params::from_json(params_json);

    // Verify round-trip preserved data
    BOOST_CHECK_EQUAL(deserialized.message, original.message);
    BOOST_CHECK_EQUAL(deserialized.requested_schema.dump(), original.requested_schema.dump());
}

// ============================================================================
// Test 14: Empty content for accept action is valid
// ============================================================================
BOOST_AUTO_TEST_CASE(AcceptWithEmptyContent) {
    // Create result with accept action but empty content
    elicitation_result result;
    result.action = elicitation_action::accept;
    result.content = json::object();

    // Convert to JSON
    json result_json = result.to_json();

    // Verify JSON structure - content should not be present for empty object
    BOOST_CHECK(result_json.contains("action"));
    BOOST_CHECK_EQUAL(result_json["action"].get<std::string>(), "accept");
    BOOST_CHECK(!result_json.contains("content")); // Empty content is omitted
}

// ============================================================================
// Test 15: Schema with string formats
// ============================================================================
BOOST_AUTO_TEST_CASE(SchemaWithStringFormats) {
    // Create schema with various string formats
    json schema = {{"type", "object"},
                   {"properties",
                    {{"email", {{"type", "string"}, {"format", "email"}}},
                     {"url", {{"type", "string"}, {"format", "uri"}}},
                     {"date", {{"type", "string"}, {"format", "date"}}},
                     {"datetime", {{"type", "string"}, {"format", "date-time"}}}}},
                   {"required", json::array({"email"})}};

    // Create params
    elicitation_params params;
    params.message = "Please provide your information";
    params.requested_schema = schema;

    // Convert to JSON
    json params_json = params.to_json();

    // Verify formats are preserved
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["email"]["format"].get<std::string>(), "email");
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["url"]["format"].get<std::string>(), "uri");
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["date"]["format"].get<std::string>(), "date");
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["datetime"]["format"].get<std::string>(),
                      "date-time");
}

BOOST_AUTO_TEST_SUITE_END()
