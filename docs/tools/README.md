# Tool Reference

Per-category reference for the MCP tools exposed by this plugin. Each
category corresponds to one `FUnrealMCP<Category>Commands` C++ class in
`plugin/Source/UnrealMCP/Private/Commands/` and one matching Python wrapper
module in `server/tools/`.

> **Status (v0.8.1).** The canonical, always-up-to-date catalog is the
> `@unreal_tool` and `@mcp.tool()` decorated functions in
> `server/tools/<category>_tools.py`. Each function's docstring is what an
> MCP client sees during `tools/list` — that's the source of truth.
> This README's job is to map "which category owns which capabilities" at
> the architectural level. For the per-release narrative (what shipped
> when, with what design intent), see `CHANGELOG.md`.

## Categories shipped (v0.8.1)

| Category | C++ class | Python module | Tools | Notes |
|---|---|---|---|---|
| **Editor / actors** | `FUnrealMCPEditorCommands` | `editor_tools.py` | 48 | Actors + selection + viewport + screenshots + PIE + reflection. The biggest surface. Spawn (any `AActor` subclass — generic UClass lookup via `Reflection/ClassLookup`), batched spawn / delete, paged `find_actors`, selection write side (`set_selected_actors`, `clear_selection`, `focus_selected_actors`), `set_actor_transform` / `get_actor_transform`, dotted-path `set_actor_property` / `get_actor_property`, generalized `set_object_property` / `get_object_property` over ANY UObject, viewport camera + mode + show flags, inline-image screenshots, console, CVars, log tail, async-compile status + wait, modal dismiss, PIE control + screenshots, `recompile_live`. |
| **Assets** | `FUnrealMCPAssetCommands` | `asset_tools.py` | 16 | Registry queries (list, info, dependencies, references, find by class), mutations (move, delete, rename, duplicate), cross-project migration (`migrate_assets` + `finalize_migration`), `import_asset` (FBX/PNG/WAV/...), Content Browser cooperation (`focus_in_browser`, `navigate_to_folder`, `open_in_editor`), `static_mesh_get_info` (bounds + slots + LODs). |
| **Blueprint nodes** | `FUnrealMCPBlueprintNodeCommands` | `node_tools.py` | 8 | Event nodes, function calls, pin connections, variable get/set, input action nodes, self-component references. |
| **Blueprints** | `FUnrealMCPBlueprintCommands` | `blueprint_tools.py` | 7 | Class creation, component templates, property writes (CDO + component), pawn/static mesh/physics property bundles, compile, variable add. |
| **UMG widgets** | `FUnrealMCPUMGCommands` | `umg_tools.py` | 6 | Widget BPs, text + button add, event binding, text-binding hookup, viewport mount. |
| **Materials** | `FUnrealMCPMaterialCommands` | `material_tools.py` | 5 | Create material instances, tune parameters, query parent + uses, list instances of a parent. |
| **Outliner** | `FUnrealMCPOutlinerCommands` | `outliner_tools.py` | 5 | Folders (get list, create pending), `move_actor_to_folder` + batched form, `get_actors_in_folder`. |
| **Levels** | `FUnrealMCPLevelCommands` | `level_tools.py` | 3 | Get current, open, save current. |
| **Project** | `FUnrealMCPProjectCommands` | `project_tools.py` | 4 | Legacy input action mapping creation, INI editing (`get_ini` / `set_ini`), the v0.8.1 `execute_python` escape hatch. |

Total: **102 tools at v0.8.1.**

(Some Python wrappers are composites that dispatch to other commands and
don't add a new wire command — `get_world_settings` / `set_world_settings`
forward to `get_object_property` / `set_object_property` with target pinned
to `WorldSettings`; `get_component_property` / `get_static_mesh_material`
forward to `get_object_property` with a composed dotted path. These appear
in the Python tool surface but don't count toward the C++ command-name
total. The canonical `smoke_dispatch.py` is safe/read-only by default and
requires `--allow-mutating` for full empty-param dispatch.)

## Cross-category power tools

A few tools deserve highlighting because they cut across the category
lines, have outsized leverage, or anchor common workflows:

- **`set_object_property(target, path, value)` + `get_object_property(target, path)`**
  — Editor. The v0.8.0 §5 leverage move. Same dotted-path traversal as
  `get/set_actor_property` (object hops, struct hops, array indices) but
  `target` resolves to ANY UObject:
  - Actor display label or internal name in the current editor world
  - `/Game/`-prefixed asset path (loaded as the asset)
  - `/Script/`-prefixed engine class path or class default object
  Set side broadcasts `PostEditChangeProperty` so the editor refreshes
  Details panel + viewport + render state. Lets the agent reach into asset
  defaults, CDO values, world settings, and other non-actor UObjects with
  the same vocabulary it uses for actors. `get_world_settings` /
  `set_world_settings` are pinned-target convenience shims for the
  `AWorldSettings` actor specifically.

