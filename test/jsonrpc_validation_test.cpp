/**
 * @file jsonrpc_validation_test.cpp
 * @brief Tests for JSON-RPC 2.0 message validation
 * 
 * This file contains tests for JSON-RPC validation according to the specification
 * and MCP 2025-03-26 requirements.
 */

#include <boost/test/unit_test.hpp>
#include "mcp_jsonrpc_validation.h"
#include "mcp_message.h"

using namespace mcp;
using json = nlohmann::ordered_json;

// Test fixture for JSON-RPC validation tests
struct JsonRpcValidationTest {
    JsonRpcValidationTest() {
        // Setup test environment
    }

    ~JsonRpcValidationTest() {
        // Cleanup test environment
    }
};

// ===== Request ID Validation Tests =====

BOOST_FIXTURE_TEST_SUITE(JsonRpcValidationTest, JsonRpcValidationTest)

BOOST_AUTO_TEST_CASE(ValidRequestIdString) {
    json id = "request-123";
    std::string error;
    BOOST_CHECK(validate_request_id(id, false, error));
}

BOOST_AUTO_TEST_CASE(ValidRequestIdInteger) {
    json id = 42;
    std::string error;
    BOOST_CHECK(validate_request_id(id, false, error));
}

BOOST_AUTO_TEST_CASE(ValidRequestIdFloat) {
    json id = 3.14;
    std::string error;
    BOOST_CHECK(validate_request_id(id, false, error));
}

BOOST_AUTO_TEST_CASE(InvalidRequestIdNull) {
    json id = nullptr;
    std::string error;
    BOOST_CHECK(!validate_request_id(id, false, error));
    BOOST_CHECK_NE(error.find("must not be null"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidRequestIdObject) {
    json id = json::object({{"key", "value"}});
    std::string error;
    BOOST_CHECK(!validate_request_id(id, false, error));
    BOOST_CHECK_NE(error.find("must be a string or number"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidRequestIdArray) {
    json id = json::array({1, 2, 3});
    std::string error;
    BOOST_CHECK(!validate_request_id(id, false, error));
    BOOST_CHECK_NE(error.find("must be a string or number"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(NotificationMustNotHaveId) {
    json id = "some-id";
    std::string error;
    BOOST_CHECK(!validate_request_id(id, true, error));
    BOOST_CHECK_NE(error.find("must not have"), std::string::npos);
}

// ===== Request Message Validation Tests =====

BOOST_AUTO_TEST_CASE(ValidRequestMessage) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "test_method"},
        {"params", {{"key", "value"}}}
    };
    std::string error;
    BOOST_CHECK(validate_request_message(msg, error));
}

BOOST_AUTO_TEST_CASE(ValidRequestWithoutParams) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", "req-1"},
        {"method", "test_method"}
    };
    std::string error;
    BOOST_CHECK(validate_request_message(msg, error));
}

BOOST_AUTO_TEST_CASE(ValidNotificationMessage) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/test"}
    };
    std::string error;
    BOOST_CHECK(validate_request_message(msg, error));
}

BOOST_AUTO_TEST_CASE(InvalidRequestMissingJsonRpc) {
    json msg = {
        {"id", 1},
        {"method", "test_method"}
    };
    std::string error;
    BOOST_CHECK(!validate_request_message(msg, error));
    BOOST_CHECK_NE(error.find("jsonrpc"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidRequestWrongJsonRpcVersion) {
    json msg = {
        {"jsonrpc", "1.0"},
        {"id", 1},
        {"method", "test_method"}
    };
    std::string error;
    BOOST_CHECK(!validate_request_message(msg, error));
    BOOST_CHECK_NE(error.find("2.0"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidRequestMissingMethod) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1}
    };
    std::string error;
    BOOST_CHECK(!validate_request_message(msg, error));
    BOOST_CHECK_NE(error.find("method"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidRequestNullId) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", nullptr},
        {"method", "test_method"}
    };
    std::string error;
    BOOST_CHECK(!validate_request_message(msg, error));
    BOOST_CHECK_NE(error.find("must not be null"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidRequestNotObject) {
    json msg = json::array({1, 2, 3});
    std::string error;
    BOOST_CHECK(!validate_request_message(msg, error));
    BOOST_CHECK_NE(error.find("must be an object"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidParamsNotStructured) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "test_method"},
        {"params", "invalid"}  // String is not allowed
    };
    std::string error;
    BOOST_CHECK(!validate_request_message(msg, error));
    BOOST_CHECK_NE(error.find("params"), std::string::npos);
}

// ===== Response Message Validation Tests =====

BOOST_AUTO_TEST_CASE(ValidSuccessResponse) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"result", {{"key", "value"}}}
    };
    std::string error;
    BOOST_CHECK(validate_response_message(msg, error));
}

BOOST_AUTO_TEST_CASE(ValidErrorResponse) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", -32600},
            {"message", "Invalid Request"}
        }}
    };
    std::string error;
    BOOST_CHECK(validate_response_message(msg, error));
}

