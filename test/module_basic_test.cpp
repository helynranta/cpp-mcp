/**
 * @file module_basic_test.cpp
 * @brief Basic tests for C++ module compilation and functionality
 *
 * This test verifies that:
 * 1. C++ modules compile successfully
 * 2. Module imports work correctly
 * 3. Basic module functionality is accessible
 */

// Include nlohmann/json before importing modules that use it
#include <nlohmann/json.hpp>

// Import standard library module (requires CMake 3.28+ with experimental support)
import std;

// Import C++ modules
import mcp.core;
import mcp.logger;

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(ModuleBasicTestSuite)

/**
 * Test that mcp.core module imports and basic types are accessible
 */
BOOST_AUTO_TEST_CASE(CoreModuleImports) {
    // Verify we can use types from mcp.core module
    mcp::request req = mcp::request::create("test_method", mcp::json::object());

    BOOST_CHECK_EQUAL(req.jsonrpc, "2.0");
    BOOST_CHECK_EQUAL(req.method, "test_method");
    BOOST_CHECK(!req.is_notification());
}

/**
 * Test that mcp.logger module imports and logger is accessible
 */
BOOST_AUTO_TEST_CASE(LoggerModuleImports) {
    // Verify we can use logger from mcp.logger module
    mcp::logger& log = mcp::logger::instance();

    // Set log level (should not throw)
    log.set_level(mcp::log_level::error);

    // Log messages (should not throw)
    mcp::log_error("Test error message from module");
    mcp::log_info("Test info message from module");
}

/**
 * Test that MCP version constant is accessible
 */
BOOST_AUTO_TEST_CASE(VersionConstantAccessible) {
    std::string version = mcp::MCP_VERSION;
    BOOST_CHECK_EQUAL(version, "2025-11-25");
}

/**
 * Test request creation and JSON conversion
 */
BOOST_AUTO_TEST_CASE(RequestCreationAndConversion) {
    // Create request
    mcp::json params = {{"param1", "value1"}, {"param2", 42}};
    mcp::request req = mcp::request::create("test_method", params);

    // Convert to JSON
    mcp::json j = req.to_json();

    BOOST_CHECK_EQUAL(j["jsonrpc"].get<std::string>(), "2.0");
    BOOST_CHECK_EQUAL(j["method"].get<std::string>(), "test_method");
    BOOST_CHECK(j.contains("id"));
    BOOST_CHECK(j.contains("params"));
}

/**
 * Test response creation
 */
BOOST_AUTO_TEST_CASE(ResponseCreation) {
    mcp::json result = {{"status", "success"}};
    mcp::response res = mcp::response::create_success(1, result);

    BOOST_CHECK_EQUAL(res.jsonrpc, "2.0");
    BOOST_CHECK(!res.is_error());

    // Create error response
    mcp::response err = mcp::response::create_error(2, mcp::error_code::invalid_params, "Invalid parameters");
    BOOST_CHECK(err.is_error());
}

/**
 * Test mcp_exception from module
 */
BOOST_AUTO_TEST_CASE(ExceptionHandling) {
    try {
        throw mcp::mcp_exception(mcp::error_code::invalid_request, "Test exception");
    } catch (const mcp::mcp_exception& e) {
        BOOST_CHECK_EQUAL(e.code(), mcp::error_code::invalid_request);
        BOOST_CHECK_EQUAL(std::string(e.what()), "Test exception");
    }
}

BOOST_AUTO_TEST_SUITE_END()
