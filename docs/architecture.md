# Architecture

unreal-mcp is a **two-process system** that bridges a Model Context Protocol
(MCP) client (Claude Desktop, Claude Code, Cursor, Codex, Windsurf, etc.) to
an Unreal Engine 5.7 editor. The two processes talk over a local TCP socket
in a strict JSON envelope.

```
┌──────────────────────────┐         ┌────────────────────────────┐
│  Python MCP Server       │         │  UE Editor (UnrealMCP)     │
│  (FastMCP framework)     │         │                            │
│                          │  TCP    │  MCPServerRunnable         │
│  unreal_mcp_server.py    │  55557  │      │                     │
│    │                     │ ◄─────► │      ↓                     │
│    │ @unreal_tool        │  JSON   │  UUnrealMCPBridge          │
│    │ decorator           │ {type,  │      │                     │
│    │ (server/_registry)  │  params}│      ↓                     │
│    │                     │         │  FMCPRegistry::Dispatch    │
│    ├─ tools/editor_tools │         │      │                     │
│    ├─ tools/asset_tools  │         │      ├─► editor.* handlers │
│    ├─ tools/blueprint_   │         │      ├─► assets.*          │
│    ├─ tools/node_tools   │         │      ├─► blueprint.*       │
│    ├─ tools/umg_tools    │         │      ├─► node.*            │
│    ├─ tools/level_tools  │         │      ├─► umg.*             │
│    ├─ tools/material_    │         │      ├─► level.*           │
│    ├─ tools/outliner_    │         │      ├─► material.*        │
│    └─ tools/project_tools│         │      ├─► outliner.*        │
│                          │         │      └─► project.*         │
└──────────────────────────┘         └────────────────────────────┘
       ▲                                       │
       │ MCP protocol (stdio)                  │ UE 5.7 C++ APIs +
       │                                       │ IPythonScriptPlugin
       │                                       ▼
┌──────┴───────────────────┐         (AssetRegistry,
│  MCP client              │          EditorActorSubsystem,
│  (Claude / Cursor / ...) │          ULevelEditorSubsystem,
└──────────────────────────┘          BlueprintEditorUtils, ...)
```

Wire format (since v0.8.0): every response is the strict shape

```json
{"success": true,  "...payload fields..."}
{"success": false, "error": "..."}
```

No envelope. No `status`/`result` wrapping. The Python decorator and the C++
response builder both enforce this — see `server/_registry.py` for the
Python side and `plugin/Source/UnrealMCP/Public/MCPResponse.h` for the C++
side.

## The two halves

### Plugin (`plugin/`)

C++ UE plugin that runs **inside the editor process**. Listens on TCP port
55557 (constant `MCP_SERVER_PORT` in `UnrealMCPBridge.cpp`). Receives JSON
commands of the shape `{type: "<command_name>", params: {...}}` and dispatches
each command through `FMCPRegistry::Dispatch` (the singleton registry built
in v0.8.0 to replace the previous if/else chain). The registry is populated
at DLL load by a file-scope `FAutoRegistrar` in `MCPRegistrations.cpp` — that
runs both on initial editor start AND after every Live Coding patch reload,
so existing commands keep working after handler-body changes.

Adding a new command name still requires a full UBT rebuild while the
editor is closed — file-scope static initializers don't re-run for the
patched DLL when Live Coding applies a patch in-place. Use `recompile_live`
for modifying existing handler bodies (those land via Live Coding cleanly).

The plugin is **editor-only** — none of it ships in a packaged game. Module
type `Editor` in `UnrealMCP.uplugin`.

Three foundation modules under `plugin/Source/UnrealMCP/`:

- **`MCPRegistry` / `MCPParams` / `MCPResponse`** — the dispatch + typed
  param access + strict-response-builder primitives (lifted Day 1 of v0.8.0).
- **`Reflection/PropertyWalker`** — dotted-path UPROPERTY traversal across
  `FObjectProperty` (component hops), `FStructProperty` (struct descents),
  and `FArrayProperty` (numeric index segments). Handles JSON conversion for
  every primitive + Vector/Rotator/Vector4/LinearColor/Color/UObject* leaves.
  Broadcasts `PostEditChangeProperty` on the owning UObject after writes so
  the editor refreshes the Details panel + viewport without a restart.
- **`Reflection/ClassLookup`** + **`Reflection/ObjectLookup`** — name → UClass
  and target-string → UObject resolution. The `ObjectLookup` lift (v0.8.0 §5)
  is what makes `get/set_object_property` work on actor labels, internal
  names, `/Game/`-prefixed asset paths, `/Script/`-prefixed engine class
  paths, and class default objects with the same dispatch.

