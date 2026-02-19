# C++ Modules Migration Plan - Phase 1: Preparation & Planning

## Executive Summary

This document outlines the comprehensive plan for migrating the cpp-mcp project to C++20/23 Modules with a **Windows-first approach**. As part of this migration, we are **dropping Linux and macOS support** to focus on Windows as the primary platform, leveraging MSVC's mature C++ Modules implementation.

**Status:** Phase 1 - Planning  
**Target Completion:** TBD  
**Last Updated:** 2026-02-19

---

## 1. Scope Definition

### 1.1 Platform Support Decision

**DECISION: Windows-Only, Dropping Linux/macOS Support**

**Rationale:**
- MSVC has the most mature and stable C++20 Modules implementation
- Windows 10+ is the primary deployment target
- Simplifies codebase by removing POSIX-specific code
- Reduces maintenance burden and testing complexity
- Aligns with C++ Modules best practices (single platform initially)

**Impact:**
- Remove all POSIX-specific code (fork(), pipe(), execvp(), etc.)
- Remove Linux/macOS build configurations
- Remove sanitizer and coverage presets (Linux-only)
- Simplify stdio client implementation (Windows-only)
- Update CI/CD to Windows-only builds

### 1.2 Migration Goals

**Primary Goals:**
1. Convert all header files to C++ module interface units (.cppm)
2. Convert source files to module implementation units
3. Maintain API compatibility where possible
4. Improve build times through module compilation
5. Reduce header dependencies and improve encapsulation

**Secondary Goals:**
1. Modernize build system for modules support
2. Update documentation for modules usage
3. Provide migration guide for downstream users
4. Establish best practices for modular C++ development

### 1.3 Out of Scope

**Not Included in This Migration:**
- Cross-platform support (Linux/macOS)
- MinGW/GCC compiler support
- Clang/LLVM compiler support (initially)
- C++23 experimental features beyond modules
- Standard library module (import std;) - may be added later

---

## 2. Platform-Specific Code Audit

### 2.1 Current Platform-Specific Code

Based on comprehensive audit, the following files contain platform-specific code:

#### Files to Simplify (Remove POSIX Code)

| File | Lines | Platform-Specific Features | Action Required |
|------|-------|---------------------------|-----------------|
| src/mcp_stdio_client.cpp | 440+ | Windows CreateProcess vs POSIX fork/exec | Remove POSIX branch, keep Windows-only |
| include/mcp_stdio_client.h | 240 | HANDLE vs int file descriptors | Remove POSIX types |
| examples/agent_example.cpp | ~300 | Windows console UTF-8 setup | Keep Windows code, remove POSIX guards |

#### Third-Party Code (No Changes)

| File | Notes |
|------|-------|
| common/json.hpp | nlohmann/json library - keep as-is, platform detection is internal |

### 2.2 Build System Changes Required

#### CMakeLists.txt

**Changes:**
1. Remove if(WIN32) guards - always assume Windows
2. Remove POSIX-specific options
3. Update minimum requirements to MSVC 2022+ (C++20 modules support)
4. Add module compilation flags

**Current (with guards):**
```cmake
if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()
```

**After (Windows-only):**
```cmake
# Windows-only project
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
add_compile_definitions(_WIN32_WINNT=0x0A00)

# Enable C++ Modules
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
```

#### CMakePresets.json

**Presets to Keep (renamed, Windows-only):**
- dev-debug (renamed from dev-debug-windows)
- dev-release (renamed from dev-release-windows)
- ci (renamed from ci-windows)
- release (renamed from release-windows)
- ssl (renamed from ssl-windows)

**Presets to Remove:**
- ci-linux
- sanitizer-address (Linux-only)
- sanitizer-undefined (Linux-only)
- coverage (Linux-only)
- All platform condition checks

### 2.3 CI/CD Changes

#### GitHub Actions Workflows

**test.yml:**
- Remove check-formatting job (uses ubuntu-latest)
- Keep only test-windows job (rename to test)
- Update to use renamed presets

**conformance.yml:**
- Already Windows-only ✓
- Update preset name from ci-windows to ci

**Formatting:**
- Move clang-format check to Windows runner
- Or remove automated formatting checks (manual only)

---

## 3. C++ Modules Architecture Design

### 3.1 Module Naming Convention

**Proposed Module Structure:**

```
mcp                          // Root module (re-exports)
├── mcp.core                 // Core protocol (messages, validation)
├── mcp.server               // Server implementation
├── mcp.client               // Client implementations
│   ├── mcp.client.stdio     // Stdio client
│   ├── mcp.client.sse       // SSE client
│   └── mcp.client.http      // HTTP client
├── mcp.http                 // HTTP abstraction & transport
├── mcp.tool                 // Tool management
├── mcp.resource             // Resource management
├── mcp.progress             // Progress notifications
└── mcp.logger               // Logging utilities
```

**Rationale:**
- Hierarchical naming follows best practices
- Clear separation of concerns
- Allows selective imports
- Facilitates partial migration

### 3.2 File Organization

