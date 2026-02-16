# Agent-Based Development Guide

This document provides guidelines for AI agents and developers working on the cpp-mcp project. It outlines best practices, workflows, and expectations for contributing to this repository.

## Overview

The cpp-mcp project is designed to be developed collaboratively by multiple AI agents and human developers. Each agent can work on isolated tasks, following Test-Driven Development (TDD) practices to ensure code quality and maintainability.

## Core Development Principles

### 1. Test-Driven Development (TDD)

**All new features MUST follow TDD practices:**

1. **Write the test first** - Before writing any implementation code, write a failing test that defines the expected behavior
2. **Run the test** - Verify that the test fails for the right reason
3. **Write minimal code** - Implement just enough code to make the test pass
4. **Run tests again** - Verify that the new test passes and all existing tests still pass
5. **Refactor** - Clean up the code while keeping tests green
6. **Repeat** - Continue this cycle for each new feature or bug fix

**Example TDD Workflow:**

```bash
# 1. Write test in test/mcp_test.cpp
BOOST_AUTO_TEST_CASE(WorksCorrectly) {
    // Arrange
    auto component = create_test_component();
    
    // Act
    auto result = component.new_feature();
    
    // Assert
    BOOST_CHECK(result.is_valid());
}

# 2. Verify test fails
cmake --build build
cd build && ctest -R mcp_tests -V

# 3. Implement the feature
# ... write implementation code ...

# 4. Verify test passes
cd build && ctest -R mcp_tests -V

# 5. Refactor if needed while keeping tests green
```

### 2. Code Quality Standards

- **Follow C++23 standards** - The project uses C++23 features and requires C++23-compliant compilers
- **Use meaningful names** - Variables, functions, and classes should have descriptive names
- **Document public APIs** - All public functions and classes must have documentation comments
- **Keep functions focused** - Each function should do one thing well
- **Avoid code duplication** - Extract common code into reusable functions

### 3. Testing Requirements

**Test framework used:**

- **Boost.Test** - For all tests (unit, integration, and client/server communication)

**Test Coverage Requirements:**
- All new features must have corresponding tests
- Bug fixes should include regression tests
- Aim for high code coverage (>80% for new code)
- Tests should be independent and repeatable

**Running Tests:**

```bash
# Using CMake presets (recommended)
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release

# Or for debug builds
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug

# Manual configuration (alternative)
cmake -B build -DMCP_BUILD_TESTS=ON \
  -DVCPKG_MANIFEST_FEATURES="tests" \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# Run Boost.Test suite
cd build && ctest -R mcp_tests -V

# Run all tests
cd build && ctest -V
```

## Agent Workflow

### Task Assignment

Tasks are organized as GitHub issues with specific labels:
- `priority: high`, `priority: medium`, `priority: low` - Task priority
- `enhancement` - New features
- `bug` - Bug fixes
- `testing` - Test-related tasks
- `documentation` - Documentation updates

### Working on a Task

1. **Claim the issue** - Comment on the issue to claim it and prevent duplication
2. **Create a branch** - Use a descriptive branch name: `feature/description` or `fix/issue-number`
3. **Understand the requirements** - Read the issue description and acceptance criteria carefully
4. **Write tests first (TDD)** - Start by writing failing tests
5. **Implement the feature** - Write minimal code to pass the tests
6. **Run tests locally** - Ensure all tests pass before pushing
7. **Update documentation** - Update relevant docs if APIs changed
8. **Create a pull request** - Include a clear description of changes
9. **Address review feedback** - Respond to code review comments promptly

### Code Review Process

All code changes must be reviewed:
- **Self-review** - Review your own changes before submitting
- **Automated checks** - CI/CD pipeline must pass (builds on Linux and Windows)
- **Peer review** - At least one approval required from another contributor
- **Test validation** - All tests must pass in CI

## Project Structure

