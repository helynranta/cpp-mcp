# Session State Management Example

Demonstrates how to use the **session state storage API** to maintain state across tool calls within a session.

## Features

- `set_session_state()` / `get_session_state()` / `clear_session_state()` API
- Per-client isolated state
- Automatic cleanup on session close

## Build and Run

```bash
cmake --build build --target session_state_example
.\build\examples\session_state_example.exe
```
