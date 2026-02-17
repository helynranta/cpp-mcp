/**
 * @file elicitation_integration_test.cpp
 * @brief Integration tests for MCP 2025-06-18 elicitation multi-turn workflow
 *
 * Tests the complete elicitation flow including:
 * - Client capability checking
 * - Request formatting
 * - Response handling logic
 * - Error scenarios
 *
 * Note: Full end-to-end integration tests require a running server and client,
 * which is beyond the scope of unit tests. These tests verify the core logic.
 */

#include "mcp_message.h"
#include "mcp_server.h"

#include <boost/test/unit_test.hpp>

using namespace mcp;

BOOST_AUTO_TEST_SUITE(ElicitationIntegrationTestSuite)

// ============================================================================
// Test 1: Verify server tracks client capabilities
// ============================================================================
BOOST_AUTO_TEST_CASE(ServerTracksClientCapabilities) {
    server::configuration config;
    config.host = "localhost";
    config.port = 9100;
    config.name = "ElicitationTestServer";
    config.version = "1.0";

    server srv(config);
    std::string session_id = "test-session-capabilities";

    // Without client declaring capability, should return false
    BOOST_CHECK(!srv.client_supports_elicitation(session_id));
}

// ============================================================================
// Test 2: Elicitation without client support throws exception
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationWithoutClientSupportThrows) {
    server::configuration config;
    config.host = "localhost";
    config.port = 9101;
    config.name = "ElicitationTestServer";
    config.version = "1.0";

    server srv(config);
    std::string session_id = "test-session-no-support";

    json schema = {{"type", "object"}, {"properties", {{"field", {{"type", "string"}}}}}};

    // Should throw because client doesn't support elicitation
    BOOST_CHECK_THROW(srv.request_elicitation(session_id, "Test message", schema), mcp_exception);
}

// ============================================================================
// Test 3: Elicitation request format validation
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationRequestFormat) {
    // Test that elicitation_params creates correct format
    elicitation_params params;
    params.message = "Please provide your API key";
    params.requested_schema = {{"type", "object"}, {"properties", {{"api_key", {{"type", "string"}}}}}};

    json params_json = params.to_json();

    BOOST_CHECK(params_json.contains("message"));
    BOOST_CHECK(params_json.contains("requestedSchema"));
    BOOST_CHECK_EQUAL(params_json["message"].get<std::string>(), "Please provide your API key");
}

// ============================================================================
// Test 4: Elicitation response format validation
// ============================================================================
BOOST_AUTO_TEST_CASE(ElicitationResponseFormat) {
    // Test accept response
    elicitation_result accept_result;
    accept_result.action = elicitation_action::accept;
    accept_result.content = {{"api_key", "test-key-123"}};

    json accept_json = accept_result.to_json();
    BOOST_CHECK_EQUAL(accept_json["action"].get<std::string>(), "accept");
    BOOST_CHECK(accept_json.contains("content"));

    // Test decline response
    elicitation_result decline_result;
    decline_result.action = elicitation_action::decline;

    json decline_json = decline_result.to_json();
    BOOST_CHECK_EQUAL(decline_json["action"].get<std::string>(), "decline");
    BOOST_CHECK(!decline_json.contains("content"));

    // Test cancel response
    elicitation_result cancel_result;
    cancel_result.action = elicitation_action::cancel;

    json cancel_json = cancel_result.to_json();
    BOOST_CHECK_EQUAL(cancel_json["action"].get<std::string>(), "cancel");
    BOOST_CHECK(!cancel_json.contains("content"));
}

// ============================================================================
// Test 5: Complex schema handling
// ============================================================================
BOOST_AUTO_TEST_CASE(ComplexSchemaHandling) {
    json complex_schema = {{"type", "object"},
                           {"properties",
                            {{"name", {{"type", "string"}, {"minLength", 3}}},
                             {"email", {{"type", "string"}, {"format", "email"}}},
                             {"age", {{"type", "integer"}, {"minimum", 18}}},
                             {"subscribe", {{"type", "boolean"}, {"default", false}}},
                             {"role",
                              {{"type", "string"},
                               {"enum", json::array({"user", "admin"})},
                               {"enumNames", json::array({"User", "Admin"})}}}}},
                           {"required", json::array({"name", "email"})}};

    elicitation_params params;
    params.message = "Please provide your profile";
    params.requested_schema = complex_schema;

    json params_json = params.to_json();

    // Verify all schema components are preserved
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("name"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("email"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("age"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("subscribe"));
    BOOST_CHECK(params_json["requestedSchema"]["properties"].contains("role"));
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["name"]["minLength"].get<int>(), 3);
    BOOST_CHECK_EQUAL(params_json["requestedSchema"]["properties"]["email"]["format"].get<std::string>(), "email");
}

// ============================================================================
// Test 6: Response deserialization with all actions
// ============================================================================
BOOST_AUTO_TEST_CASE(ResponseDeserializationAllActions) {
    // Test accept
    json accept_json = {{"action", "accept"}, {"content", {{"key", "value"}}}};
    elicitation_result accept_result = elicitation_result::from_json(accept_json);
    BOOST_CHECK(accept_result.action == elicitation_action::accept);
    BOOST_CHECK_EQUAL(accept_result.content["key"].get<std::string>(), "value");

    // Test decline
    json decline_json = {{"action", "decline"}};
    elicitation_result decline_result = elicitation_result::from_json(decline_json);
    BOOST_CHECK(decline_result.action == elicitation_action::decline);

    // Test cancel
    json cancel_json = {{"action", "cancel"}};
    elicitation_result cancel_result = elicitation_result::from_json(cancel_json);
    BOOST_CHECK(cancel_result.action == elicitation_action::cancel);
}

// ============================================================================
// Test 7: Invalid action handling
// ============================================================================
BOOST_AUTO_TEST_CASE(InvalidActionHandling) {
    json invalid_json = {{"action", "invalid_action"}};
    BOOST_CHECK_THROW(elicitation_result::from_json(invalid_json), mcp_exception);
}

// ============================================================================
// Test 8: Round-trip serialization
// ============================================================================
BOOST_AUTO_TEST_CASE(RoundTripSerialization) {
    elicitation_params original_params;
    original_params.message = "Test with unicode: 你好 🌍";
    original_params.requested_schema = {
        {"type", "object"}, {"properties", {{"field1", {{"type", "string"}}}, {"field2", {{"type", "number"}}}}}};

    json params_json = original_params.to_json();
    elicitation_params deserialized_params = elicitation_params::from_json(params_json);

    BOOST_CHECK_EQUAL(deserialized_params.message, original_params.message);
    BOOST_CHECK_EQUAL(deserialized_params.requested_schema.dump(), original_params.requested_schema.dump());
}

BOOST_AUTO_TEST_SUITE_END()
