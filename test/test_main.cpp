/**
 * @file test_main.cpp
 * @brief Common test main for all Boost.Test executables
 *
 * This file provides a shared test main that is linked into each test executable.
 * It defines the BOOST_TEST_MODULE for all test executables.
 */

#define BOOST_TEST_MODULE MCP_Tests
#include <boost/test/unit_test.hpp>

// This file is intentionally minimal - it just provides the test main
// All test cases are defined in separate test files
