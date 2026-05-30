# Tool Reference

Per-category reference for the MCP tools exposed by this plugin. Each
category corresponds to one `FUnrealMCP<Category>Commands` C++ class in
`plugin/Source/UnrealMCP/Private/Commands/` and one matching Python wrapper
in `server/tools/`.

> **Status (v0.7.7).** The canonical, always-up-to-date catalog is the
> `@mcp.tool()` decorated functions in `server/tools/<category>_tools.py`.
> Each function's docstring is what an MCP client sees during
> `tools/list` — that's the source of truth, and these per-category
> markdown stubs (`assets.md`, etc.) intentionally don't try to mirror
> it. This README's job is to map "which category owns which
> capabilities" at the architectural level.

## Categories shipped (v0.7.7)

| Category | C++ class | Python module | Tools | Notes |
|---|---|---|---|---|
| **Actor / editor** | `FUnrealMCPEditorCommands` | `editor_tools.py` | 21 | Spawn (generic + StaticMeshActor), delete, transform, properties with dotted-path traversal, viewport camera + mode, screenshots (inline image), console, CVars, log tail, compile-status |
| **Blueprint** | `FUnrealMCPBlueprintCommands` | `blueprint_tools.py` | 8 | Class creation, components, compile, variables |
| **Blueprint nodes** | `FUnrealMCPBlueprintNodeCommands` | `node_tools.py` | 8 | Event nodes, function calls, connections, variable get/set, input action nodes |
| **Project** | `FUnrealMCPProjectCommands` | `project_tools.py` | 1 | Input action mappings |
| **UMG widgets** | `FUnrealMCPUMGCommands` | `umg_tools.py` | 6 | Widget BPs, text/button add, event binding, viewport |
| **Asset** | `FUnrealMCPAssetCommands` | `asset_tools.py` | 12 | List, info, dependencies, references, move/delete/rename/duplicate, `migrate_assets` + `finalize_migration` + `import_asset` |
| **Level** | `FUnrealMCPLevelCommands` | `level_tools.py` | 4 | Current, open, save, save all dirty |
| **Material** | `FUnrealMCPMaterialCommands` | `material_tools.py` | 5 | Create material instances, tune parameters, query parent + uses |
| **Outliner** | `FUnrealMCPOutlinerCommands` | `outliner_tools.py` | 4 | Get/create folders, move actors to folders, list actors by folder |

Total: ~69 tools at v0.7.7.

## Cross-category capabilities

A few power-tools deserve highlighting because they cut across the
category lines:

- **`set_actor_property(name, "ComponentName.PropertyName", value)`** —
  in the Editor category. Walks dotted paths through both `FObjectProperty`
  (component) and `FStructProperty` (`Settings.AutoExposureBias` style)
  hops. Handles JSON values for `FVector`, `FRotator`, `FLinearColor`,
  `FColor`, and `/Game/`-prefixed asset paths for `FObjectProperty` leaves.
  This single tool covers what would otherwise require a dozen typed
  setters.
- **`take_screenshot(filename)`** — returns the PNG as inline image
  content (FastMCP `Image`), not just a path. Forces a fresh viewport
  redraw before reading pixels (since UE's editor viewport is
  event-driven and otherwise serves stale buffers to ReadPixels).
- **`migrate_assets` + `finalize_migration`** — Asset category. Two-phase
  cross-project asset copy with serialized reference fix-up. See the
  v0.7.3 entry in CHANGELOG.md for the failure mode this solves.

## Where to look for per-tool details

The docstrings in `server/tools/*.py` are kept current with each ship
because that's literally what the LLM sees when it calls `tools/list`.
For most needs, grep there:

```sh
grep -rn '@mcp.tool' server/tools/   # list every registered tool
grep -rn '"""' server/tools/asset_tools.py | head -30   # see docstrings
```