BOOST_AUTO_TEST_CASE(InvalidResponseBothResultAndError) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"result", {{"key", "value"}}},
        {"error", {
            {"code", -32600},
            {"message", "Invalid Request"}
        }}
    };
    std::string error;
    BOOST_CHECK(!validate_response_message(msg, error));
    BOOST_CHECK_NE(error.find("both"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidResponseNeitherResultNorError) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1}
    };
    std::string error;
    BOOST_CHECK(!validate_response_message(msg, error));
    BOOST_CHECK_NE(error.find("either"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidResponseMissingId) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"result", {{"key", "value"}}}
    };
    std::string error;
    BOOST_CHECK(!validate_response_message(msg, error));
    BOOST_CHECK_NE(error.find("id"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidErrorMissingCode) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"message", "Invalid Request"}
        }}
    };
    std::string error;
    BOOST_CHECK(!validate_response_message(msg, error));
    BOOST_CHECK_NE(error.find("code"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidErrorMissingMessage) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", -32600}
        }}
    };
    std::string error;
    BOOST_CHECK(!validate_response_message(msg, error));
    BOOST_CHECK_NE(error.find("message"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidErrorCodeNotInteger) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", "not-an-int"},
            {"message", "Invalid Request"}
        }}
    };
    std::string error;
    BOOST_CHECK(!validate_response_message(msg, error));
    BOOST_CHECK_NE(error.find("integer"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(InvalidErrorMessageNotString) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", -32600},
            {"message", 123}
        }}
    };
    std::string error;
    BOOST_CHECK(!validate_response_message(msg, error));
    BOOST_CHECK_NE(error.find("string"), std::string::npos);
}

// ===== Request ID Tracker Tests =====

BOOST_AUTO_TEST_CASE(TrackerAcceptsUniqueIds) {
    request_id_tracker tracker;
    
    json id1 = 1;
    json id2 = 2;
    
    BOOST_CHECK(tracker.add_request_id("session1", id1));
    BOOST_CHECK(tracker.add_request_id("session1", id2));
}

BOOST_AUTO_TEST_CASE(TrackerRejectsDuplicateIds) {
    request_id_tracker tracker;
    
    json id = 1;
    
    BOOST_CHECK(tracker.add_request_id("session1", id));
    BOOST_CHECK(!tracker.add_request_id("session1", id));
}

BOOST_AUTO_TEST_CASE(TrackerAllowsSameIdDifferentSessions) {
    request_id_tracker tracker;
    
    json id = 1;
    
    BOOST_CHECK(tracker.add_request_id("session1", id));
    BOOST_CHECK(tracker.add_request_id("session2", id));
}

BOOST_AUTO_TEST_CASE(TrackerRemovesIds) {
    request_id_tracker tracker;
    
    json id = 1;
    
    BOOST_CHECK(tracker.add_request_id("session1", id));
    tracker.remove_request_id("session1", id);
    BOOST_CHECK(tracker.add_request_id("session1", id));
}

BOOST_AUTO_TEST_CASE(TrackerClearsSession) {
    request_id_tracker tracker;
    
    json id1 = 1;
    json id2 = 2;
    
    BOOST_CHECK(tracker.add_request_id("session1", id1));
    BOOST_CHECK(tracker.add_request_id("session1", id2));
    
    tracker.clear_session("session1");
    
    BOOST_CHECK(tracker.add_request_id("session1", id1));
    BOOST_CHECK(tracker.add_request_id("session1", id2));
}

BOOST_AUTO_TEST_CASE(TrackerHandlesStringIds) {
    request_id_tracker tracker;
    
    json id1 = "request-1";
    json id2 = "request-2";
    
    BOOST_CHECK(tracker.add_request_id("session1", id1));
    BOOST_CHECK(tracker.add_request_id("session1", id2));
    BOOST_CHECK(!tracker.add_request_id("session1", id1));
}

// ===== MCP Request/Notification Structure Tests =====

BOOST_AUTO_TEST_CASE(McpRequestStructure) {
    request req = request::create("test_method", {{"key", "value"}});
    json req_json = req.to_json();
    
    std::string error;
    BOOST_CHECK(validate_request_message(req_json, error));
    BOOST_CHECK(!req.is_notification());
    BOOST_CHECK(req_json.contains("id"));
}

BOOST_AUTO_TEST_CASE(McpNotificationStructure) {
    request notif = request::create_notification("test_notif", {{"key", "value"}});
    json notif_json = notif.to_json();
    
    std::string error;
    BOOST_CHECK(validate_request_message(notif_json, error));
    BOOST_CHECK(notif.is_notification());
    BOOST_CHECK(!notif_json.contains("id"));
}

BOOST_AUTO_TEST_CASE(McpResponseStructure) {
    response res = response::create_success(1, {{"key", "value"}});
    json res_json = res.to_json();
    
    std::string error;
    BOOST_CHECK(validate_response_message(res_json, error));
    BOOST_CHECK(!res.is_error());
}

BOOST_AUTO_TEST_CASE(McpErrorResponseStructure) {
    response res = response::create_error(1, error_code::invalid_params, "Invalid parameters");
    json res_json = res.to_json();
    
    std::string error;
    BOOST_CHECK(validate_response_message(res_json, error));
    BOOST_CHECK(res.is_error());
}

BOOST_AUTO_TEST_SUITE_END()
