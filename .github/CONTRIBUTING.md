# Contributing to cpp-mcp

Thank you for contributing to cpp-mcp! This guide will help you ensure your changes pass CI checks.

## Before Committing - Required Steps

**Every commit must follow these steps to pass CI:**

### 1. Format Your Code

All C++ code must be formatted with clang-format before committing:

```bash
# Format all changed files
find src include examples test -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### 2. Verify Formatting

Check that formatting is correct (this is what CI runs):

```bash
# This must exit with code 0 for CI to pass
clang-format --dry-run --Werror src/**/*.cpp include/**/*.h test/**/*.cpp examples/**/*.cpp
```

### 3. Build the Project

Build to ensure there are no compilation errors:

```bash
# Using CMake presets (recommended)
cmake --preset dev-release
cmake --build --preset dev-release

# OR manual configuration
cmake -B build -G Ninja -DMCP_BUILD_TESTS=ON \
  -DVCPKG_MANIFEST_FEATURES="tests" \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release -j$(nproc)
```

### 4. Run Tests

Verify your changes don't break existing functionality:

```bash
# Using presets
ctest --preset dev-release

# OR manual
cd build && ctest -V
```

## Quick Pre-Commit Checklist

Before running `git commit`:

- [ ] Code formatted with clang-format
- [ ] Formatting verified with `--dry-run --Werror`
- [ ] Project builds successfully
- [ ] All tests pass
- [ ] New tests added for new features
- [ ] Documentation updated if APIs changed

## Common Issues

### Build Failure: Formatting

**Error**: `clang-format check failed`

**Solution**: Run `clang-format -i` on all changed files:
```bash
find src include test -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Build Failure: Compilation Error

**Error**: Syntax errors, missing includes, etc.

**Solution**: 
1. Fix the compilation errors locally
2. Run full build to verify: `cmake --build build`
3. Test the fix: `cd build && ctest`

### Test Failure

**Error**: Tests fail in CI

**Solution**:
1. Run tests locally: `cd build && ctest -V`
2. Fix failing tests
3. Verify all tests pass before committing

## Development Workflow

### Recommended Workflow

```bash
# 1. Make your changes
vim src/my_file.cpp

# 2. Format the code
clang-format -i src/my_file.cpp

# 3. Build
cmake --build build

# 4. Run tests
cd build && ctest -V

# 5. If all passes, commit
git add src/my_file.cpp
git commit -m "feat: add new feature"
git push
```

### Test-Driven Development (TDD)

The project follows TDD principles:

1. **Write failing test first**
   ```cpp
   BOOST_AUTO_TEST_CASE(NewFeatureWorks) {
       // Test the feature that doesn't exist yet
       BOOST_CHECK(new_feature_works());
   }
   ```

2. **Implement minimal code to pass**
   ```cpp
   bool new_feature_works() {
       return true; // Minimal implementation
   }
   ```

3. **Refactor while keeping tests green**
   - Improve code quality
   - Add edge case handling
   - Keep all tests passing

4. **Format and commit**
   ```bash
   clang-format -i test/my_test.cpp src/my_feature.cpp
   git add test/my_test.cpp src/my_feature.cpp
   git commit -m "feat: implement new feature"
   ```

## CI/CD Pipeline

The CI pipeline runs:

1. **Formatting Check** (blocking)
   - Uses clang-format with `--dry-run --Werror`
   - Fails if any file is not properly formatted

2. **Build** (blocking)
   - Builds on Linux and Windows
   - Must compile without errors

3. **Tests** (blocking)
   - Runs full test suite
   - All tests must pass

4. **Conformance Tests** (advisory)
   - Tests MCP protocol compliance
   - Some expected failures documented in `conformance-baseline.yml`

## Editor Setup

Configure your editor to format on save:

### VS Code

Add to `.vscode/settings.json`:
```json
{
  "editor.formatOnSave": true,
  "C_Cpp.clang_format_style": "file"
}
```

### Vim

Add to `.vimrc`:
```vim
autocmd BufWritePre *.cpp,*.h :silent! execute '!clang-format -i ' . shellescape(expand('%'))
```

### CLion

1. Settings → Editor → Code Style → C/C++
2. Enable "Enable ClangFormat"
3. Check "Format on save"

## Getting Help

- Check [AGENTS.md](../AGENTS.md) for detailed development guidelines
- Review [README.md](../README.md) for project overview
- See [MCP_BREAKING_CHANGES.md](../MCP_BREAKING_CHANGES.md) for protocol changes

## Additional Resources

- [C++23 Standards](https://en.cppreference.com/w/cpp/23)
- [Boost.Test Documentation](https://www.boost.org/doc/libs/release/libs/test/)
- [MCP Specification](https://spec.modelcontextprotocol.io/)
