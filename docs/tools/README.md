# Tool Reference

Per-category reference for the MCP tools exposed by this plugin. Each
category corresponds to one `FUnrealMCP<Category>Commands` C++ class in
`plugin/Source/UnrealMCP/Private/Commands/` and one matching Python wrapper
in `server/tools/`.

> **Status (v0.3.0).** Tool docs in this directory are stubs pending
> regeneration. The previous upstream docs were written against the v0.1
> surface and predate the v0.2/v0.3 additions, so they were removed to
> avoid misleading readers. Each `<category>.md` will be regenerated as
> we go — the source of truth in the meantime is the docstring on each
> Python tool wrapper in `server/tools/`, which is what an MCP client
> sees anyway.

## Categories shipped (v0.3.0)

| Category | C++ class | Python module | Tools | Notes |
|---|---|---|---|---|
| **Actor / editor** | `FUnrealMCPEditorCommands` | `server/tools/editor_tools.py` | 12 | Actors, viewport, screenshot, console, CVars |
| **Blueprint** | `FUnrealMCPBlueprintCommands` | `server/tools/blueprint_tools.py` | 8 | Class creation, components, compile |
| **Blueprint nodes** | `FUnrealMCPBlueprintNodeCommands` | `server/tools/node_tools.py` | 9 | Graph node manipulation, connections, variables |
| **Project** | `FUnrealMCPProjectCommands` | `server/tools/project_tools.py` | 1 | Input mappings |
| **UMG widgets** | `FUnrealMCPUMGCommands` | `server/tools/umg_tools.py` | 11+ | Widget BPs, layout, bindings |
| **Asset** (Sprint 1) | `FUnrealMCPAssetCommands` | `server/tools/asset_tools.py` | 9 | List, info, dependencies, references, mutations |
| **Level** (Sprint 1) | `FUnrealMCPLevelCommands` | `server/tools/level_tools.py` | 4 | Current, open, save, save all dirty |

Total: ~54 tools at v0.3.0.

## Tool catalog (in-progress)

These will be filled in per category over Sprint 2+:

- `assets.md` — TBD
- `editor.md` — TBD
- `level.md` — TBD
- `blueprint.md` — TBD
- `umg.md` — TBD
- `project.md` — TBD
- `nodes.md` — TBD

Until then, refer to the docstrings in `server/tools/*.py` for accurate
per-tool documentation.
