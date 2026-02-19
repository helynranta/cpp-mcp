# Testing Infrastructure Implementation Plan

## Overview
This document outlines the testing infrastructure and CI/CD pipeline for the cpp-mcp project.

## Test Parallelization (Completed ✅)

**Status:** Implemented - Tests now run in parallel for 3x performance improvement

### Architecture
The test infrastructure has been redesigned to support parallel execution:

1. **Separate Test Executables**: Each test file is compiled into its own executable (21 total)
2. **Shared Test Main**: `test/test_main.cpp` provides the BOOST_TEST_MODULE definition
3. **Parallel CTest Execution**: CMake test presets configured with `"jobs": 0` for automatic parallelization

### Performance Results
- **Before**: 5-6 minutes (sequential execution)
- **After**: ~2 minutes (parallel execution)
- **Speedup**: ~3x faster
- **Test Count**: 21 separate test executables, 261 total test cases

### Implementation Details

**Files Modified:**
- `test/CMakeLists.txt` - Creates individual test executables using `add_mcp_test()` helper
- `test/test_main.cpp` - New file providing shared BOOST_TEST_MODULE
- `test/mcp_test.cpp` - Removed BOOST_TEST_MODULE (now in test_main.cpp)
- `CMakePresets.json` - Added `"jobs": 0` to all test presets

**Test Executables:**
1. mcp_test
2. jsonrpc_validation_test
3. lifecycle_compliance_test
4. streamable_http_transport_test
5. http_security_test
6. tool_safety_test
7. boost_integration_test
8. beast_sse_proof_of_concept
9. http_abstraction_test
10. beast_adapter_test
11. sse_client_beast_test
12. session_management_test
13. streamable_http_client_test
14. batch_rejection_test
15. protocol_version_header_test
16. structured_tool_output_test
17. elicitation_test
18. elicitation_integration_test
19. completion_test
20. error_result_compliance_test
21. structured_tool_handler_test

### Adding New Tests

When adding new test files, follow this pattern:

1. **Create test file** (e.g., `test/my_new_test.cpp`):
   ```cpp
   // DO NOT define BOOST_TEST_MODULE - it's in test_main.cpp
   #include <boost/test/unit_test.hpp>
   
   BOOST_AUTO_TEST_SUITE(MyNewTestSuite)
   
   BOOST_AUTO_TEST_CASE(MyTestCase) {
       // Your test code
   }
   
   BOOST_AUTO_TEST_SUITE_END()
   ```

2. **Register in CMakeLists.txt**:
   ```cmake
   add_mcp_test(my_new_test my_new_test.cpp)
   ```

3. **Add to run_tests target dependencies** (optional, for convenience target)

**Important:** Never define `BOOST_TEST_MODULE` in test files - it's provided by `test_main.cpp`.

## Completed Work ✅

1. **vcpkg Integration**
   - Created `vcpkg.json` manifest (dependencies managed via vcpkg)
   - Created `vcpkg-configuration.json` for vcpkg registry configuration
   - Updated `.gitignore` to exclude `vcpkg_installed/`
   - Verified vcpkg integration works with CMake

2. **GoogleTest Framework**
   - Existing GoogleTest tests in `test/mcp_test.cpp`
   - Updated `test/CMakeLists.txt` to use only GoogleTest (Catch2 removed)
   - Tests include: message format, lifecycle, version control, ping, tool functionality

3. **CI/CD Pipeline**
   - Created `.github/workflows/test.yml`
   - Supports Linux (Ubuntu) and Windows
   - Uses vcpkg for dependency management with caching enabled
   - Runs GoogleTest tests automatically
   - Triggers only on main branch (PRs and pushes)

4. **Code Cleanup**
   - Fixed SSE-related compilation errors in `src/mcp_server.cpp`
   - Cleaned up `examples/agent_example.cpp`

## Remaining Tasks - GitHub Issues to Create

### Issue #1: Update GoogleTest Tests (Remove SSE Client)
**Priority:** HIGH  
**Estimated Effort:** 2-3 hours  
**Assignable to:** Single agent

**Description:**
The existing GoogleTest suite (`test/mcp_test.cpp`) still references the removed `sse_client`. Update tests to use `stdio_client` or remove SSE-specific tests.