- **`set_actor_property(name, "Path.To.Property", value)` /
  `get_actor_property(name, "Path.To.Property")`** — Editor. The actor-
  specific predecessors of `get/set_object_property`. Retained because
  they're the workhorses of property mutation on actors in the loaded
  level; behavior is identical to `get/set_object_property` when target
  is an actor name.

- **`find_actors(pattern?, class_filter?, folder?, limit=200, offset=0)`**
  — Editor. Paged enumeration with class + name + folder filters. Replaces
  `get_actors_in_level` for any caller that doesn't want the full scene
  dump (~744 KB on a 3000-actor scene). Per-actor payload matches the
  `get_selected_actors` shape so selection round-trips just work.

- **`spawn_actor(name, type, location?, rotation?, scale?)` and
  `spawn_actor_batch(actors=[...])`** — Editor. Generic UClass lookup
  (via `Reflection/ClassLookup`) accepts any `AActor` subclass: short
  name (`"SkyAtmosphere"` → `/Script/Engine.SkyAtmosphere`), full path
  (`"/Script/Engine.PostProcessVolume"`), or anywhere-in-loaded-modules
  search. Batched form does one round-trip instead of N — important for
  dense scene placement (RomanCave-style 200+ spawns at once).

- **Selection write side** — Editor. `set_selected_actors(names)` mirrors
  the Outliner / viewport selection state to a list of names with two-pass
  display-label + internal-name lookup. `clear_selection()` deselects.
  `focus_selected_actors()` frames the selection in the viewport. Round-
  trips with the v0.7.12 `get_selected_actors`. `selected_count` is post-
  state authoritative (some special actors like `AWorldSettings` silently
  resist multi-select — they show up in the `rejected` array).

- **`take_screenshot(filename)` + `pie_screenshot(filename)`** — Editor.
  Return the PNG as inline FastMCP `Image` content, not just a path. The
  editor form forces a fresh viewport redraw before reading pixels (since
  UE's editor viewport is event-driven and otherwise serves stale buffers
  to `ReadPixels`); the PIE form captures the in-game viewport with no
  editor gizmos.

- **PIE control surface (v0.7.11)** — Editor. `start_pie` / `stop_pie` /
  `is_pie_active`, `pie_get_player` (reads `{location, rotation,
  velocity, movement_mode, is_falling, ...}`), `pie_set_player`
  (teleport), `pie_apply_movement` (fire-and-forget "hold W for N
  seconds"), `pie_screenshot`. The autonomous walkability oracle — can
  validate scene changes end-to-end without a human pressing Play.

- **`migrate_assets` + `finalize_migration` + `wait_for_async_compile`** —
  Asset + Editor. Two-phase cross-project asset copy with serialized
  reference fix-up + the v0.8.0 compile-queue drain that finally lets
  `finalize_migration` not race the mesh + shader compile. See the
  v0.7.3 and v0.8.0 entries in `CHANGELOG.md` for the failure modes
  these solve.

- **`recompile_live`** — Editor. Triggers Live Coding rebuild of the
  plugin DLL programmatically (same effect as Ctrl+Alt+F11). Closes the
  autonomous-build loop for handler-body edits. Caveat: file-scope static
  initializers don't re-run on patch reload, so adding NEW command names
  still needs full UBT + editor restart.

- **`execute_python(code, unsafe=False)`** — Project, v0.8.1. The
  documented escape hatch. Refuses by default; pass `unsafe=True` per
  call to actually run arbitrary Python in UE's embedded interpreter.
  Response shape `{success, error?, stdout, stderr}` is deliberately NOT
  the strict `{success, ...payload}` contract — this is the labeled
  exception, for when no typed wrapper exists for what you need.

## Where to look for per-tool details

The docstrings in `server/tools/*.py` are kept current with each ship
because that's literally what the LLM sees when it calls `tools/list`.
For most needs, grep there:

```sh
grep -rn '@unreal_tool\|@mcp.tool' server/tools/   # list every registered tool
grep -A 30 'def find_actors' server/tools/editor_tools.py   # see a docstring
```

Or, programmatically:

```bash
cd server
uv run python smoke_dispatch.py --list-only  # no socket traffic
uv run python smoke_dispatch.py              # safe/read-only editor smoke
uv run python smoke_dispatch.py --allow-mutating  # throwaway/checkpointed projects only
```