```
cpp-mcp/
├── include/           # Public header files
│   ├── mcp_client.h
│   ├── mcp_server.h
│   ├── mcp_tool.h
│   └── ...
├── src/              # Implementation files
│   ├── mcp_server.cpp
│   ├── mcp_tool.cpp
│   └── ...
├── test/             # Test files
│   ├── mcp_test.cpp         # Boost.Test tests
│   └── ...                  # Other test files
├── examples/         # Example applications
│   ├── server_example.cpp
│   ├── stdio_client_example.cpp
│   └── agent_example.cpp
├── .github/          # GitHub Actions workflows
│   └── workflows/
│       └── test.yml  # CI/CD pipeline
└── vcpkg.json       # Dependency manifest
```

## Common Development Tasks

### Adding a New Feature

1. **Create an issue** describing the feature
2. **Write tests first:**
   ```cpp
   // test/mcp_test.cpp
   BOOST_AUTO_TEST_CASE(Behavior) {
       // Write test before implementation
       BOOST_CHECK(new_feature_works());
   }
   ```
3. **Implement the feature** in appropriate files
4. **Verify tests pass**
5. **Update documentation** in header files and README if needed
6. **Submit PR** with test results

### Fixing a Bug

1. **Write a regression test** that reproduces the bug
2. **Verify the test fails** with the bug present
3. **Fix the bug** with minimal changes
4. **Verify the test passes** and no other tests broke
5. **Submit PR** with before/after test results

### Adding a New Tool

Tools are registered in the server and can be called by clients:

```cpp
// 1. Write test first
BOOST_AUTO_TEST_CASE(NewTool) {
    tool new_tool = tool_builder("my_tool")
        .with_description("Description")
        .with_string_param("param", "Description", "default")
        .build();
    
    BOOST_CHECK_EQUAL(new_tool.name, "my_tool");
}

// 2. Implement the tool
tool my_tool = tool_builder("my_tool")
    .with_description("My new tool")
    .with_string_param("input", "Input parameter", "")
    .build();

server.register_tool(my_tool, [](const json& params, const std::string&) -> json {
    // Implementation
    return result;
});
```

### Updating Dependencies

Dependencies are managed via vcpkg:

1. **Update vcpkg.json** to add/modify dependencies
2. **Test locally** with updated dependencies
3. **Update CI workflow** if needed (.github/workflows/test.yml)
4. **Document changes** in commit message

#### Current Dependencies

The project currently depends on:

- **Boost.Beast** (`boost-beast`) - HTTP and WebSocket networking library
  - Automatically includes Boost.Asio and Boost.System as transitive dependencies
  - Used for all HTTP transport (client and server)
  - Provides async I/O and SSE streaming support

**HTTP Transport Implementation:**
- Default factory uses Boost.Beast: `mcp::http::create_server()` and `mcp::http::create_client()`
- All MCP HTTP communication uses the Beast-based implementation
- Examples demonstrate Beast client usage for both MCP and external APIs

All dependencies are managed through vcpkg manifest mode and automatically installed when using the vcpkg toolchain file.

## CI/CD Pipeline

The project uses GitHub Actions for continuous integration:

- **Platforms tested:** Ubuntu, Windows
- **Build configurations:** Release

- **Test framework:** Boost.Test
- **Dependency management:** vcpkg with binary caching


**Pipeline triggers:**
- Pull requests to main/master branches
- Direct pushes to main/master
- Manual workflow dispatch

**vcpkg Binary Caching:**
- Both Linux and Windows use file-based cache stored in GitHub Actions cache
  - Uses `files` provider to store binaries locally
  - GitHub Actions cache persists between runs
  - Cache key based on vcpkg.json and vcpkg-configuration.json hashes
- First build: Dependencies are built and cached
- Subsequent builds: Pre-built binaries are downloaded, significantly reducing build time
- Cache is automatically managed per platform

**Requirements for merge:**
- ✅ All builds must succeed
- ✅ All tests must pass
- ✅ Code review approved
- ✅ No merge conflicts

## Best Practices for Agents

### Communication

- **Clear commit messages** - Use conventional commit format: `feat:`, `fix:`, `test:`, `docs:`
- **Descriptive PR titles** - Summarize the change clearly
- **Comment on issues** - Update status and ask questions when needed
- **Reply to reviews** - Address all review comments

### Collaboration