**Tasks:**
- [ ] Remove `#include "mcp_sse_client.h"` from `test/mcp_test.cpp`
- [ ] Replace all `sse_client` usage with `stdio_client`
- [ ] Remove or update SSE-specific test cases
- [ ] Ensure all tests compile and pass
- [ ] Update test documentation

**Files to modify:**
- `test/mcp_test.cpp`

**Acceptance criteria:**
- `mcp_tests` compiles without errors
- All GoogleTest tests pass when run
- No references to removed SSE components

---

### Issue #2: Complete Server SSE Cleanup
**Priority:** MEDIUM  
**Estimated Effort:** 1-2 hours  
**Assignable to:** Single agent

**Description:**
Remove remaining SSE-related code, particularly the `event_dispatcher` class which is no longer needed.

**Tasks:**
- [ ] Remove `event_dispatcher` class from `include/mcp_server.h`
- [ ] Remove any remaining SSE-related helper functions
- [ ] Verify server compiles without errors
- [ ] Test server startup/shutdown
- [ ] Update comments to remove SSE references

**Files to modify:**
- `include/mcp_server.h`
- `src/mcp_server.cpp`

**Acceptance criteria:**
- Server compiles without errors
- No unused SSE-related code remains
- Server runs correctly without SSE components

---

### Issue #3: Validate and Fix CI/CD Pipeline
**Priority:** HIGH  
**Estimated Effort:** 1-2 hours  
**Assignable to:** Single agent

**Description:**
Test the GitHub Actions workflow and fix any platform-specific issues. Ensure tests run successfully on all platforms.

**Tasks:**
- [ ] Trigger workflow manually to test
- [ ] Fix any Linux build/test failures
- [ ] Fix any Windows build/test failures
- [ ] Ensure vcpkg dependency installation works on all platforms
- [ ] Verify GoogleTest tests run in CI
- [ ] Add test result reporting/badges

**Files to modify:**
- `.github/workflows/test.yml`

**Acceptance criteria:**
- Workflow runs successfully on Linux and Windows
- GoogleTest tests execute successfully
- Clear pass/fail status visible in GitHub Actions

---

### Issue #4: Update Documentation
**Priority:** MEDIUM  
**Estimated Effort:** 1-2 hours  
**Assignable to:** Single agent

**Description:**
Update README and add testing documentation to help developers understand how to build and run tests.

**Tasks:**
- [ ] Add CI/CD status badge to README.md
- [ ] Document testing requirements (vcpkg, GoogleTest)
- [ ] Add instructions for running tests locally
- [ ] Document test framework used and why
- [ ] Add troubleshooting section for common build issues
- [ ] Update building instructions with vcpkg toolchain

**Files to modify:**
- `README.md`

**Files to create:**
- `docs/TESTING.md` (optional - detailed testing guide)

**Acceptance criteria:**
- README has clear testing instructions
- CI badge shows workflow status
- Developers can follow docs to run tests locally

---

## Implementation Order

**Phase 1 (Parallel):**
- Issue #1: Update GoogleTest Tests

**Phase 2 (After Phase 1):**
- Issue #2: Complete Server SSE Cleanup

**Phase 3 (After Phases 1 & 2):**
- Issue #3: Validate and Fix CI/CD Pipeline

**Phase 4 (After Phase 3):**
- Issue #4: Update Documentation

## Testing Strategy

**Local Testing:**
```bash
# Configure with vcpkg
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DMCP_BUILD_TESTS=ON

# Build
cmake --build build --config Release

# Run GoogleTest tests
cd build && ctest -R mcp_tests -V

# Run Catch2 tests
cd build && ctest -R mcp_catch2_tests -V
```

**CI Testing:**
- Automated on every PR
- Runs on Linux and Windows
- Must pass before merge

## Dependencies

- **vcpkg** (for dependency management)
- **GoogleTest** (installed via vcpkg feature "tests")
- **CMake 3.25+** (required for C++ module support and modern CMake features)

## Success Criteria

- [ ] All tests compile without errors
- [ ] All tests pass on all platforms
- [ ] CI/CD pipeline runs automatically on PRs
- [ ] Documentation is clear and complete
- [ ] No SSE-related code remains (except in git history)

## Notes

- Both GoogleTest and Catch2 are used for backward compatibility
- GoogleTest for existing tests, Catch2 for new tests
- vcpkg simplifies dependency management across platforms
- CI uses lukka/run-vcpkg action for consistency
