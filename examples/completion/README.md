# Completion Example

Demonstrates **argument autocompletion** for prompts and resource templates (MCP 2025-06-18).

## Features

- Server declares `completions` capability
- Completion handlers with context-aware suggestions
- Results with `_meta` metadata field

## Build and Run

```bash
cmake --build build --target completion_example
.\build\examples\completion_example.exe
```