**Proposed Directory Structure:**

```
cpp-mcp/
├── modules/                 # Module interface units (.cppm)
│   ├── mcp.cppm            # Root module
│   ├── mcp.core.cppm
│   ├── mcp.server.cppm
│   ├── mcp.client.cppm
│   ├── mcp.client.stdio.cppm
│   ├── mcp.client.sse.cppm
│   ├── mcp.client.http.cppm
│   ├── mcp.http.cppm
│   ├── mcp.tool.cppm
│   ├── mcp.resource.cppm
│   ├── mcp.progress.cppm
│   └── mcp.logger.cppm
├── src/                     # Module implementation units
│   ├── mcp.core.cpp
│   ├── mcp.server.cpp
│   └── ...
├── include/                 # Legacy headers (deprecated)
│   └── [to be removed after migration]
├── examples/               # Example applications
├── test/                   # Tests
├── common/                 # Third-party headers
└── CMakeLists.txt
```


### 3.3 Module Interface Design

**Example Module Interface (mcp.core.cppm):**

```cpp
module;

// Global module fragment (for #includes)
#include <string>
#include <vector>
#include <memory>
#include "common/json.hpp"  // Third-party headers

export module mcp.core;

// Export declarations
export namespace mcp {
    class request;
    class response;
    class notification;
    
    // Type aliases
    using json = nlohmann::json;
    
    // Constants
    inline constexpr const char* MCP_VERSION = "2025-11-25";
}

// Module implementation (inline definitions)
export namespace mcp {
    class request {
    public:
        std::string method;
        json params;
        json id;
        
        static request create(const std::string& method, const json& params);
        // ... more members
    };
}
```

### 3.4 Dependency Management

**Module Dependency Graph:**

```
mcp (root)
  └─ imports: mcp.core, mcp.server, mcp.client, mcp.tool, mcp.resource
  
mcp.server
  └─ imports: mcp.core, mcp.http, mcp.tool, mcp.resource, mcp.progress, mcp.logger

mcp.client
  └─ imports: mcp.core, mcp.logger

mcp.client.stdio
  └─ imports: mcp.client, mcp.core

mcp.client.sse
  └─ imports: mcp.client, mcp.core, mcp.http

mcp.http
  └─ imports: mcp.core

mcp.tool
  └─ imports: mcp.core

mcp.resource
  └─ imports: mcp.core

mcp.progress
  └─ imports: mcp.core

mcp.logger
  └─ imports: (none - uses std only)
```

---

## 4. Coding Conventions & Best Practices

### 4.1 Module Interface Guidelines

**DO:**
- Use export module for primary module declaration
- Use global module fragment for #includes
- Export only public API (classes, functions, constants)
- Use inline for header-only functionality
- Document exported entities with Doxygen comments
- Use namespace exports for logical grouping

**DON'T:**
- Export implementation details
- Use macros in module interfaces (use module purview instead)
- Include transitive dependencies unnecessarily
- Mix module and header includes in public API

### 4.2 Module Organization

**Module Interface (.cppm):**
- Primary module declaration
- Public API declarations
- Inline implementations for templates
- Documentation comments

**Module Implementation (.cpp):**
- Implementation of non-inline functions
- Private helper functions
- Static/internal linkage utilities

### 4.3 Naming Conventions

**Module Names:**
- Use dot notation: mcp.component.subcomponent
- Lowercase with underscores for multi-word: mcp.http_client
- Match directory structure when possible

**File Names:**
- Module interface: <module.name>.cppm
- Module implementation: <module.name>.cpp
- Keep consistent with module name

### 4.4 Build Configuration

**CMake Module Support:**

```cmake
# Enable CMake's native module support
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# MSVC-specific module flags
if(MSVC)
    add_compile_options(
        /std:c++20           # C++20 standard
        /interface           # Generate module interfaces
        /ifcOutput ${CMAKE_BINARY_DIR}/modules/
    )
endif()

# Define module library
add_library(mcp)
target_sources(mcp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            modules/mcp.cppm
            modules/mcp.core.cppm
            modules/mcp.server.cppm
            # ... more modules
)
```

### 4.5 Testing Strategy

**Test Approach:**
- Use module imports in tests: import mcp.core;
- Maintain Boost.Test framework
- Test both public API and internal components
- Use module partitions for test-only exports

**Example Test:**

```cpp
import mcp.core;
import mcp.server;

#define BOOST_TEST_MODULE MCP_Tests
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(RequestCreation) {
    mcp::request req = mcp::request::create("test", {});
    BOOST_CHECK_EQUAL(req.method, "test");
}
```

---

## 5. Migration Timeline & Phases

### Phase 1: Preparation & Planning (Current) ✓
**Duration:** 1 week  
**Status:** In Progress

**Tasks:**
- [x] Audit platform-specific code
- [x] Define Windows-only scope
- [x] Design module architecture
- [x] Define coding conventions
- [ ] Finalize migration checklist
- [ ] Review and approve plan

