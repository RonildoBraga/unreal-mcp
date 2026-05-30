# Tests

This directory holds the integration test suite for the unreal-mcp plugin.

## Status

**Placeholder.** Sprint 1 shipped 18 tools without a paired test suite — we
validated them by hand against a live UE editor (Lauder project's Phase 7.1
kit inventory was the first real-world exercise). Sprint 2+ adds tests as
new tools land.

## Strategy

Two test layers:

### Unit tests (Python side)
Pure-Python tests of the JSON marshalling and response handling. Don't need
a live editor. Run via `pytest` from `server/`.

### Integration tests (live editor)
Tests that send a real MCP command, wait for the plugin's TCP response, and
assert on the result. Need:
- The UE editor running with `UnrealMCPSample` (or any project with the
  plugin) loaded
- The plugin's TCP server reachable on port 55557

Run as `pytest --integration` (the marker gates the slower live-editor tests).

## Why no tests yet

Honest reason: Sprint 1's priority was unblocking Phase 7.1 (the kit
inventory) in the Lauder project. Hand-validation against the real use case
was higher-signal than synthetic tests would have been. Sprint 2 starts
adding test coverage so we don't accumulate untested surface area.

## Adding a test

When you add a new MCP tool:

1. Write a Python unit test in `tests/unit/test_<category>.py` covering the
   tool's argument marshalling and response unwrapping. Mock the TCP
   socket.
2. Write an integration test in `tests/integration/test_<category>.py` that
   sends a real command and asserts on the response. Mark it with the
   `integration` pytest marker.
3. Make sure both pass before opening a PR.
