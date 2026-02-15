/**
 * @file session_management_test.cpp
 * @brief Test session management functionality
 * 
 * Tests session ID generation, session state storage, and session cleanup.
 */

#include <boost/test/unit_test.hpp>
#include "mcp_server.h"
#include "mcp_message.h"
#include <set>
#include <regex>

using namespace mcp;
using json = nlohmann::ordered_json;

// Simple fixture for session management tests (no server needed for basic tests)
struct SessionManagementFixture {
    SessionManagementFixture() {
        // Create server configuration
        server::configuration config;
        config.host = "localhost";
        config.port = 18888; // Don't start the server, just create it
        config.name = "SessionTestServer";
        config.version = "1.0.0";
        server_ = std::make_unique<server>(config);
    }
    
    ~SessionManagementFixture() {
        // No cleanup needed as server is never started
    }
    
    std::unique_ptr<server> server_;
};

// Helper to access private generate_session_id (we'll make it public or add a test API)
// For now, we'll test indirectly through the public API

BOOST_FIXTURE_TEST_SUITE(SessionManagementTest, SessionManagementFixture)

// Test: Session state storage - set and get
BOOST_AUTO_TEST_CASE(SessionStateStorageSetAndGet) {
    // Create a session ID
    std::string session_id = "test-session-001";
    
    // Store some state data
    json state_data = {
        {"user", "alice"},
        {"counter", 42},
        {"active", true}
    };
    
    // Set session state
    server_->set_session_state(session_id, state_data);
    
    // Retrieve the state data
    json retrieved_state = server_->get_session_state(session_id);
    
    // Verify the data matches
    BOOST_CHECK_EQUAL(retrieved_state["user"], "alice");
    BOOST_CHECK_EQUAL(retrieved_state["counter"], 42);
    BOOST_CHECK_EQUAL(retrieved_state["active"], true);
}

// Test: Session state returns empty for non-existent session
BOOST_AUTO_TEST_CASE(SessionStateReturnsEmptyForNonExistentSession) {
    std::string non_existent_session = "does-not-exist";
    
    // Get state for non-existent session
    json state = server_->get_session_state(non_existent_session);
    
    // Should return empty or null JSON
    BOOST_CHECK(state.is_null() || state.empty());
}

// Test: Session state can be updated
BOOST_AUTO_TEST_CASE(SessionStateCanBeUpdated) {
    std::string session_id = "test-session-002";
    
    // Set initial state
    json initial_state = {{"counter", 1}};
    server_->set_session_state(session_id, initial_state);
    
    // Update state
    json updated_state = {{"counter", 2}, {"new_field", "value"}};
    server_->set_session_state(session_id, updated_state);
    
    // Retrieve and verify
    json retrieved = server_->get_session_state(session_id);
    BOOST_CHECK_EQUAL(retrieved["counter"], 2);
    BOOST_CHECK_EQUAL(retrieved["new_field"], "value");
}

// Test: Multiple sessions can coexist with independent state
BOOST_AUTO_TEST_CASE(MultipleSessionsWithIndependentState) {
    std::string session_id1 = "session-alice";
    std::string session_id2 = "session-bob";
    
    // Store different state for each session
    server_->set_session_state(session_id1, {{"user", "alice"}});
    server_->set_session_state(session_id2, {{"user", "bob"}});
    
    // Verify each session has its own state
    BOOST_CHECK_EQUAL(server_->get_session_state(session_id1)["user"], "alice");
    BOOST_CHECK_EQUAL(server_->get_session_state(session_id2)["user"], "bob");
}

// Test: Session state is cleared when session is closed
BOOST_AUTO_TEST_CASE(SessionStateIsClearedOnClose) {
    std::string session_id = "test-session-003";
    
    // Store some state
    server_->set_session_state(session_id, {{"data", "value"}});
    
    // Verify state exists
    BOOST_CHECK(!server_->get_session_state(session_id).is_null());
    
    // Clear session state (simulate session close)
    server_->clear_session_state(session_id);
    
    // Verify state is cleared
    json state = server_->get_session_state(session_id);
    BOOST_CHECK(state.is_null() || state.empty());
}

BOOST_AUTO_TEST_SUITE_END()