Per-category command handlers live in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCP<Category>Commands.cpp`.
Each `FUnrealMCP<Category>Commands` class exposes a `HandleCommand` method
that the registry routes individual command names to. (Migrating these to
free-function self-registration at the handler definition site is queued
as a v0.8.x cleanup task — the dispatch infrastructure is there, the
mechanical migration of the ~110 handlers is the deferred step.)

### Server (`server/`)

Python MCP server using the `FastMCP` framework. Exposes one Python function
per MCP tool, decorated with `@unreal_tool` (from `server/_registry.py`).
The decorator absorbs all per-wrapper boilerplate: it introspects the
function signature, drops `OMIT` / `None` defaults from the params dict,
opens a fresh TCP socket to the plugin, sends the JSON, reads the response,
and surfaces it back to the MCP client.

Function bodies are usually just docstrings — the docstring IS the agent-
facing surface (every MCP client sees it via `tools/list`). The wire command
name defaults to the function name unless overridden via
`@unreal_tool(mcp, command="...")`.

A handful of tools (`take_screenshot`, `pie_screenshot`, `execute_python`,
the `get/set_world_settings` convenience pair) are raw `@mcp.tool()`
wrappers rather than `@unreal_tool` because their response shape doesn't
match the strict `{success, ...payload}` contract (screenshot returns a
FastMCP `Image` content block; `execute_python` returns
`{success, stdout, stderr}`; world_settings forwards to other commands
with the target pre-pinned). They use `dispatch_unreal_command` from
`server/_registry.py` directly — the same primitive `@unreal_tool` wraps.

The server is launched by the MCP client (Claude Desktop, Cursor, etc.) per
the client's MCP config — see `examples/mcp-client-config.json` for an
example.

## Why TCP, not (e.g.) shared memory or stdio

- **Process boundary.** The plugin runs inside the UE editor process; the
  Python server runs as a separate child of the MCP client. They have no
  shared address space.
- **Lifecycle independence.** The editor can restart without restarting the
  Python server, and vice versa. The TCP server is set up to allow rapid
  reconnect (`SetReuseAddr(true)`).
- **Cross-language.** C++ ↔ Python over JSON is the lowest-common-denominator
  ABI that's reliable on Windows + macOS + Linux.

## Coverage canary

`server/smoke_dispatch.py` pings every registered command with empty params
and asserts the response is NOT the bridge's "Unknown command: …" fallback.
This is the §8 Q5 commitment from the v0.8.0 architecture plan — it catches
"did I forget to wire a new command name" before the user does. Run after
any plugin rebuild + editor reopen:

```bash
cd server
./.venv/Scripts/python smoke_dispatch.py   # exit 0 = every command dispatched
```

Timeouts (Windows-specific TCP RST-after-FIN race on small-payload fast-
error responses) are classified separately from "Unknown command" — the
editor log confirms those handlers ran and wrote responses, the client
just couldn't read them before close.

## Adding a new tool

Both sides need a change. See `CONTRIBUTING.md` for the style guide. The
short version:

1. **C++ side:**
   - Add a handler method to the appropriate `FUnrealMCP<Category>Commands`
     class (`HandleXxx`).
   - Wire it in the class's `HandleCommand` dispatch (the per-class if/else
     — yes, the cleanup migration to free-function self-registration is
     pending).
   - Add the command name to `plugin/Source/UnrealMCP/Private/MCPRegistrations.cpp`
     in the appropriate `RegBatch<FUnrealMCP<Category>Commands>({...})` call.
2. **Python side:**
   - Add a `@unreal_tool(mcp)` function to the matching
     `server/tools/<category>_tools.py`. Function body can be just the
     docstring — the decorator handles dispatch.
   - For non-standard response shapes (image content, multi-call
     composites), use raw `@mcp.tool()` and call `dispatch_unreal_command`
     directly.
3. **Rebuild:** close the editor → run UBT
   (`Engine/Build/BatchFiles/Build.bat LauderEditor Win64 Development`)
   → reopen the editor. The new command name appears in `tools/list` on
   next connect. For changes to existing handler bodies (no new command
   names), `recompile_live` MCP call triggers Live Coding instead.
4. **Validate:** `smoke_dispatch.py` confirms the new command name
   dispatches; a targeted integration smoke (see `_smoke_*.py` patterns
   in past Day 3-4 commits) confirms the actual behavior.
