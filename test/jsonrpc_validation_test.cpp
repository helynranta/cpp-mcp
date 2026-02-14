/**
 * @file jsonrpc_validation_test.cpp
 * @brief Tests for JSON-RPC 2.0 message validation
 * 
 * This file contains tests for JSON-RPC validation according to the specification
 * and MCP 2025-03-26 requirements.
 */

#include <gtest/gtest.h>
#include "mcp_jsonrpc_validation.h"
#include "mcp_message.h"

using namespace mcp;
using json = nlohmann::ordered_json;

// Test fixture for JSON-RPC validation tests
class JsonRpcValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup test environment
    }
};

// ===== Request ID Validation Tests =====

TEST_F(JsonRpcValidationTest, ValidRequestIdString) {
    json id = "request-123";
    std::string error;
    EXPECT_TRUE(validate_request_id(id, false, error)) << "String ID should be valid";
}

TEST_F(JsonRpcValidationTest, ValidRequestIdInteger) {
    json id = 42;
    std::string error;
    EXPECT_TRUE(validate_request_id(id, false, error)) << "Integer ID should be valid";
}

TEST_F(JsonRpcValidationTest, ValidRequestIdFloat) {
    json id = 3.14;
    std::string error;
    EXPECT_TRUE(validate_request_id(id, false, error)) << "Float ID should be valid (numbers are allowed)";
}

TEST_F(JsonRpcValidationTest, InvalidRequestIdNull) {
    json id = nullptr;
    std::string error;
    EXPECT_FALSE(validate_request_id(id, false, error)) << "Null ID should be invalid for requests";
    EXPECT_NE(error.find("must not be null"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidRequestIdObject) {
    json id = json::object({{"key", "value"}});
    std::string error;
    EXPECT_FALSE(validate_request_id(id, false, error)) << "Object ID should be invalid";
    EXPECT_NE(error.find("must be a string or number"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidRequestIdArray) {
    json id = json::array({1, 2, 3});
    std::string error;
    EXPECT_FALSE(validate_request_id(id, false, error)) << "Array ID should be invalid";
    EXPECT_NE(error.find("must be a string or number"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, NotificationMustNotHaveId) {
    json id = "some-id";
    std::string error;
    EXPECT_FALSE(validate_request_id(id, true, error)) << "Notifications must not have ID";
    EXPECT_NE(error.find("must not have"), std::string::npos);
}

// ===== Request Message Validation Tests =====

TEST_F(JsonRpcValidationTest, ValidRequestMessage) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "test_method"},
        {"params", {{"key", "value"}}}
    };
    std::string error;
    EXPECT_TRUE(validate_request_message(msg, error)) << "Valid request should pass validation";
}

TEST_F(JsonRpcValidationTest, ValidRequestWithoutParams) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", "req-1"},
        {"method", "test_method"}
    };
    std::string error;
    EXPECT_TRUE(validate_request_message(msg, error)) << "Request without params should be valid";
}

TEST_F(JsonRpcValidationTest, ValidNotificationMessage) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/test"}
    };
    std::string error;
    EXPECT_TRUE(validate_request_message(msg, error)) << "Notification without ID should be valid";
}

TEST_F(JsonRpcValidationTest, InvalidRequestMissingJsonRpc) {
    json msg = {
        {"id", 1},
        {"method", "test_method"}
    };
    std::string error;
    EXPECT_FALSE(validate_request_message(msg, error)) << "Request missing 'jsonrpc' should fail";
    EXPECT_NE(error.find("jsonrpc"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidRequestWrongJsonRpcVersion) {
    json msg = {
        {"jsonrpc", "1.0"},
        {"id", 1},
        {"method", "test_method"}
    };
    std::string error;
    EXPECT_FALSE(validate_request_message(msg, error)) << "Wrong jsonrpc version should fail";
    EXPECT_NE(error.find("2.0"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidRequestMissingMethod) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1}
    };
    std::string error;
    EXPECT_FALSE(validate_request_message(msg, error)) << "Request missing method should fail";
    EXPECT_NE(error.find("method"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidRequestNullId) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", nullptr},
        {"method", "test_method"}
    };
    std::string error;
    EXPECT_FALSE(validate_request_message(msg, error)) << "Request with null ID should fail";
    EXPECT_NE(error.find("must not be null"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidRequestNotObject) {
    json msg = json::array({1, 2, 3});
    std::string error;
    EXPECT_FALSE(validate_request_message(msg, error)) << "Array should not be valid request";
    EXPECT_NE(error.find("must be an object"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidParamsNotStructured) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "test_method"},
        {"params", "invalid"}  // String is not allowed
    };
    std::string error;
    EXPECT_FALSE(validate_request_message(msg, error)) << "Params must be object or array";
    EXPECT_NE(error.find("params"), std::string::npos);
}

// ===== Response Message Validation Tests =====

TEST_F(JsonRpcValidationTest, ValidSuccessResponse) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"result", {{"key", "value"}}}
    };
    std::string error;
    EXPECT_TRUE(validate_response_message(msg, error)) << "Valid success response should pass";
}

TEST_F(JsonRpcValidationTest, ValidErrorResponse) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", -32600},
            {"message", "Invalid Request"}
        }}
    };
    std::string error;
    EXPECT_TRUE(validate_response_message(msg, error)) << "Valid error response should pass";
}

