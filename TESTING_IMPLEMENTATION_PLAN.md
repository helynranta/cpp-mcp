# Testing Infrastructure Implementation Plan

## Overview
This document outlines the remaining tasks to complete the testing infrastructure and CI/CD pipeline for the cpp-mcp project.

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
cmake -B build \
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
- **CMake 3.10+**

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
