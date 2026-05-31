# Tool Reference

Per-category reference for the MCP tools exposed by this plugin. Each
category corresponds to one `FUnrealMCP<Category>Commands` C++ class in
`plugin/Source/UnrealMCP/Private/Commands/` and one matching Python wrapper
in `server/tools/`.

> **Status (v0.7.11).** The canonical, always-up-to-date catalog is the
> `@mcp.tool()` decorated functions in `server/tools/<category>_tools.py`.
> Each function's docstring is what an MCP client sees during
> `tools/list` — that's the source of truth, and these per-category
> markdown stubs (`assets.md`, etc.) intentionally don't try to mirror
> it. This README's job is to map "which category owns which
> capabilities" at the architectural level.

## Categories shipped (v0.7.11)

| Category | C++ class | Python module | Tools | Notes |
|---|---|---|---|---|
| **Actor / editor** | `FUnrealMCPEditorCommands` | `editor_tools.py` | 30 | Spawn (any `AActor` subclass — generic UClass lookup), `spawn_static_mesh_actor`, `set_static_mesh_material`, delete, transform, `get_actor_property` / `set_actor_property` with dotted-path traversal (objects + structs + array indices), viewport camera + mode, screenshots (inline image), console, CVars, log tail, compile-status, **PIE control (v0.7.11)** |
| **Asset** | `FUnrealMCPAssetCommands` | `asset_tools.py` | 12 | List, info, dependencies, references, move/delete/rename/duplicate, `migrate_assets` + `finalize_migration` + `import_asset` |
| **Blueprint** | `FUnrealMCPBlueprintCommands` | `blueprint_tools.py` | 8 | Class creation, components, compile, variables |
| **Blueprint nodes** | `FUnrealMCPBlueprintNodeCommands` | `node_tools.py` | 8 | Event nodes, function calls, connections, variable get/set, input action nodes |
| **UMG widgets** | `FUnrealMCPUMGCommands` | `umg_tools.py` | 6 | Widget BPs, text/button add, event binding, viewport |
| **Material** | `FUnrealMCPMaterialCommands` | `material_tools.py` | 5 | Create material instances, tune parameters, query parent + uses, list instances |
| **Level** | `FUnrealMCPLevelCommands` | `level_tools.py` | 4 | Current, open, save, save all dirty |
| **Outliner** | `FUnrealMCPOutlinerCommands` | `outliner_tools.py` | 4 | Get/create folders, move actors to folders, list actors by folder |
| **Project** | `FUnrealMCPProjectCommands` | `project_tools.py` | 1 | Input action mappings |

Total: **78 tools at v0.7.11.**

## Cross-category power tools

A few tools deserve highlighting because they cut across the category
lines or have outsized impact on common workflows:

- **`set_actor_property(name, "Path.To.Property", value)`** — Editor.
  Walks dotted paths through `FObjectProperty` (component hops),
  `FStructProperty` (`Settings.AutoExposureBias` style), and
  `FArrayProperty` (numeric index, e.g. `OverrideMaterials.0`) hops.
  Handles JSON values for `FVector`, `FRotator`, `FVector4`, `FLinearColor`,
  `FColor`, and `/Game/`-prefixed asset paths for `FObjectProperty`
  leaves. Broadcasts `PostEditChangeProperty` after each write so the
  renderer actually picks up the change (the v0.7.9 fix). Covers what
  would otherwise need a dozen typed setters.
- **`get_actor_property(name, "Path.To.Property")`** — Editor. Read
  counterpart to the above. Returns numeric leaves as numbers, strings
  as strings, structs as arrays, UObject refs as path strings, TArrays
  as `{kind, length, inner}`. Lets you inspect mesh refs, material slot
  assignments, light parameters etc. without restarting the editor.
- **`spawn_actor(name, type, location, rotation)`** — Editor. Generic
  UClass lookup (v0.7.10) accepts any `AActor` subclass: short name
  (`"SkyAtmosphere"` → `/Script/Engine.SkyAtmosphere`) or full path
  (`"/Script/Engine.PostProcessVolume"`). Unlocks every engine actor
  class without per-type code.
- **`set_static_mesh_material(name, material_path, slot)`** — Editor.
  Ergonomic path for the common "Megascans migration lost the parent
  material, swap slot 0 for a known-good MI" workflow.
- **`take_screenshot(filename)`** — Editor. Returns the PNG as inline
  image content (FastMCP `Image`), not just a path. Forces a fresh
  viewport redraw before reading pixels (since UE's editor viewport is
  event-driven and otherwise serves stale buffers to ReadPixels).
- **PIE control surface (v0.7.11)** — Editor. `start_pie` / `stop_pie`
  / `is_pie_active`, `pie_get_player` (reads `{location, rotation,
  velocity, movement_mode, is_falling, ...}`), `pie_set_player`
  (teleport), `pie_apply_movement` (fire-and-forget "hold W for N
  seconds"), `pie_screenshot` (in-game viewport, no editor gizmos).
  The autonomous walkability oracle — can validate scene changes
  end-to-end without a human in the loop.
- **`migrate_assets` + `finalize_migration`** — Asset. Two-phase
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