- **Avoid conflicts** - Check if someone else is working on the same issue
- **Small, focused changes** - Each PR should address one concern
- **Reuse existing code** - Don't reinvent the wheel
- **Follow project conventions** - Match the existing code style

### Error Handling

When encountering issues:
1. **Check existing tests** - Run tests to understand failures
2. **Review recent changes** - Use `git log` to see what changed
3. **Read error messages carefully** - Compiler and test output provides clues
4. **Ask for help** - Comment on the issue if stuck

## Testing Strategy

### Unit Tests

Test individual components in isolation:
```cpp
BOOST_AUTO_TEST_CASE(CreatesValidTools) {
    tool t = tool_builder("test")
        .with_description("Test tool")
        .build();
    BOOST_CHECK_EQUAL(t.name, "test");
}
```

### Integration Tests

Test component interactions:
```cpp
BOOST_AUTO_TEST_CASE(Communication) {
    TestServer server(8891);
    server.start();
    
    // Test server is reachable
    BOOST_CHECK(server.is_running());
}
```

### Test Organization

- **Use test fixtures** - Group related tests with Boost.Test fixtures
- **Descriptive names** - Test names should describe what they verify
- **Independent tests** - Tests should not depend on execution order
- **Clean up resources** - Use RAII or test fixtures for cleanup

## Documentation Requirements

### Code Documentation

```cpp
/**
 * @brief Brief description of the function
 * 
 * Detailed description of what the function does,
 * its parameters, and return value.
 * 
 * @param param1 Description of parameter
 * @return Description of return value
 * @throws mcp_exception Description of when exception is thrown
 */
json process_request(const request& param1);
```

### README Updates

Update README.md when:
- Adding new features visible to users
- Changing build requirements
- Adding new examples
- Modifying public APIs

### Changelog

Maintain TESTING_IMPLEMENTATION_PLAN.md and related docs:
- Mark tasks as complete when done
- Update status of ongoing work
- Document any blockers or issues

## Resources

### Project Documentation

- **README.md** - Project overview and quick start
- **TESTING_IMPLEMENTATION_PLAN.md** - Current testing roadmap
- **MCP_UPDATE_PLAN.md** - Feature update roadmap
- **TECHNICAL_ARCHITECTURE.md** - Technical design details

### External Resources

- [Model Context Protocol Specification](https://spec.modelcontextprotocol.io/)
- [MCP GitHub Repository](https://github.com/modelcontextprotocol/modelcontextprotocol) - Official specification and protocol details
- [Boost.Test Documentation](https://www.boost.org/doc/libs/1_90_0/libs/test/doc/html/index.html)
- [vcpkg Documentation](https://vcpkg.io/en/docs/)

## Quick Reference

### Common Commands

```bash
# Using CMake Presets (Recommended)

# Development with tests
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release

# Debug build
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug

# Sanitizers (Linux/macOS only)
cmake --preset sanitizer-address
cmake --build --preset sanitizer-address
ctest --preset sanitizer-address

# Code coverage (Linux/macOS only)
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage

# List all presets
cmake --list-presets

# Manual Configuration (Alternative)

# Setup
git submodule update --init --recursive
cmake -B build -DMCP_BUILD_TESTS=ON \
  -DVCPKG_MANIFEST_FEATURES="tests" \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release -j$(nproc)

# Test
cd build && ctest -V

# Run specific test
cd build && ctest -R mcp_tests -V

# Clean build
rm -rf build && cmake -B build ...
```

### Commit Message Format

```
<type>: <subject>

<body>

<footer>
```

**Types:** feat, fix, test, docs, style, refactor, chore

**Example:**
```
feat: Add new calculator tool with TDD approach

- Wrote failing tests for addition, subtraction operations
- Implemented calculator tool with basic arithmetic
- All tests passing
- Updated documentation

Closes #42
```

## Contact and Support

- **Issues** - Use GitHub Issues for bug reports and feature requests
- **Discussions** - Use GitHub Discussions for questions and ideas
- **Pull Requests** - Submit PRs for code contributions

---

**Remember:** Always follow TDD when developing new features. Write tests first, then implement the feature to make those tests pass. This ensures code quality, maintainability, and reduces bugs.
