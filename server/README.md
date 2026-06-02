# Unreal MCP Server

Python FastMCP bridge for the UnrealMCP editor plugin.

The MCP client launches this server over stdio. The server exposes Python
tool wrappers, then forwards each Unreal-facing command to the editor plugin
over local TCP at `127.0.0.1:55557` using:

```json
{"type": "<command_name>", "params": {...}}
```

The editor response shape is strict:

```json
{"success": true, "...payload": "..."}
{"success": false, "error": "..."}
```

## Setup

Prerequisites: Python 3.12+, `uv`, Unreal Engine 5.7+, and a project with
the UnrealMCP plugin loaded.

```bash
cd server
uv venv
uv pip install -e .
```

Configure your MCP client to run:

```bash
uv --directory /absolute/path/to/unreal-mcp/server run unreal_mcp_server.py
```

See the top-level `README.md` and `examples/mcp-client-config.json` for
client-specific config examples.

## Validation

`smoke_dispatch.py` is safe/read-only by default. It pings only an explicit
allowlist of read-only wire commands with empty params and skips mutating,
saving, viewport/UI, screenshot, PIE-control, and code-execution commands.

```bash
cd server
uv run python smoke_dispatch.py --list-only  # no socket traffic
uv run python smoke_dispatch.py              # safe/read-only editor smoke
```

Full empty-param dispatch is available only as an explicit opt-in:

```bash
uv run python smoke_dispatch.py --allow-mutating
```

`--allow-mutating` can modify or save the currently open Unreal project. Use
it only in a throwaway project or after taking an intentional checkpoint.

The integration tests in `server/tests/test_object_property.py` require a
live editor with the plugin listening on `127.0.0.1:55557`:

```bash
uv run pytest tests/test_object_property.py -q
```

Pure-Python tests, such as the smoke classifier tests, do not require the
editor:

```bash
uv run pytest tests/test_smoke_dispatch.py -q
```

## Development

To add a normal Unreal-facing tool:

1. Add a C++ handler in the matching file under
   `plugin/Source/UnrealMCP/Private/Commands/`.
2. Register the handler at the definition site with `REGISTER_MCP_COMMAND`.
3. Add a Python wrapper in `server/tools/<category>_tools.py` using
   `@unreal_tool(mcp)`.
4. Write the tool docstring as the agent-facing catalog entry.
5. Classify the new command in `smoke_dispatch.py` if it belongs in the
   safe default smoke. Otherwise it remains skipped until deliberately
   run with `--allow-mutating`.

For wrappers that compose other commands or return FastMCP content blocks
such as screenshots, use `@mcp.tool()` and call `dispatch_unreal_command`
directly.
