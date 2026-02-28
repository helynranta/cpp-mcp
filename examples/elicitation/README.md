# Elicitation Example

Demonstrates **human-in-the-loop elicitation** (MCP 2025-06-18), allowing tools to request user input during execution.

## Features

- Server requests structured user input via `elicitation/create`
- JSON Schema validation of user responses
- Action handling: accept, decline, cancel

## Build and Run

```bash
cmake --build build --target elicitation_example
.\build\examples\elicitation_example.exe
```