TEST_F(JsonRpcValidationTest, InvalidResponseBothResultAndError) {
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
    EXPECT_FALSE(validate_response_message(msg, error)) << "Response with both result and error should fail";
    EXPECT_NE(error.find("both"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidResponseNeitherResultNorError) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1}
    };
    std::string error;
    EXPECT_FALSE(validate_response_message(msg, error)) << "Response without result or error should fail";
    EXPECT_NE(error.find("either"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidResponseMissingId) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"result", {{"key", "value"}}}
    };
    std::string error;
    EXPECT_FALSE(validate_response_message(msg, error)) << "Response missing ID should fail";
    EXPECT_NE(error.find("id"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidErrorMissingCode) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"message", "Invalid Request"}
        }}
    };
    std::string error;
    EXPECT_FALSE(validate_response_message(msg, error)) << "Error without code should fail";
    EXPECT_NE(error.find("code"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidErrorMissingMessage) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", -32600}
        }}
    };
    std::string error;
    EXPECT_FALSE(validate_response_message(msg, error)) << "Error without message should fail";
    EXPECT_NE(error.find("message"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidErrorCodeNotInteger) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", "not-an-int"},
            {"message", "Invalid Request"}
        }}
    };
    std::string error;
    EXPECT_FALSE(validate_response_message(msg, error)) << "Error code must be integer";
    EXPECT_NE(error.find("integer"), std::string::npos);
}

TEST_F(JsonRpcValidationTest, InvalidErrorMessageNotString) {
    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"error", {
            {"code", -32600},
            {"message", 123}
        }}
    };
    std::string error;
    EXPECT_FALSE(validate_response_message(msg, error)) << "Error message must be string";
    EXPECT_NE(error.find("string"), std::string::npos);
}

// ===== Request ID Tracker Tests =====

TEST_F(JsonRpcValidationTest, TrackerAcceptsUniqueIds) {
    request_id_tracker tracker;
    
    json id1 = 1;
    json id2 = 2;
    
    EXPECT_TRUE(tracker.add_request_id("session1", id1));
    EXPECT_TRUE(tracker.add_request_id("session1", id2));
}

TEST_F(JsonRpcValidationTest, TrackerRejectsDuplicateIds) {
    request_id_tracker tracker;
    
    json id = 1;
    
    EXPECT_TRUE(tracker.add_request_id("session1", id));
    EXPECT_FALSE(tracker.add_request_id("session1", id)) << "Duplicate ID should be rejected";
}

TEST_F(JsonRpcValidationTest, TrackerAllowsSameIdDifferentSessions) {
    request_id_tracker tracker;
    
    json id = 1;
    
    EXPECT_TRUE(tracker.add_request_id("session1", id));
    EXPECT_TRUE(tracker.add_request_id("session2", id)) << "Same ID in different sessions should be allowed";
}

TEST_F(JsonRpcValidationTest, TrackerRemovesIds) {
    request_id_tracker tracker;
    
    json id = 1;
    
    EXPECT_TRUE(tracker.add_request_id("session1", id));
    tracker.remove_request_id("session1", id);
    EXPECT_TRUE(tracker.add_request_id("session1", id)) << "Removed ID should be reusable";
}

TEST_F(JsonRpcValidationTest, TrackerClearsSession) {
    request_id_tracker tracker;
    
    json id1 = 1;
    json id2 = 2;
    
    EXPECT_TRUE(tracker.add_request_id("session1", id1));
    EXPECT_TRUE(tracker.add_request_id("session1", id2));
    
    tracker.clear_session("session1");
    
    EXPECT_TRUE(tracker.add_request_id("session1", id1)) << "After clearing session, IDs should be reusable";
    EXPECT_TRUE(tracker.add_request_id("session1", id2)) << "After clearing session, IDs should be reusable";
}

TEST_F(JsonRpcValidationTest, TrackerHandlesStringIds) {
    request_id_tracker tracker;
    
    json id1 = "request-1";
    json id2 = "request-2";
    
    EXPECT_TRUE(tracker.add_request_id("session1", id1));
    EXPECT_TRUE(tracker.add_request_id("session1", id2));
    EXPECT_FALSE(tracker.add_request_id("session1", id1)) << "Duplicate string ID should be rejected";
}

// ===== MCP Request/Notification Structure Tests =====

TEST_F(JsonRpcValidationTest, McpRequestStructure) {
    request req = request::create("test_method", {{"key", "value"}});
    json req_json = req.to_json();
    
    std::string error;
    EXPECT_TRUE(validate_request_message(req_json, error)) << "MCP request should be valid JSON-RPC";
    EXPECT_FALSE(req.is_notification()) << "Request should not be a notification";
    EXPECT_TRUE(req_json.contains("id")) << "Request should have ID";
}

TEST_F(JsonRpcValidationTest, McpNotificationStructure) {
    request notif = request::create_notification("test_notif", {{"key", "value"}});
    json notif_json = notif.to_json();
    
    std::string error;
    EXPECT_TRUE(validate_request_message(notif_json, error)) << "MCP notification should be valid JSON-RPC";
    EXPECT_TRUE(notif.is_notification()) << "Notification should be identified as such";
    EXPECT_FALSE(notif_json.contains("id")) << "Notification should NOT have ID field";
}

TEST_F(JsonRpcValidationTest, McpResponseStructure) {
    response res = response::create_success(1, {{"key", "value"}});
    json res_json = res.to_json();
    
    std::string error;
    EXPECT_TRUE(validate_response_message(res_json, error)) << "MCP response should be valid JSON-RPC";
    EXPECT_FALSE(res.is_error()) << "Success response should not be error";
}

TEST_F(JsonRpcValidationTest, McpErrorResponseStructure) {
    response res = response::create_error(1, error_code::invalid_params, "Invalid parameters");
    json res_json = res.to_json();
    
    std::string error;
    EXPECT_TRUE(validate_response_message(res_json, error)) << "MCP error response should be valid JSON-RPC";
    EXPECT_TRUE(res.is_error()) << "Error response should be identified as error";
}
