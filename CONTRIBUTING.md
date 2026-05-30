# Contributing to unreal-mcp (RonildoBraga fork)

Style guide + reference notes for anyone (or any AI assistant) adding new tools
to the plugin. This file replaces the IDE-specific rule files (`.cursor/`,
`.windsurfrules`, `.clinerules`, `.github/copilot-instructions.md`) that the
upstream chongdashu repo carried for Cursor/Windsurf/Cline/Copilot users. The
content was duplicated four ways across those files, lightly customized per
IDE — consolidated here as a single source of truth that any assistant can
read.

## Guidelines for Python tools (`@mcp.tool()` functions)

Apply to anything in `Python/tools/*.py` decorated with `@mcp.tool()`.

- **Parameter types must be FastMCP-friendly.** Avoid `Any`, `object`,
  `Optional[T]`, `Union[T]` — they don't translate into clean JSON schemas
  for the MCP client.
- **For parameters with default values**, prefer `x: T = None` and handle the
  default inside the function body rather than `x: T | None = None`.
  FastMCP's schema generator treats the latter as "nullable, no default."
- **Always include a docstring.** It becomes the tool description the LLM
  sees in its tool list. Document:
  - What the tool does in one sentence at the top.
  - Each parameter, with examples.
  - The return shape, ideally with a small JSON example.
  - Common failure modes (e.g. "returns `{success: False, note: ...}` if
    the asset is CDO-pinned").
- **Wrap C++ command calls in a `try/except`** and return `{"error": str(e)}`
  on exception. The MCP client surfaces this cleanly; uncaught exceptions
  often blow up the whole tool invocation chain.

See `Python/tools/asset_tools.py` for a worked example following this style.

## Guidelines for C++ command handlers

Apply to anything in `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/`.

- **One category = one `FUnrealMCP<Category>Commands` class** under
  `Commands/`. The `HandleCommand(CommandType, Params)` method dispatches to
  private `HandleXxx` methods per command name.
- **Wire the new category** into `UUnrealMCPBridge`:
  1. Include the header in `UnrealMCPBridge.h`.
  2. Add a `TSharedPtr<F...Commands>` member.
  3. Initialize in the constructor; reset in the destructor.
  4. Add the command-name `if/else` branch in `ExecuteCommand` (in
     `UnrealMCPBridge.cpp`).
- **Use `FUnrealMCPCommonUtils::CreateErrorResponse(...)`** for error returns.
  Successful returns are `TSharedPtr<FJsonObject>` shaped as documented in
  the corresponding Python tool's docstring.
- **Build target:** UE 5.7+. The fork applies UE-5.5→5.7 compatibility
  patches; check `CHANGELOG.md` v0.1.0 for the specifics (ANY_PACKAGE removal,
  BufferSize name collision).

## Unreal Engine reference notes

These ship with the plugin because they're load-bearing for anyone writing
tools that touch transforms, geometry, or world state.

### Coordinate system (Z-up, left-handed)

- **X-axis (red):** forward / backward. Positive is forward.
- **Y-axis (green):** left / right. Positive is right.
- **Z-axis (blue):** up / down. Positive is up.

### Units (UE defaults, SI-derived)

| Quantity | UE unit |
|---|---|
| Distance / length | centimeters (cm) |
| Mass | kilograms (kg) |
| Time | seconds (s), minutes (min) |
| Angles | degrees |
| Speed | meters per second (m/s) |
| Temperature | Celsius (°C) |
| Force | Newtons (N) |
| Torque | Newton-meters (N·m) |

When converting from real-world measurements: 1 meter = 100 cm. UE actor
transforms use cm everywhere.

## Build & test loop

The standard inner-loop for adding a new tool:

1. **Write the C++ handler** in a `Commands/` file. Add the `if` branch in
   `UnrealMCPBridge.cpp::ExecuteCommand`.
2. **Write the Python wrapper** in `Python/tools/<category>_tools.py`.
   Register the category in `Python/unreal_mcp_server.py` if it's new.
3. **Build the editor target** against a project that has the plugin
   installed (the sample `MCPGameProject` ships in this repo; the Lauder
   project uses a snapshot copy). UBT command:

   ```
   <UE_install>/Engine/Build/BatchFiles/Build.bat <ProjectName>Editor Win64 Development -Project="<path>.uproject" -WaitMutex -FromMsBuild
   ```

4. **Restart the MCP client** (Claude Desktop / Claude Code / etc.) to
   re-import the updated Python tool wrappers. The C++ side picks up new
   commands without an editor restart if you keep the editor running across
   the rebuild (UE 5.7's Live Coding handles plugin DLL reloads).
5. **Test the tool end-to-end** via the MCP client, then update
   `CHANGELOG.md` under `[Unreleased]`.

## File layout

```
unreal-mcp/
├── README.md                                   fork notice + tool catalog
├── CHANGELOG.md                                version history
├── CONTRIBUTING.md                             this file
├── LICENSE                                     MIT (preserves upstream attribution)
│
├── plugin/                                     ★ THE UE PLUGIN
│   ├── UnrealMCP.uplugin
│   └── Source/UnrealMCP/
│       ├── UnrealMCP.Build.cs
│       ├── Private/
│       │   ├── UnrealMCPBridge.cpp             dispatch + TCP server
│       │   ├── UnrealMCPModule.cpp
│       │   ├── MCPServerRunnable.cpp
│       │   └── Commands/                       handler implementations
│       └── Public/
│           ├── UnrealMCPBridge.h
│           ├── UnrealMCPModule.h
│           ├── MCPServerRunnable.h
│           └── Commands/                       handler headers
│
├── server/                                     ★ THE PYTHON MCP SERVER
│   ├── pyproject.toml
│   ├── unreal_mcp_server.py                    FastMCP server, registers tools
│   └── tools/
│       ├── asset_tools.py                      Sprint 1 — asset registry
│       ├── editor_tools.py                     actor + viewport + screenshot + CVars
│       ├── level_tools.py                      Sprint 1 — level lifecycle
│       ├── blueprint_tools.py                  BP class authoring
│       ├── node_tools.py                       BP graph node manipulation
│       ├── project_tools.py                    project settings (input maps etc.)
│       └── umg_tools.py                        widget editing
│
├── sample/                                     ★ minimal dev/test UE 5.7 project
│   ├── UnrealMCPSample.uproject
│   ├── Config/
│   ├── Source/UnrealMCPSample/                 minimal BP-only-style game module
│   └── Plugins/UnrealMCP/                      junction → ../../plugin/ (gitignored)
│
├── docs/
│   ├── architecture.md                         system diagram, how the halves talk
│   ├── installing.md                           user + contributor install paths
│   └── tools/                                  per-category tool reference (stub at v0.3)
│
├── tests/                                      integration tests (stub at v0.3)
├── examples/
│   ├── mcp-client-config.json                  example MCP client registration
│   └── kit_inventory.md                        worked example: asset inventory workflow
└── scripts/
    └── setup-dev-junction.ps1                  creates sample/Plugins/UnrealMCP junction
```
