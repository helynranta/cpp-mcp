# GitHub Copilot Instructions for cpp-mcp

This repository implements the Model Context Protocol (MCP) in C++23. When contributing to this project, follow these guidelines to ensure high-quality, consistent code.

## Core Development Philosophy

**Test-Driven Development (TDD) is mandatory** for all new features:
1. Write failing tests first
2. Implement minimal code to pass tests
3. Refactor while keeping tests green
4. See [AGENTS.md](../AGENTS.md) for detailed TDD workflow

## Build & Test Commands

### Setup
```bash
# Clone with vcpkg (if not already done)
git submodule update --init --recursive
```

### Using CMake Presets (Recommended)

```bash
# Development with tests (Release)
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release

# Development with tests (Debug)
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug

# List all available presets
cmake --list-presets
```

### Manual Configuration (Alternative)

```bash
# Configure with tests enabled
cmake -B build -DMCP_BUILD_TESTS=ON \
  -DVCPKG_MANIFEST_FEATURES="tests" \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build the project
cmake --build build --config Release -j$(nproc)

# Run all tests
cd build && ctest -V

# Run specific test suite
cd build && ctest -R mcp_tests -V
```

### Linting and Formatting

The project uses **clang-format** for automated code formatting:

```bash
# Format changed files before committing
find src include examples test -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting (what CI runs)
clang-format --dry-run --Werror src/**/*.cpp include/**/*.h
```

**Style Guide:**
- Based on LLVM preset (.clang-format in project root)
- 120 character line limit, 4-space indentation
- Sorted and grouped includes
- All PRs must pass clang-format CI checks

**EditorConfig:** Basic whitespace rules in .editorconfig (most editors auto-apply)

