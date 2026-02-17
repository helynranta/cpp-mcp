/**
 * @file error_result_compliance_test.cpp
 * @brief Tests for strict MCP error/result field compliance
 *
 * This file contains tests to ensure error responses conform to MCP specification:
 * - Errors must have only 'code' (integer) and 'message' (string) fields
 * - Success responses must never include both 'error' and 'result'
 * - Error responses must never include 'result'
 * - Success responses must never include 'error'
 */

#include "mcp_jsonrpc_validation.h"
#include "mcp_message.h"

#include <boost/test/unit_test.hpp>

using namespace mcp;
using json = nlohmann::ordered_json;

// Test fixture for error/result compliance tests
struct ErrorResultComplianceTest {
    ErrorResultComplianceTest() {
        // Setup test environment
    }

    ~ErrorResultComplianceTest() {
        // Cleanup test environment
    }
};

BOOST_FIXTURE_TEST_SUITE(ErrorResultComplianceTestSuite, ErrorResultComplianceTest)

// ===== Error Structure Tests =====

BOOST_AUTO_TEST_CASE(ErrorObjectHasOnlyCodeAndMessage) {
    // Create error response using the API
    response res = response::create_error(1, error_code::invalid_params, "Invalid parameters");
    json res_json = res.to_json();

    // Verify error object exists
    BOOST_REQUIRE(res_json.contains("error"));
    const auto& error = res_json["error"];

    // Error must be an object
    BOOST_CHECK(error.is_object());

    // Error must have exactly 2 fields: code and message
    BOOST_CHECK_EQUAL(error.size(), 2);

    // Error must have code field (integer)
    BOOST_REQUIRE(error.contains("code"));
    BOOST_CHECK(error["code"].is_number_integer());
    BOOST_CHECK_EQUAL(error["code"].get<int>(), static_cast<int>(error_code::invalid_params));

    // Error must have message field (string)
    BOOST_REQUIRE(error.contains("message"));
    BOOST_CHECK(error["message"].is_string());
    BOOST_CHECK_EQUAL(error["message"].get<std::string>(), "Invalid parameters");

    // Error must NOT have any other fields (especially 'data')
    BOOST_CHECK(!error.contains("data"));
}

BOOST_AUTO_TEST_CASE(ErrorResponseNeverIncludesResult) {
    // Create error response
    response res = response::create_error(1, error_code::method_not_found, "Method not found");
    json res_json = res.to_json();

    // Verify it has error
    BOOST_CHECK(res_json.contains("error"));

    // Verify it does NOT have result
    BOOST_CHECK(!res_json.contains("result"));
}

BOOST_AUTO_TEST_CASE(SuccessResponseNeverIncludesError) {
    // Create success response
    response res = response::create_success(1, {{"key", "value"}});
    json res_json = res.to_json();

    // Verify it has result
    BOOST_CHECK(res_json.contains("result"));

    // Verify it does NOT have error
    BOOST_CHECK(!res_json.contains("error"));
}

BOOST_AUTO_TEST_CASE(MultipleErrorTypesAllHaveCorrectStructure) {
    // Test various error codes
    std::vector<error_code> codes = {error_code::parse_error, error_code::invalid_request, error_code::method_not_found,
                                     error_code::invalid_params, error_code::internal_error};

    for (auto code : codes) {
        response res = response::create_error(1, code, "Test error");
        json res_json = res.to_json();

        const auto& error = res_json["error"];

        // Each error must have exactly 2 fields
        BOOST_CHECK_EQUAL(error.size(), 2);
        BOOST_CHECK(error.contains("code"));
        BOOST_CHECK(error.contains("message"));
        BOOST_CHECK(!error.contains("data"));
    }
}

// ===== Validation Tests for Invalid Error Structures =====

BOOST_AUTO_TEST_CASE(ValidateRejectsErrorWithExtraFields) {
    // Manually construct an error response with extra fields (data field)
    json res_json = {{"jsonrpc", "2.0"},
                     {"id", 1},
                     {"error", {{"code", -32600}, {"message", "Invalid Request"}, {"data", {{"extra", "field"}}}}}};

    std::string error_msg;
    // Strict validation must reject errors with extra fields
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
    BOOST_CHECK_NE(error_msg.find("exactly"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(ValidateRejectsResponseWithBothErrorAndResult) {
    // Response with both error and result
    json res_json = {{"jsonrpc", "2.0"},
                     {"id", 1},
                     {"result", {{"key", "value"}}},
                     {"error", {{"code", -32600}, {"message", "Invalid Request"}}}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
    BOOST_CHECK_NE(error_msg.find("both"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(ErrorCodeMustBeInteger) {
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", "not-an-integer"}, {"message", "Error"}}}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
    BOOST_CHECK_NE(error_msg.find("integer"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(ErrorMessageMustBeString) {
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", -32600}, {"message", 123}}}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
    BOOST_CHECK_NE(error_msg.find("string"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(ErrorMustHaveCodeField) {
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"message", "Error without code"}}}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
    BOOST_CHECK_NE(error_msg.find("code"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(ErrorMustHaveMessageField) {
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", -32600}}}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
    BOOST_CHECK_NE(error_msg.find("message"), std::string::npos);
}

// ===== Edge Cases and Invalid Inputs =====

BOOST_AUTO_TEST_CASE(EmptyErrorObject) {
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", json::object()}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
}

BOOST_AUTO_TEST_CASE(ErrorAsNonObject) {
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", "error string"}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
}

BOOST_AUTO_TEST_CASE(ErrorCodeWithFloatValue) {
    // Error code should be integer, not float
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", -32600.5}, {"message", "Error"}}}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
    BOOST_CHECK_NE(error_msg.find("integer"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(NullErrorMessage) {
    json res_json = {{"jsonrpc", "2.0"}, {"id", 1}, {"error", {{"code", -32600}, {"message", nullptr}}}};

    std::string error_msg;
    BOOST_CHECK(!validate_response_message(res_json, error_msg));
}

// ===== Response API Correctness =====

BOOST_AUTO_TEST_CASE(CreateErrorProducesValidResponse) {
    response res = response::create_error("test-id", error_code::internal_error, "Internal server error");
    json res_json = res.to_json();

    std::string error_msg;
    BOOST_CHECK(validate_response_message(res_json, error_msg));
    BOOST_CHECK(res.is_error());
}

BOOST_AUTO_TEST_CASE(CreateSuccessProducesValidResponse) {
    response res = response::create_success("test-id", {{"data", "value"}});
    json res_json = res.to_json();

    std::string error_msg;
    BOOST_CHECK(validate_response_message(res_json, error_msg));
    BOOST_CHECK(!res.is_error());
}

BOOST_AUTO_TEST_CASE(ErrorResponseIsErrorReturnsTrue) {
    response res = response::create_error(1, error_code::parse_error, "Parse error");
    BOOST_CHECK(res.is_error());
}

BOOST_AUTO_TEST_CASE(SuccessResponseIsErrorReturnsFalse) {
    response res = response::create_success(1, {{"result", "data"}});
    BOOST_CHECK(!res.is_error());
}

BOOST_AUTO_TEST_SUITE_END()
