# Architecture

unreal-mcp is a **two-process system** that bridges a Model Context Protocol
(MCP) client (Claude Desktop, Claude Code, Cursor, etc.) to an Unreal Engine
editor. The two processes talk over a local TCP socket.

```
┌──────────────────────────┐         ┌────────────────────────────┐
│  Python MCP Server       │         │  UE Editor (UnrealMCP)     │
│  (FastMCP framework)     │         │                            │
│                          │  TCP    │  MCPServerRunnable         │
│  unreal_mcp_server.py    │  55557  │      │                     │
│    │                     │ ◄─────► │      ↓                     │
│    ├─ tools/asset_tools  │  JSON   │  UnrealMCPBridge           │
│    ├─ tools/editor_tools │         │      │                     │
│    ├─ tools/level_tools  │         │      ├─► EditorCommands    │
│    ├─ tools/blueprint_   │         │      ├─► BlueprintCommands │
│    ├─ tools/node_tools   │         │      ├─► NodeCommands      │
│    ├─ tools/project_     │         │      ├─► ProjectCommands   │
│    ├─ tools/umg_tools    │         │      ├─► UMGCommands       │
│    └─ tools/asset_tools  │         │      ├─► AssetCommands     │
│                          │         │      └─► LevelCommands     │
│  @mcp.tool()             │         │                            │
│  decorated functions     │         │                            │
└──────────────────────────┘         └────────────────────────────┘
       ▲                                       │
       │ MCP protocol (stdio)                  │ UE 5.7 C++ APIs
       │                                       ▼
┌──────┴───────────────────┐         (AssetRegistry,
│  MCP client              │          EditorActorSubsystem,
│  (Claude / Cursor / ...) │          BlueprintEditorUtils,
└──────────────────────────┘          ULevelEditorSubsystem, etc.)
```

## The two halves

### Plugin (`plugin/`)

C++ UE plugin that runs **inside the editor process**. Listens on TCP port
55557 (constant `MCP_SERVER_PORT` in `UnrealMCPBridge.cpp`). Receives JSON
commands of the shape `{type: "<command_name>", params: {...}}`. Dispatches
each command to a category handler (one C++ class per category) which calls
into UE 5.7 C++ APIs and returns a JSON result.

The plugin is **editor-only** — none of it ships in a packaged game. Module
type `Editor` in `UnrealMCP.uplugin`.

### Server (`server/`)

Python MCP server using the `FastMCP` framework. Exposes one Python function
per MCP tool (decorated with `@mcp.tool()`); each function marshals its
arguments to JSON, opens a fresh TCP socket to the plugin, sends the JSON,
waits for the JSON response, and returns it to the MCP client.

The server is launched by the MCP client (Claude, Cursor, etc.) per the
client's MCP config — see `examples/mcp-client-config.json` for an example.

## Why TCP, not (e.g.) shared memory or stdio

- **Process boundary.** The plugin runs inside the UE editor process; the
  Python server runs as a separate child of the MCP client. They have no
  shared address space.
- **Lifecycle independence.** The editor can restart without restarting the
  Python server, and vice versa. The TCP server is set up to allow rapid
  reconnect (`SetReuseAddr(true)`).
- **Cross-language.** C++ ↔ Python over JSON is the lowest-common-denominator
  ABI that's reliable on Windows + macOS + Linux.

## Adding a new tool

Both sides need a change. See `CONTRIBUTING.md` for the style guide and
worked example. The short version:

1. **C++ side:** add a `HandleXxx` method to the appropriate `FUnrealMCP<Category>Commands`
   class, add the command name to the `HandleCommand` dispatch, and add the
   command-name `if/else` branch in `UnrealMCPBridge::ExecuteCommand`.
2. **Python side:** add a `@mcp.tool()`-decorated function to the matching
   `server/tools/<category>_tools.py`. If it's a new category, also register
   it in `server/unreal_mcp_server.py`.
3. **Rebuild** the plugin via UBT and restart the MCP client.