See [README.md - Code Style and Formatting](../README.md#code-style-and-formatting) for complete formatting guidelines.

## Technology Stack

- **Language**: C++23 (requires C++23-compliant compiler)
- **Build System**: CMake 3.25+ with Ninja generator
- **Package Manager**: vcpkg (manifest mode)
- **Test Framework**: Boost.Test (all tests use Boost.Test, not GoogleTest)
- **HTTP Library**: Boost.Beast for HTTP transport and SSE streaming
- **Primary Dependencies**:
  - `boost-beast` - HTTP/WebSocket networking (includes Boost.Asio and Boost.System)
  - `nlohmann-json` - JSON parsing and serialization

## Code Quality Standards

### C++ Standards
- Use C++23 features and modern C++ idioms
- Follow RAII principles for resource management
- Use `std::jthread` for automatic thread joining with stop tokens
- Prefer value types over pointers where appropriate
- Document all public APIs with Doxygen-style comments

### Naming Conventions
- Use `snake_case` for variables, functions, and namespaces
- Use `PascalCase` for class/struct names
- Use meaningful, descriptive names

### Code Formatting
- **All code must be formatted with clang-format before committing**
- CI enforces formatting - PRs fail if code is not formatted
- Run `clang-format -i <files>` on changed files before pushing
- Configure your editor to format on save (recommended)

### Testing Requirements
- All new features MUST have tests
- Bug fixes MUST include regression tests
- Use Boost.Test framework exclusively
- Test coverage goal: >80% for new code
- Tests must be independent and repeatable

### Example Test Structure
```cpp
BOOST_AUTO_TEST_CASE(DescriptiveName) {
    // Arrange
    auto component = create_test_component();
    
    // Act
    auto result = component.method();
    
    // Assert
    BOOST_CHECK(result.is_valid());
    BOOST_CHECK_EQUAL(result.value, expected);
}
```

## Project Structure

```
cpp-mcp/
├── include/           # Public header files
│   ├── mcp_server.h
│   ├── mcp_client.h
│   ├── mcp_tool.h
│   └── ...
├── src/              # Implementation files
├── test/             # Boost.Test test files
│   ├── mcp_test.cpp  # Main test module
│   └── *_test.cpp    # Additional test suites
├── examples/         # Example applications
│   ├── server_example.cpp
│   ├── stdio_client_example.cpp
│   └── agent_example.cpp
├── .github/
│   └── workflows/
│       └── test.yml  # CI/CD pipeline
└── vcpkg.json       # Dependency manifest
```

## Important Conventions

### HTTP Transport
- Default HTTP implementation uses Boost.Beast
- Factory methods: `mcp::http::create_server()` and `mcp::http::create_client()`
- Use `http::client_result` (struct) with dot operator: `res.status_code`, `res.success`
- Headers are `std::multimap` - use `headers.emplace()` not `headers[]`

### Boost.Test Specific
- Test module defined in `test/mcp_test.cpp` with `BOOST_TEST_MODULE`
- Test suite and fixture names must differ: `BOOST_FIXTURE_TEST_SUITE(SuiteNameTestSuite, FixtureName)`
- Don't use `BOOST_CHECK_NE` with iterators; use `BOOST_CHECK(it != container.end())`
- Manual tests with threads must use bounded future waits to prevent CI hangs

### Threading
- Use `std::jthread` for automatic cleanup and cooperative cancellation
- SSE heartbeat loops should use stop-token-interruptible sleep
- Avoid blocking thread joins that can cause CI hangs

### Session Management
- Server provides `set_session_state()`, `get_session_state()`, `clear_session_state()`
- State is automatically cleaned up in `close_session()`

### Windows Compatibility
- Windows-specific includes (`<io.h>`, `<fcntl.h>`) must be wrapped in `#if defined(_WIN32)` guards
- Use `_setmode()` for Windows console output mode changes

## Dependency Management

Dependencies are managed via vcpkg manifest mode (`vcpkg.json`):

1. **Adding a dependency**: Update `vcpkg.json` with the package name
2. **Testing locally**: Rebuild with vcpkg toolchain file
3. **Update CI if needed**: Modify `.github/workflows/test.yml` if special configuration required
4. **Document changes**: Explain in commit message and PR description

Current vcpkg configuration uses:
- Manifest mode with feature flags (e.g., `"tests"` feature for Boost.Test)
- Binary caching via file-based provider with GitHub Actions cache
- Automatic dependency resolution

## CI/CD Pipeline

The GitHub Actions workflow (`.github/workflows/test.yml`) tests on:
- **Linux** (Ubuntu latest)
- **Windows** (Windows latest)

### CI Requirements
- All builds must succeed
- All tests must pass
- No merge conflicts
- Code review approval required

### vcpkg Binary Caching
- Both platforms use file-based cache stored in GitHub Actions cache
- Cache key based on `vcpkg.json` and `vcpkg-configuration.json` hashes
- Significantly reduces build time on subsequent runs

## Common Pitfalls to Avoid

1. **Don't use GoogleTest** - All tests use Boost.Test
2. **Don't skip TDD** - Always write tests before implementation
3. **Don't use blocking sleeps in async code** - Use stop-token-interruptible waits
4. **Don't remove working tests** - Only modify tests if they're broken or testing wrong behavior
5. **Don't add temporary files to git** - Use `/tmp` for temporary files
6. **Don't introduce security vulnerabilities** - Follow secure coding practices
7. **Don't break existing functionality** - Run tests before and after changes

## Documentation Requirements

### Code Documentation
Use Doxygen-style comments for public APIs:
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
Update `README.md` when:
- Adding new features visible to users
- Changing build requirements
- Adding new examples
- Modifying public APIs

## Commit Message Format

Use conventional commit format:
```
<type>: <subject>

<body>

<footer>
```

**Types**: `feat`, `fix`, `test`, `docs`, `style`, `refactor`, `chore`

**Example**:
```
feat: Add new calculator tool with TDD approach

- Wrote failing tests for addition, subtraction operations
- Implemented calculator tool with basic arithmetic
- All tests passing
- Updated documentation

Closes #42
```

## Additional Resources

For comprehensive guidelines, see:
- **[AGENTS.md](../AGENTS.md)** - Detailed agent-based development guide
- **[README.md](../README.md)** - Project overview and quick start
- **[SECURITY.md](../SECURITY.md)** - Security policies and guidelines
- **[Model Context Protocol Specification](https://spec.modelcontextprotocol.io/)** - MCP specification

## Boundaries and Restrictions

### DO:
- Follow TDD practices religiously
- Write focused, minimal changes
- Use existing libraries and patterns
- Update documentation for API changes
- Run tests before pushing
- Use vcpkg for dependency management

### DO NOT:
- Modify files in `.github/agents/` directory (instructions for other agents)
- Remove or modify working tests unless they're broken
- Introduce new test frameworks (use Boost.Test)
- Add dependencies without updating vcpkg.json
- Skip writing tests for new features
- Create helper scripts when standard tools exist
- Commit secrets or sensitive data
- Use deprecated C++ features

## Working with Issues

When assigned an issue:
1. Read the issue description and comments carefully
2. Understand acceptance criteria
3. Write failing tests that define success
4. Implement minimal code to pass tests
5. Run full test suite to ensure no regressions
6. Update relevant documentation
7. Create PR with clear description

For questions or clarification, comment on the issue - don't guess or make assumptions.

---

**Remember**: Quality over speed. Test-driven development ensures correctness, maintainability, and reduces bugs. When in doubt, refer to [AGENTS.md](../AGENTS.md) for detailed guidance.