**Deliverables:**
- This document (MODULES_MIGRATION_PLAN.md)
- Platform audit report
- Module architecture design
- Build system strategy

---

### Phase 2: Platform Cleanup (Week 1-2)
**Duration:** 2 weeks  
**Dependencies:** Phase 1 complete

**Tasks:**
- Remove POSIX-specific code from mcp_stdio_client.cpp
- Remove POSIX types from mcp_stdio_client.h
- Remove POSIX guards from agent_example.cpp
- Update CMakeLists.txt (remove Windows conditionals)
- Update CMakePresets.json (remove Linux presets)
- Update GitHub Actions workflows
- Update documentation (README, build instructions)
- Run tests to verify Windows-only build

**Deliverables:**
- Windows-only codebase
- Updated build system
- Updated CI/CD
- Platform cleanup report

**Success Criteria:**
- All tests pass on Windows
- No POSIX code remains
- CI runs Windows-only builds

---

### Phase 3: Module Infrastructure Setup (Week 3)
**Duration:** 1 week  
**Dependencies:** Phase 2 complete

**Tasks:**
- Create modules/ directory structure
- Update CMakeLists.txt for module support
- Add CMake module scanning
- Set up MSVC module compilation flags
- Create initial mcp.cppm skeleton
- Test basic module compilation
- Document build process

**Deliverables:**
- Module directory structure
- CMake module build configuration
- Basic module compilation working
- Build documentation

**Success Criteria:**
- CMake detects and compiles modules
- Simple module compiles successfully
- Module import works in test case


### Phase 4: Core Module Migration (Week 4-5)
**Duration:** 2 weeks  
**Dependencies:** Phase 3 complete

**Tasks:**
- Create mcp.core module (messages, validation)
- Migrate mcp_message.h/cpp to module
- Migrate mcp_jsonrpc_validation.h to module
- Create mcp.logger module
- Migrate mcp_logger.h to module
- Update tests to use core modules
- Verify compilation and tests

**Deliverables:**
- Core modules implemented
- Tests using modules
- Migration guide (core modules)

---

### Phase 5-10: Remaining Migration Phases

**Phase 5:** Tool & Resource Modules (Week 6)
**Phase 6:** HTTP & Transport Modules (Week 7-8)
**Phase 7:** Server Module Migration (Week 9-10)
**Phase 8:** Client Modules Migration (Week 11-12)
**Phase 9:** Root Module & Cleanup (Week 13)
**Phase 10:** Documentation & Release (Week 14)

*Detailed task lists for these phases available in sections above.*

---

## 6. Migration Checklist

### 6.1 Platform Cleanup

**Code Changes:**
- Remove WIN32 conditional compilation
- Remove all POSIX code branches
- Simplify to Windows-only implementations

**Build System:**
- Remove Linux/macOS CMake presets
- Rename Windows presets (remove -windows suffix)
- Remove sanitizer and coverage presets

**CI/CD:**
- Windows-only workflow
- Update preset references

**Documentation:**
- Update README for Windows-only
- Update build instructions
- Create platform cleanup notes

### 6.2 Module Migration

**Infrastructure:**
- Create modules/ directory
- Enable CMake module scanning
- Configure MSVC module flags

**Component Migration (Phased):**
- Core modules (messages, logger)
- Component modules (tool, resource, progress)
- Transport modules (http, clients)
- Server and client modules
- Root module and cleanup

---

## 7. Risk Management

### Technical Risks
- MSVC module bugs - Mitigation: Incremental migration
- Build time increase - Mitigation: Module caching
- API breaking changes - Mitigation: Compatibility layer

### Schedule Risks
- Complexity underestimation - Mitigation: 20% buffer
- Resource availability - Mitigation: Clear phase boundaries

### Project Risks
- User migration friction - Mitigation: Comprehensive guide
- Backward compatibility - Mitigation: Transition period

---

## 8. Success Criteria

**Phase 1 (Current):**
- Platform audit complete
- Windows-only scope defined
- Module architecture designed
- Coding conventions established

**Overall Migration:**
- All code uses modules
- No POSIX code remains
- Windows-only codebase
- All tests pass
- Build times maintained or improved
- Complete documentation

---

## 9. References

### Internal
- README.md - Project overview
- AGENTS.md - Development guidelines
- CMakeLists.txt - Build configuration
- CMakePresets.json - Build presets

### External
- C++20 Modules (cppreference.com)
- MSVC Modules Documentation (Microsoft)
- CMake Module Support documentation

---

## 10. Appendix

### File Mapping

Current headers will be migrated to modules as follows:

- mcp_message.h → mcp.core module
- mcp_logger.h → mcp.logger module
- mcp_tool.h → mcp.tool module
- mcp_resource.h → mcp.resource module
- mcp_progress.h → mcp.progress module
- mcp_http_*.h → mcp.http module
- mcp_server.h → mcp.server module
- mcp_client*.h → mcp.client.* modules

---

**Status:** Draft for Review  
**Last Updated:** 2026-02-19  
**Next Steps:** Review and approval

---

*End of document*
